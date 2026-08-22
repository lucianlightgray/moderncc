#if (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))

#include "mccast.h"
#include "mccrir.h"
#include "mccstats.h"

#ifndef MCC_TRACE
#include "mcclog.h"
unsigned char mcc_log_verbose = 0;
#endif

#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mcchost.h"

#include "mccforecast.h"

#include "algorithms/lzss.h"
#include "algorithms/lzw.h"
#include "algorithms/rle.h"
#ifdef MCC_EMBED_JIT
#include "algorithms/jit.h"
void mccjit_embed_stash_leaf(AstArena *ast, Sym *sym);
void mccjit_embed_note(const char *name, AstArena *ast, Sym *sym, uint64_t warm_gates);
int mcc_jit_submit_ast(Sym *sym, AstArena *ast, uint64_t gate_mask, int flags);
#endif
#include "mcccombo.h"
#include "mccsurro.h"
#include "mccmagic.h"
#include "mccthread.h"

#ifndef AST_ASSERT
#include <assert.h>
#define AST_ASSERT(x) assert(x)
#endif

#ifndef VT_BITFIELD
#define VT_BITFIELD 0x0100
#else
typedef char ast_vt_bitfield_check[VT_BITFIELD == 0x0100 ? 1 : -1];
#endif

#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef realloc
#undef free

#define AST_FB_STORE_VALUE_LIVE 2u

#define AST_FB_CALL_STOREVAL_ARG 4u

#define AST_FB_LANDOR_INVERT 8u

#define AST_FB_STOREVAL_CONST_LEFT 16u

#define AST_FB_CONVERT_GV 32u

#define AST_FB_CALL_NORETURN 64u

#define AST_FB_CALL_STOREVAL_STORE 128u

#define AST_FB_STORE_CHAIN_REUSE 256u

#define AST_FB_STORE_CHAIN_MEMBER 512u

#define AST_FB_STORE_CHAIN_SKIP 1024u

#define AST_FB_STORE_LIVE_ROT 2048u
#define AST_FB_LANDOR_MATERIAL 4096u

#define AST_FB_STORE_BF_GV 8192u

#define AST_FB_STMT_DISCARD 16384u

#define AST_FB_FOR_INCR_LIVE 32768u

#define AST_FB_STORE_CMP_GV 65536u

#define AST_FB_STORE_ADDR_LATE 131072u

#define AST_FB_CMP_INVERT_LATE 262144u

#define AST_FB_CONVERT_FCS 524288u

#define AST_FB_BINARY_RHS_GV 1048576u
#define AST_FB_NOCODE 2097152u
#define AST_FB_LOAD_LVAL 4194304u

#define AST_FB_STORE_CHAIN_LIVE 8388608u

/* T-win-50028: a captured MEMBER node whose access is reversed-scalar_storage_order.
 * Carried as a dedicated fbits bit (NOT the raw VT_REVSO r-flag 0x10000, which aliases
 * AST_FB_STORE_CMP_GV and corrupts bit-field stores under the -O1 optimizer); replay
 * translates it back to VT_REVSO on the SValue.  See mccgen.c:13958 (parser origin). */
#define AST_FB_MEMBER_REVSO 16777216u
/* T-lin-10457 (3): a blind-retype-narrowed COMPUTATION node whose int arithmetic
 * must be overflow-guarded — the reemit emits `seto; or -> mccjit_boundary_hit`
 * after the op, and the KGC stub deopts to baseline on any hit. Set only under
 * MCC_JIT_BLIND_RETYPE (default off) -> o0-neutral. */
#define AST_FB_JIT_GUARD 33554432u

struct AstArena {
	uint16_t *kind;
	AstLocal *parent;
	AstLocal *first_child;
	AstLocal *last_child;
	AstLocal *next_sib;
	uint32_t *nchild;

	int32_t *op;
	int32_t *type_t;
	uint8_t *type_bp;
	uint8_t *type_bs;
	uint64_t *type_ref;
	uint64_t *ival;
	uint64_t *fbits;
	uint64_t *sym;

	uint64_t *wide_hi;
	uint16_t *wide_r2;
	uint8_t *st_have;
	int32_t *st_t;
	uint64_t *st_ref;
	uint8_t *st_bp;
	uint8_t *st_bs;

	AstLocal count;
	AstLocal cap;
	uint64_t epoch;
};

static void ast_grow(AstArena *a, AstLocal need) { MCC_TRACE("enter\n");
	if (need <= a->cap)
		{ MCC_TRACE("br\n"); return; }
	AstLocal ocap = a->cap;
	AstLocal ncap = a->cap ? a->cap * 2 : 64;
	while (ncap < need)
		{ MCC_TRACE("br\n"); ncap *= 2; }
#define AST_REGROW(field) \
	a->field = realloc(a->field, ncap * sizeof *a->field)
	AST_REGROW(kind);
	AST_REGROW(parent);
	AST_REGROW(first_child);
	AST_REGROW(last_child);
	AST_REGROW(next_sib);
	AST_REGROW(nchild);
	AST_REGROW(op);
	AST_REGROW(type_t);
	AST_REGROW(type_bp);
	AST_REGROW(type_bs);
	AST_REGROW(type_ref);
	AST_REGROW(ival);
	AST_REGROW(fbits);
	AST_REGROW(sym);
#undef AST_REGROW
	if (a->wide_hi) { MCC_TRACE("br\n");
		a->wide_hi = realloc(a->wide_hi, ncap * sizeof *a->wide_hi);
		a->wide_r2 = realloc(a->wide_r2, ncap * sizeof *a->wide_r2);
		for (AstLocal i = ocap; i < ncap; i++) { MCC_TRACE("br\n");
			a->wide_hi[i] = 0;
			a->wide_r2[i] = AST_R2_NONE;
		}
	}
	if (a->st_have) { MCC_TRACE("br\n");
		a->st_have = realloc(a->st_have, ncap * sizeof *a->st_have);
		a->st_t = realloc(a->st_t, ncap * sizeof *a->st_t);
		a->st_ref = realloc(a->st_ref, ncap * sizeof *a->st_ref);
		a->st_bp = realloc(a->st_bp, ncap * sizeof *a->st_bp);
		a->st_bs = realloc(a->st_bs, ncap * sizeof *a->st_bs);
		for (AstLocal i = ocap; i < ncap; i++) { MCC_TRACE("br\n");
			a->st_have[i] = 0;
			a->st_t[i] = 0;
			a->st_ref[i] = 0;
			a->st_bp[i] = 0;
			a->st_bs[i] = 0;
		}
	}
	a->cap = ncap;
}

static void ast_wide_open(AstArena *a) { MCC_TRACE("enter\n");
	if (a->wide_hi)
		{ MCC_TRACE("br\n"); return; }
	AstLocal cap = a->cap ? a->cap : 1;
	a->wide_hi = calloc(cap, sizeof *a->wide_hi);
	a->wide_r2 = malloc(cap * sizeof *a->wide_r2);
	if (!a->wide_hi || !a->wide_r2) { MCC_TRACE("br\n");
		free(a->wide_hi);
		free(a->wide_r2);
		a->wide_hi = NULL;
		a->wide_r2 = NULL;
		return;
	}
	for (AstLocal i = 0; i < cap; i++)
		{ MCC_TRACE("br\n"); a->wide_r2[i] = AST_R2_NONE; }
}

void ast_set_wide(AstArena *a, AstLocal n, uint64_t hi, unsigned r2) { MCC_TRACE("enter\n");
	if (!a->wide_hi) { MCC_TRACE("br\n");
		if (!hi && r2 == AST_R2_NONE)
			{ MCC_TRACE("br\n"); return; }
		ast_wide_open(a);
		if (!a->wide_hi)
			{ MCC_TRACE("br\n"); return; }
	}
	a->epoch++;
	a->wide_hi[n] = hi;
	a->wide_r2[n] = (uint16_t)r2;
}

uint64_t ast_wide_hi(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->wide_hi ? a->wide_hi[n] : 0;
}

unsigned ast_wide_r2(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->wide_r2 ? a->wide_r2[n] : AST_R2_NONE;
}

#if defined(MCC_INTERNAL)
static void ast_du_invalidate(const AstArena *a);
static void ast_memo_invalidate(const AstArena *a);
static void ast_hash_invalidate(const AstArena *a);
static void ast_vlat_invalidate(const AstArena *a);
static void ast_loopnest_invalidate(const AstArena *a);
static void ast_divmagic_invalidate(const AstArena *a);
#endif

AstArena *ast_arena_new(void) { MCC_TRACE("enter\n");
	AstArena *a = calloc(1, sizeof *a);
	return a;
}

void ast_arena_reset(AstArena *a) { MCC_TRACE("enter\n");
	a->count = 0;
	a->epoch++;
}

void ast_teardown(void);

void ast_arena_free(AstArena *a) { MCC_TRACE("enter\n");
	if (!a)
		{ MCC_TRACE("br\n"); return; }
#if defined(MCC_INTERNAL)
	ast_du_invalidate(a);
	ast_memo_invalidate(a);
	ast_hash_invalidate(a);
	ast_vlat_invalidate(a);
	ast_loopnest_invalidate(a);
	ast_divmagic_invalidate(a);
#endif
	free(a->kind);
	free(a->parent);
	free(a->first_child);
	free(a->last_child);
	free(a->next_sib);
	free(a->nchild);
	free(a->op);
	free(a->type_t);
	free(a->type_bp);
	free(a->type_bs);
	free(a->type_ref);
	free(a->ival);
	free(a->fbits);
	free(a->sym);
	free(a->wide_hi);
	free(a->wide_r2);
	free(a->st_have);
	free(a->st_t);
	free(a->st_ref);
	free(a->st_bp);
	free(a->st_bs);
	free(a);
}

AstArena *ast_arena_clone(const AstArena *src) { MCC_TRACE("enter\n");
	AstArena *a = calloc(1, sizeof *a);
	if (!a)
		{ MCC_TRACE("br\n"); return NULL; }
	a->count = src->count;
	a->cap = src->count;
	if (src->count == 0)
		{ MCC_TRACE("br\n"); return a; }
#define AST_DUP(field)                                    \
	a->field = malloc(a->cap * sizeof *a->field);           \
	if (a->field)                                           \
		memcpy(a->field, src->field, src->count * sizeof *a->field)
	AST_DUP(kind);
	AST_DUP(parent);
	AST_DUP(first_child);
	AST_DUP(last_child);
	AST_DUP(next_sib);
	AST_DUP(nchild);
	AST_DUP(op);
	AST_DUP(type_t);
	AST_DUP(type_bp);
	AST_DUP(type_bs);
	AST_DUP(type_ref);
	AST_DUP(ival);
	AST_DUP(fbits);
	AST_DUP(sym);
	if (src->wide_hi) { MCC_TRACE("br\n");
		AST_DUP(wide_hi);
		AST_DUP(wide_r2);
	}
	if (src->st_have) { MCC_TRACE("br\n");
		AST_DUP(st_have);
		AST_DUP(st_t);
		AST_DUP(st_ref);
		AST_DUP(st_bp);
		AST_DUP(st_bs);
	}
#undef AST_DUP
	return a;
}

#if MCC_EMBED_JIT
static void ast_slice_walk(const AstArena *src, AstLocal node, AstLocal *map,
													 AstLocal *order, AstLocal *cnt) { MCC_TRACE("enter\n");
	AstLocal c;
	map[node] = *cnt;
	order[(*cnt)++] = node;
	for (c = src->first_child[node]; c != AST_NONE; c = src->next_sib[c])
		{ MCC_TRACE("br\n"); ast_slice_walk(src, c, map, order, cnt); }
}

AstArena *ast_slice_extract(const AstArena *src, AstLocal root) { MCC_TRACE("enter\n");
	AstLocal n, i, s, cnt = 0;
	AstLocal *map, *order;
	AstArena *a;
	if (!src || root >= src->count)
		{ MCC_TRACE("br\n"); return NULL; }
	n = src->count;
	map = malloc((size_t)n * sizeof *map);
	order = malloc((size_t)n * sizeof *order);
	if (!map || !order) { MCC_TRACE("br\n");
		free(map);
		free(order);
		return NULL;
	}
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); map[i] = AST_NONE; }
	ast_slice_walk(src, root, map, order, &cnt);
	a = calloc(1, sizeof *a);
	if (!a) { MCC_TRACE("br\n");
		free(map);
		free(order);
		return NULL;
	}
	a->count = cnt;
	a->cap = cnt;
#define AST_SLICE_ALLOC(field) a->field = malloc((size_t)cnt * sizeof *a->field)
	AST_SLICE_ALLOC(kind);
	AST_SLICE_ALLOC(parent);
	AST_SLICE_ALLOC(first_child);
	AST_SLICE_ALLOC(last_child);
	AST_SLICE_ALLOC(next_sib);
	AST_SLICE_ALLOC(nchild);
	AST_SLICE_ALLOC(op);
	AST_SLICE_ALLOC(type_t);
	AST_SLICE_ALLOC(type_bp);
	AST_SLICE_ALLOC(type_bs);
	AST_SLICE_ALLOC(type_ref);
	AST_SLICE_ALLOC(ival);
	AST_SLICE_ALLOC(fbits);
	AST_SLICE_ALLOC(sym);
	if (src->wide_hi) { MCC_TRACE("br\n");
		AST_SLICE_ALLOC(wide_hi);
		AST_SLICE_ALLOC(wide_r2);
	}
	if (src->st_have) { MCC_TRACE("br\n");
		AST_SLICE_ALLOC(st_have);
		AST_SLICE_ALLOC(st_t);
		AST_SLICE_ALLOC(st_ref);
		AST_SLICE_ALLOC(st_bp);
		AST_SLICE_ALLOC(st_bs);
	}
#undef AST_SLICE_ALLOC
	if (!a->kind || !a->parent || !a->first_child || !a->last_child ||
			!a->next_sib || !a->nchild || !a->op || !a->type_t || !a->type_bp ||
			!a->type_bs || !a->type_ref ||
			!a->ival || !a->fbits || !a->sym ||
			(src->wide_hi && (!a->wide_hi || !a->wide_r2))) { MCC_TRACE("br\n");
		free(map);
		free(order);
		ast_arena_free(a);
		return NULL;
	}
#define AST_SLICE_LINK(x) ((x) == AST_NONE ? AST_NONE : map[(x)])
	for (i = 0; i < cnt; i++) { MCC_TRACE("br\n");
		s = order[i];
		a->kind[i] = src->kind[s];
		a->parent[i] = AST_SLICE_LINK(src->parent[s]);
		a->first_child[i] = AST_SLICE_LINK(src->first_child[s]);
		a->last_child[i] = AST_SLICE_LINK(src->last_child[s]);
		a->next_sib[i] = AST_SLICE_LINK(src->next_sib[s]);
		a->nchild[i] = src->nchild[s];
		a->op[i] = src->op[s];
		a->type_t[i] = src->type_t[s];
		a->type_bp[i] = src->type_bp[s];
		a->type_bs[i] = src->type_bs[s];
		a->type_ref[i] = src->type_ref[s];
		a->ival[i] = src->ival[s];
		a->fbits[i] = src->fbits[s];
		a->sym[i] = src->sym[s];
		if (src->wide_hi) { MCC_TRACE("br\n");
			a->wide_hi[i] = src->wide_hi[s];
			a->wide_r2[i] = src->wide_r2[s];
		}
		if (src->st_have && a->st_have) { MCC_TRACE("br\n");
			a->st_have[i] = src->st_have[s];
			a->st_t[i] = src->st_t[s];
			a->st_ref[i] = src->st_ref[s];
			a->st_bp[i] = src->st_bp[s];
			a->st_bs[i] = src->st_bs[s];
		}
	}
#undef AST_SLICE_LINK
	free(map);
	free(order);
	return a;
}
#endif

AstLocal ast_node(AstArena *a, uint16_t kind) { MCC_TRACE("enter\n");
	ast_grow(a, a->count + 1);
	AstLocal n = a->count++;
	a->kind[n] = kind;
	a->parent[n] = AST_NONE;
	a->first_child[n] = AST_NONE;
	a->last_child[n] = AST_NONE;
	a->next_sib[n] = AST_NONE;
	a->nchild[n] = 0;
	a->op[n] = 0;
	a->type_t[n] = 0;
	a->type_bp[n] = 0;
	a->type_bs[n] = 0;
	a->type_ref[n] = 0;
	a->ival[n] = 0;
	a->fbits[n] = 0;
	a->sym[n] = 0;
	if (a->wide_hi) { MCC_TRACE("br\n");
		a->wide_hi[n] = 0;
		a->wide_r2[n] = AST_R2_NONE;
	}
	if (a->st_have) { MCC_TRACE("br\n");
		a->st_have[n] = 0;
		a->st_t[n] = 0;
		a->st_ref[n] = 0;
		a->st_bp[n] = 0;
		a->st_bs[n] = 0;
	}
	a->epoch++;
	return n;
}

static int ast_reparent_dbg_cached;
static const char *ast_rirproddump_cached;
static const char *ast_rvattr_cached;

void ast_add_child(AstArena *a, AstLocal parent, AstLocal child) { MCC_TRACE("enter\n");
	AST_ASSERT(parent < a->count && child < a->count);
	if (ast_reparent_dbg_cached && a->parent[child] != AST_NONE)
		fprintf(stderr, "[reparent] child=%u(k%u) from %u to %u\n", (unsigned)child,
						(unsigned)a->kind[child], (unsigned)a->parent[child], (unsigned)parent);
	a->epoch++;
	a->parent[child] = parent;
	a->next_sib[child] = AST_NONE;
	if (a->first_child[parent] == AST_NONE) { MCC_TRACE("br\n");
		a->first_child[parent] = child;
	} else { MCC_TRACE("br\n");
		a->next_sib[a->last_child[parent]] = child;
	}
	a->last_child[parent] = child;
	a->nchild[parent]++;
}

void ast_set_kind(AstArena *a, AstLocal n, uint16_t kind) { MCC_TRACE("enter\n");
	a->epoch++;
	a->kind[n] = kind;
}
int ast_detach_last_child(AstArena *a, AstLocal parent, AstLocal child) { MCC_TRACE("enter\n");
	AstLocal c, prev = AST_NONE;
	if (parent == AST_NONE || child == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (a->last_child[parent] != child)
		{ MCC_TRACE("br\n"); return 0; }
	for (c = a->first_child[parent]; c != AST_NONE && c != child;
			 c = a->next_sib[c])
		{ MCC_TRACE("br\n"); prev = c; }
	if (c != child)
		{ MCC_TRACE("br\n"); return 0; }
	a->epoch++;
	if (prev == AST_NONE)
		{ MCC_TRACE("br\n"); a->first_child[parent] = AST_NONE; }
	else
		{ MCC_TRACE("br\n"); a->next_sib[prev] = AST_NONE; }
	a->last_child[parent] = prev;
	a->nchild[parent]--;
	a->parent[child] = AST_NONE;
	a->next_sib[child] = AST_NONE;
	return 1;
}

void ast_clear_children(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	a->epoch++;
	a->first_child[n] = AST_NONE;
	a->last_child[n] = AST_NONE;
	a->nchild[n] = 0;
}

void ast_set_op(AstArena *a, AstLocal n, int op) { MCC_TRACE("enter\n");
	a->epoch++;
	a->op[n] = op;
}
void ast_set_type(AstArena *a, AstLocal n, int type_t, uint64_t type_ref) { MCC_TRACE("enter\n");
	a->epoch++;
	a->type_t[n] = type_t;
	a->type_bp[n] = 0;
	a->type_bs[n] = 0;
	a->type_ref[n] = type_ref;
}
void ast_set_type_bf(AstArena *a, AstLocal n, int type_t, uint64_t type_ref,
										 unsigned bp, unsigned bs) { MCC_TRACE("enter\n");
	a->epoch++;
	a->type_t[n] = type_t;
	a->type_bp[n] = (type_t & VT_BITFIELD) ? (uint8_t)bp : 0;
	a->type_bs[n] = (type_t & VT_BITFIELD) ? (uint8_t)bs : 0;
	a->type_ref[n] = type_ref;
}
void ast_copy_type(AstArena *a, AstLocal n, const AstArena *src, AstLocal m) { MCC_TRACE("enter\n");
	a->epoch++;
	a->type_t[n] = src->type_t[m];
	a->type_bp[n] = src->type_bp[m];
	a->type_bs[n] = src->type_bs[m];
	a->type_ref[n] = src->type_ref[m];
	if (src->st_have && src->st_have[m])
		{ MCC_TRACE("br\n"); ast_set_stype(a, n, src->st_t[m], src->st_ref[m],
																				src->st_bp[m], src->st_bs[m]); }
}
void ast_set_ival(AstArena *a, AstLocal n, uint64_t v) { MCC_TRACE("enter\n");
	a->epoch++;
	a->ival[n] = v;
}
void ast_set_fbits(AstArena *a, AstLocal n, uint64_t bits) { MCC_TRACE("enter\n");
	a->epoch++;
	a->fbits[n] = bits;
}
void ast_set_sym(AstArena *a, AstLocal n, uint64_t sym) { MCC_TRACE("enter\n");
	a->epoch++;
	a->sym[n] = sym;
}
uint16_t ast_kind(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->kind[n];
}
int ast_op(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->op[n];
}
int ast_type_t(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->type_t[n];
}
int ast_stype_known(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->type_t[n] != 0 || (a->st_have && a->st_have[n]);
}
int ast_stype_t(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (a->type_t[n])
		{ MCC_TRACE("br\n"); return a->type_t[n]; }
	return a->st_have ? a->st_t[n] : 0;
}
uint64_t ast_stype_ref(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (a->type_t[n])
		{ MCC_TRACE("br\n"); return a->type_ref[n]; }
	return a->st_have ? a->st_ref[n] : 0;
}
unsigned ast_stype_bp(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (a->type_t[n])
		{ MCC_TRACE("br\n"); return a->type_bp[n]; }
	return a->st_have ? a->st_bp[n] : 0;
}
unsigned ast_stype_bs(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (a->type_t[n])
		{ MCC_TRACE("br\n"); return a->type_bs[n]; }
	return a->st_have ? a->st_bs[n] : 0;
}
void ast_set_stype(AstArena *a, AstLocal n, int t, uint64_t ref, unsigned bp,
									 unsigned bs) { MCC_TRACE("enter\n");
	if (!a->st_have) { MCC_TRACE("br\n");
		AstLocal cap = a->cap ? a->cap : 1;
		a->st_have = calloc(cap, sizeof *a->st_have);
		a->st_t = calloc(cap, sizeof *a->st_t);
		a->st_ref = calloc(cap, sizeof *a->st_ref);
		a->st_bp = calloc(cap, sizeof *a->st_bp);
		a->st_bs = calloc(cap, sizeof *a->st_bs);
		if (!a->st_have || !a->st_t || !a->st_ref || !a->st_bp || !a->st_bs)
			{ MCC_TRACE("br\n"); return; }
	}
	a->st_have[n] = 1;
	a->st_t[n] = t;
	a->st_ref[n] = ref;
	a->st_bp[n] = (uint8_t)((t & VT_BITFIELD) ? bp : 0);
	a->st_bs[n] = (uint8_t)((t & VT_BITFIELD) ? bs : 0);
}
uint64_t ast_type_ref(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->type_ref[n];
}
unsigned ast_type_bp(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->type_bp[n];
}
unsigned ast_type_bs(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->type_bs[n];
}
uint64_t ast_ival(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->ival[n];
}
uint64_t ast_fbits(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->fbits[n];
}
uint64_t ast_sym(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->sym[n];
}
AstLocal ast_parent(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->parent[n];
}
AstLocal ast_first_child(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->first_child[n];
}
AstLocal ast_last_child(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->last_child[n];
}
AstLocal ast_next_sib(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->next_sib[n];
}
uint32_t ast_nchild(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return a->nchild[n];
}
AstLocal ast_count(const AstArena *a) { MCC_TRACE("enter\n");
	return a->count;
}
AstLocal ast_root(const AstArena *a) { MCC_TRACE("enter\n");
	return a->count ? 0 : AST_NONE;
}

AstLocal ast_child(const AstArena *a, AstLocal n, uint32_t i) { MCC_TRACE("enter\n");
	AstLocal c = a->first_child[n];
	while (c != AST_NONE && i--)
		{ MCC_TRACE("br\n"); c = a->next_sib[c]; }
	return c;
}

static const char *const kind_names[AST_KIND_COUNT] = {
		"BasicBlock",
		"If",
		"Jump",
		"Return",
		"Ref",
		"Literal",
		"Load",
		"Store",
		"Unary",
		"Binary",
		"Convert",
		"Invoke",
		"Poison",
		"StoreVal",
		"Bailout",
};

const char *ast_kind_name(uint16_t kind) { MCC_TRACE("enter\n");
	if (kind >= AST_KIND_COUNT)
		{ MCC_TRACE("br\n"); return "?"; }
	return kind_names[kind];
}

static void op_str(int op, char *buf, size_t cap) { MCC_TRACE("enter\n");
	if (op > 0x20 && op < 0x7f)
		{ MCC_TRACE("br\n"); snprintf(buf, cap, "%c", op); }
	else if (op)
		{ MCC_TRACE("br\n"); snprintf(buf, cap, "op#%d", op); }
	else
		{ MCC_TRACE("br\n"); buf[0] = 0; }
}

typedef struct {
	char *out;
	size_t cap;
	size_t len;
} DumpBuf;

static void dump_emit(DumpBuf *d, const char *s) { MCC_TRACE("enter\n");
	size_t n = strlen(s);
	for (size_t i = 0; i < n; i++) { MCC_TRACE("br\n");
		if (d->out && d->len < d->cap)
			{ MCC_TRACE("br\n"); d->out[d->len] = s[i]; }
		d->len++;
	}
}

static void dump_rec(const AstArena *a, AstLocal n, int depth, DumpBuf *d) { MCC_TRACE("enter\n");
	char line[128], ops[32];
	for (int i = 0; i < depth; i++)
		{ MCC_TRACE("br\n"); dump_emit(d, "  "); }
	op_str(a->op[n], ops, sizeof ops);
	switch (a->kind[n]) { MCC_TRACE("br\n");
	case AST_Literal:
		snprintf(line, sizeof line, "Literal %llu", (unsigned long long)a->ival[n]);
		break;
	case AST_Binary:
		snprintf(line, sizeof line, "Binary %s", ops);
		break;
	case AST_Unary:
		snprintf(line, sizeof line, "Unary %s", ops);
		break;
	case AST_Convert:
		snprintf(line, sizeof line, "Convert t=%d", a->type_t[n]);
		break;
	case AST_Ref:
		snprintf(line, sizeof line, "Ref #%llu", (unsigned long long)a->sym[n]);
		break;
	case AST_Invoke:
		snprintf(line, sizeof line, "Invoke #%llu", (unsigned long long)a->sym[n]);
		break;
	default:
		snprintf(line, sizeof line, "%s", ast_kind_name(a->kind[n]));
		break;
	}
	dump_emit(d, line);
	dump_emit(d, "\n");
	for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c])
		{ MCC_TRACE("br\n"); dump_rec(a, c, depth + 1, d); }
}

size_t ast_dump(const AstArena *a, AstLocal root, char *out, size_t cap) { MCC_TRACE("enter\n");
	DumpBuf d = {out, cap ? cap - 1 : 0, 0};
	if (root != AST_NONE && root < a->count)
		{ MCC_TRACE("br\n"); dump_rec(a, root, 0, &d); }
	if (out && cap)
		{ MCC_TRACE("br\n"); out[d.len < cap ? d.len : cap - 1] = 0; }
	return d.len;
}

int ast_validate(const AstArena *a, char *msg, size_t msgcap) { MCC_TRACE("enter\n");
#define AST_FAIL(m)                   \
	do {                                \
		if (msg)                          \
			snprintf(msg, msgcap, "%s", m); \
		return -1;                        \
	} while (0)
	for (AstLocal n = 0; n < a->count; n++) { MCC_TRACE("br\n");
		if (a->kind[n] >= AST_KIND_COUNT)
			{ MCC_TRACE("br\n"); AST_FAIL("node kind out of range"); }
		uint32_t seen = 0;
		AstLocal prev = AST_NONE;
		for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c]) { MCC_TRACE("br\n");
			if (a->parent[c] != n)
				{ MCC_TRACE("br\n"); AST_FAIL("child parent link mismatch"); }
			prev = c;
			seen++;
			if (seen > a->count)
				{ MCC_TRACE("br\n"); AST_FAIL("cyclic sibling chain"); }
		}
		if (seen != a->nchild[n])
			{ MCC_TRACE("br\n"); AST_FAIL("nchild disagrees with sibling chain"); }
		if (seen && a->last_child[n] != prev)
			{ MCC_TRACE("br\n"); AST_FAIL("last_child is not the final sibling"); }
	}
#undef AST_FAIL
	return 0;
}

typedef struct {
	uint64_t *syms;
	uint32_t nsym, cap;
	int oom;
} AstIhSyms;

static uint64_t ast_ih_fold(uint64_t h, uint64_t v) { MCC_TRACE("enter\n");
	int k = 8;
	for (; v; k--) { MCC_TRACE("br\n");
		h ^= v & 0xff;
		h *= 0x100000001b3u;
		v >>= 8;
	}
	if (k & 1)
		{ MCC_TRACE("br\n"); h *= 0x100000001b3u; }
	if (k & 2)
		{ MCC_TRACE("br\n"); h *= 0x366000002e329u; }
	if (k & 4)
		{ MCC_TRACE("br\n"); h *= 0x9ffaac085635bc91u; }
	if (k & 8)
		{ MCC_TRACE("br\n"); h *= 0x1efac7090aef4a21u; }
	return h;
}

static uint64_t ast_ih_sym(AstIhSyms *m, uint64_t sym) { MCC_TRACE("enter\n");
	for (uint32_t i = 0; i < m->nsym; i++)
		{ MCC_TRACE("br\n"); if (m->syms[i] == sym)
			{ MCC_TRACE("br\n"); return i + 1; } }
	if (m->nsym == m->cap) { MCC_TRACE("br\n");
		uint32_t ncap = m->cap ? m->cap * 2 : 32;
		uint64_t *ns = realloc(m->syms, ncap * sizeof *ns);
		if (!ns) { MCC_TRACE("br\n");
			m->oom = 1;
			return 0;
		}
		m->syms = ns;
		m->cap = ncap;
	}
	m->syms[m->nsym++] = sym;
	return m->nsym;
}

#ifndef VT_VALMASK
#define VT_VALMASK 0x007f
#else
typedef char ast_vt_valmask_check[VT_VALMASK == 0x007f ? 1 : -1];
#endif
#ifndef VT_LOCAL
#define VT_LOCAL 0x0032
#else
typedef char ast_vt_local_check[VT_LOCAL == 0x0032 ? 1 : -1];
#endif
#ifndef VT_SYM
#define VT_SYM 0x0200
#else
typedef char ast_vt_sym_check[VT_SYM == 0x0200 ? 1 : -1];
#endif
#ifndef VT_LONG
#define VT_LONG 0x1000
#else
typedef char ast_vt_long_check[VT_LONG == 0x1000 ? 1 : -1];
#endif
#ifndef MCC_PTR_SIZE
#define MCC_PTR_SIZE 8
#endif
#ifndef VT_BTYPE
#define VT_BTYPE 0x001f
#else
typedef char ast_vt_btype_check[VT_BTYPE == 0x001f ? 1 : -1];
#endif
#ifndef VT_PTR
#define VT_PTR 5
#else
typedef char ast_vt_ptr_check[VT_PTR == 5 ? 1 : -1];
#endif

static int ast_ih_sym_dropped(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return ((uint32_t)a->op[n] & VT_VALMASK) == VT_LOCAL &&
				 !((uint32_t)a->op[n] & VT_SYM);
}

static uint64_t ast_wide_fold(const AstArena *a, AstLocal n, uint64_t h) { MCC_TRACE("enter\n");
	uint64_t hi = ast_wide_hi(a, n);
	unsigned r2 = ast_wide_r2(a, n);
	if (!hi && r2 == AST_R2_NONE)
		{ MCC_TRACE("br\n"); return h; }
	h = ast_ih_fold(h, hi);
	return ast_ih_fold(h, r2);
}

static uint64_t ast_ih_node(const AstArena *a, AstLocal n, AstIhSyms *m,
														uint64_t h) { MCC_TRACE("enter\n");
	h = ast_ih_fold(h, a->kind[n]);
	h = ast_ih_fold(h, (uint32_t)a->op[n]);
	h = ast_ih_fold(h, (uint32_t)a->type_t[n]);
	h = ast_ih_fold(h, (uint32_t)a->type_bp[n] | ((uint32_t)a->type_bs[n] << 8));
	h = ast_ih_fold(h, (a->sym[n] && !ast_ih_sym_dropped(a, n)) ? ast_ih_sym(m, a->sym[n]) : 0);
	if (a->kind[n] != AST_Ref)
		{ MCC_TRACE("br\n"); h = ast_ih_fold(h, a->ival[n]); }
	h = ast_ih_fold(h, a->fbits[n]);
	h = ast_wide_fold(a, n, h);
	h = ast_ih_fold(h, a->nchild[n]);
	for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c])
		{ MCC_TRACE("br\n"); h = ast_ih_node(a, c, m, h); }
	return h;
}

uint64_t ast_intention_hash(const AstArena *a, AstLocal root) { MCC_TRACE("enter\n");
	if (!a || !a->count)
		{ MCC_TRACE("br\n"); return 0; }
	if (root == AST_NONE)
		{ MCC_TRACE("br\n"); root = ast_root(a); }
	if (root >= a->count)
		{ MCC_TRACE("br\n"); return 0; }
	AstIhSyms m = {NULL, 0, 0, 0};
	uint64_t h = ast_ih_node(a, root, &m, 0xcbf29ce484222325u);
	free(m.syms);
	return m.oom ? 0 : h;
}

#define AST_SID_MAXOFF 64
typedef struct {
	int32_t off[AST_SID_MAXOFF];
	int n;
	int over;
} AstSidOffs;

static uint64_t ast_sid_off(AstSidOffs *m, int32_t off) { MCC_TRACE("enter\n");
	for (int i = 0; i < m->n; i++)
		{ MCC_TRACE("br\n"); if (m->off[i] == off)
			{ MCC_TRACE("br\n"); return (uint64_t)(i + 1); } }
	if (m->n >= AST_SID_MAXOFF)
		{ MCC_TRACE("br\n"); m->over = 1; return 0; }
	m->off[m->n++] = off;
	return (uint64_t)m->n;
}

static int ast_sid_widthnorm_on(void) { MCC_TRACE("enter\n");
	static int on = -1;
	if (on < 0) { MCC_TRACE("br\n");
		const char *e = getenv("MCC_AST_SLICE_WIDTHNORM");
		on = e && e[0] && strcmp(e, "0") ? 1 : 0;
	}
	return on;
}

static uint32_t ast_sid_type_norm(uint32_t t) { MCC_TRACE("enter\n");
	return ast_sid_widthnorm_on() ? (t & ~(uint32_t)VT_LONG) : t;
}

static uint64_t ast_sid_ival_norm(const AstArena *a, AstLocal n, uint64_t v) { MCC_TRACE("enter\n");
	if (!ast_sid_widthnorm_on() ||
			((uint32_t)a->type_t[n] & VT_BTYPE) != VT_PTR)
		{ MCC_TRACE("br\n"); return v; }
	return ((v / MCC_PTR_SIZE) * 31u) ^ (v % MCC_PTR_SIZE);
}

static uint64_t ast_sid_node(const AstArena *a, AstLocal n, AstIhSyms *sm,
														 AstSidOffs *om, uint64_t h) { MCC_TRACE("enter\n");
	int is_local = a->kind[n] == AST_Ref &&
								 ((uint32_t)a->op[n] & VT_VALMASK) == VT_LOCAL &&
								 !((uint32_t)a->op[n] & VT_SYM);
	h = ast_ih_fold(h, a->kind[n]);
	h = ast_ih_fold(h, (uint32_t)a->op[n]);
	h = ast_ih_fold(h, ast_sid_type_norm((uint32_t)a->type_t[n]));
	h = ast_ih_fold(h, (uint32_t)a->type_bp[n] | ((uint32_t)a->type_bs[n] << 8));
	h = ast_ih_fold(h, (a->sym[n] && !ast_ih_sym_dropped(a, n)) ? ast_ih_sym(sm, a->sym[n]) : 0);
	if (is_local)
		{ MCC_TRACE("br\n"); h = ast_ih_fold(h, ast_sid_off(om, (int32_t)(int64_t)a->ival[n])); }
	else if (a->kind[n] != AST_Ref)
		{ MCC_TRACE("br\n"); h = ast_ih_fold(h, ast_sid_ival_norm(a, n, a->ival[n])); }
	h = ast_ih_fold(h, a->fbits[n]);
	h = ast_wide_fold(a, n, h);
	h = ast_ih_fold(h, a->nchild[n]);
	for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c])
		{ MCC_TRACE("br\n"); h = ast_sid_node(a, c, sm, om, h); }
	return h;
}

uint64_t ast_slice_ident_hash(const AstArena *a, AstLocal root) { MCC_TRACE("enter\n");
	if (!a || !a->count)
		{ MCC_TRACE("br\n"); return 0; }
	if (root == AST_NONE)
		{ MCC_TRACE("br\n"); root = ast_root(a); }
	if (root >= a->count)
		{ MCC_TRACE("br\n"); return 0; }
	AstIhSyms sm = {NULL, 0, 0, 0};
	AstSidOffs om = {{0}, 0, 0};
	uint64_t h = ast_sid_node(a, root, &sm, &om, 0xcbf29ce484222325u);
	free(sm.syms);
	return (sm.oom || om.over) ? 0 : h;
}

#pragma pop_macro("malloc")
#pragma pop_macro("realloc")
#pragma pop_macro("free")

#define AST_SLICE_MEMO_CAP 8192
#define AST_SLICE_MIN_NODES 3
#define AST_SLICE_MAX_NODES 65536

typedef struct AstSliceMemo {
	uint64_t ident;
	uint64_t gates;
	int64_t size;
	unsigned refcount;
	unsigned proven;
	uint64_t eligible;
} AstSliceMemo;

static AstSliceMemo ast_slice_memo[AST_SLICE_MEMO_CAP];
static int ast_slice_memo_n;
static long ast_slice_seen;
static long ast_slice_reuse;

static const AstSliceMemo *ast_slice_memo_get(uint64_t ident) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_slice_memo_n; i++)
		{ MCC_TRACE("br\n"); if (ast_slice_memo[i].ident == ident)
			{ MCC_TRACE("br\n"); return &ast_slice_memo[i]; } }
	return NULL;
}

static uint64_t ast_slice_eligible_now;

static void ast_slice_memo_put(uint64_t ident, uint64_t gates, int64_t size) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_slice_memo_n; i++) { MCC_TRACE("br\n");
		if (ast_slice_memo[i].ident == ident) { MCC_TRACE("br\n");
			ast_slice_memo[i].refcount++;
			ast_slice_memo[i].eligible |= ast_slice_eligible_now;
			if (size >= 0 && (ast_slice_memo[i].size < 0 || size < ast_slice_memo[i].size)) {
				MCC_TRACE("br\n");
				ast_slice_memo[i].size = size;
				ast_slice_memo[i].gates = gates;
			}
			return;
		}
	}
	if (ast_slice_memo_n >= AST_SLICE_MEMO_CAP)
		{ MCC_TRACE("br\n"); return; }
	ast_slice_memo[ast_slice_memo_n].ident = ident;
	ast_slice_memo[ast_slice_memo_n].gates = gates;
	ast_slice_memo[ast_slice_memo_n].size = size;
	ast_slice_memo[ast_slice_memo_n].eligible = ast_slice_eligible_now;
	ast_slice_memo[ast_slice_memo_n].refcount = 1;
	ast_slice_memo[ast_slice_memo_n].proven = 0;
	ast_slice_memo_n++;
}

#define AST_OP_ASMGEN 0x4001a
#define AST_OP_ASM 0x4001b
#define AST_OP_ASMOPS 0x40025

static int ast_op_is_asm(int op) { MCC_TRACE("enter\n");
	return op == AST_OP_ASM || op == AST_OP_ASMGEN || op == AST_OP_ASMOPS;
}

static int ast_slice_win_nodes(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int k = 1;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); k += ast_slice_win_nodes(a, c); }
	return k;
}

static int ast_slice_win_root_ok(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	uint16_t k = ast_kind(a, n);
	return (k == AST_Binary || k == AST_Unary || k == AST_Convert || k == AST_Load) &&
				 ast_first_child(a, n) != AST_NONE && !ast_op_is_asm(ast_op(a, n));
}

typedef void (*AstSliceVisitFn)(uint64_t ident, int size, uint64_t gates, void *ctx);

static long ast_slice_enum(const AstArena *a, uint64_t gates,
													 AstSliceVisitFn visit, void *ctx) { MCC_TRACE("enter\n");
	long recorded = 0;
	AstLocal nn;
	if (!a)
		{ MCC_TRACE("br\n"); return 0; }
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int sz;
		uint64_t id;
		if (!ast_slice_win_root_ok(a, n))
			{ MCC_TRACE("br\n"); continue; }
		sz = ast_slice_win_nodes(a, n);
		if (sz < AST_SLICE_MIN_NODES || sz > AST_SLICE_MAX_NODES)
			{ MCC_TRACE("br\n"); continue; }
		id = ast_slice_ident_hash(a, n);
		if (!id)
			{ MCC_TRACE("br\n"); continue; }
		visit(id, sz, gates, ctx);
		recorded++;
	}
	return recorded;
}

static void ast_slice_visit_put(uint64_t ident, int size, uint64_t gates, void *ctx) { MCC_TRACE("enter\n");
	(void)ctx;
	ast_slice_seen++;
	if (ast_slice_memo_get(ident))
		{ MCC_TRACE("br\n"); ast_slice_reuse++; }
	ast_slice_memo_put(ident, gates, (int64_t)size);
}

static long ast_slice_window_scan(const AstArena *a, uint64_t gates) { MCC_TRACE("enter\n");
	return ast_slice_enum(a, gates, ast_slice_visit_put, NULL);
}

#define AST_SLICE_REC_PROVEN_BIT ((uint64_t)1 << 32)
#define AST_SLICE_RECWORDS 5
#define AST_SLICE_RECBYTES (AST_SLICE_RECWORDS * 8)
#define AST_SLICE_REC_MAGIC 0x534fu

static long ast_slice_rec_serialize(const AstSliceMemo *recs, int n,
																		unsigned char *buf, long cap) { MCC_TRACE("enter\n");
	long off = 0;
	int i;
	if (!recs || !buf)
		{ MCC_TRACE("br\n"); return -1; }
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		uint64_t rec[AST_SLICE_RECWORDS];
		if (off + AST_SLICE_RECBYTES > cap)
			{ MCC_TRACE("br\n"); return -1; }
		rec[0] = recs[i].ident;
		rec[1] = recs[i].gates;
		rec[2] = (uint64_t)recs[i].size;
		rec[3] = ((uint64_t)recs[i].refcount) |
						 (recs[i].proven ? AST_SLICE_REC_PROVEN_BIT : 0) |
						 ((uint64_t)AST_SLICE_REC_MAGIC << 48);
		rec[4] = recs[i].eligible;
		memcpy(buf + off, rec, sizeof rec);
		off += AST_SLICE_RECBYTES;
	}
	return off;
}

static int ast_slice_rec_deserialize(const unsigned char *buf, long len,
																		 AstSliceMemo *out, int cap) { MCC_TRACE("enter\n");
	long off = 0;
	int n = 0;
	if (!buf || !out)
		{ MCC_TRACE("br\n"); return 0; }
	while (off + AST_SLICE_RECBYTES <= len && n < cap) { MCC_TRACE("br\n");
		uint64_t rec[AST_SLICE_RECWORDS];
		memcpy(rec, buf + off, sizeof rec);
		off += AST_SLICE_RECBYTES;
		if ((rec[3] >> 48) != AST_SLICE_REC_MAGIC)
			{ MCC_TRACE("br\n"); continue; }
		out[n].ident = rec[0];
		out[n].gates = rec[1];
		out[n].size = (int64_t)rec[2];
		out[n].refcount = (unsigned)(rec[3] & 0xffffffffu);
		out[n].proven = (rec[3] & AST_SLICE_REC_PROVEN_BIT) ? 1u : 0u;
		out[n].eligible = rec[4];
		n++;
	}
	return n;
}

static int ast_slice_multi_on(void) { MCC_TRACE("enter\n");
	static int on = -1;
	if (on < 0) { MCC_TRACE("br\n");
		const char *e = getenv("MCC_AST_SLICE_MULTI");
		on = e && e[0] && strcmp(e, "0") ? 1 : 0;
	}
	return on;
}

static void ast_slice_merge_one(AstSliceMemo *tab, int *n, int cap,
																const AstSliceMemo *rec) { MCC_TRACE("enter\n");
	int j;
	int multi = ast_slice_multi_on();
	for (j = 0; j < *n; j++) { MCC_TRACE("br\n");
		if (tab[j].ident != rec->ident)
			{ MCC_TRACE("br\n"); continue; }
		if (multi && tab[j].gates != rec->gates)
			{ MCC_TRACE("br\n"); continue; }
		tab[j].refcount += rec->refcount;
		if (rec->proven && !tab[j].proven) { MCC_TRACE("br\n");
			tab[j].proven = 1;
			tab[j].gates = rec->gates;
			tab[j].size = rec->size;
		} else if (rec->proven == tab[j].proven &&
							 rec->size >= 0 && (tab[j].size < 0 || rec->size < tab[j].size)) {
			MCC_TRACE("br\n");
			tab[j].size = rec->size;
			tab[j].gates = rec->gates;
		}
		return;
	}
	if (*n < cap)
		{ MCC_TRACE("br\n"); tab[(*n)++] = *rec; }
}

typedef struct AstSliceProbeCtx {
	const AstSliceMemo *table;
	int table_n;
	int64_t best_size;
	uint64_t best_gates;
	uint64_t best_elig;
	uint64_t allow;
	int best_proven;
	int best_surv;
	int found;
} AstSliceProbeCtx;

static int ast_slice_popcount(uint64_t v) { MCC_TRACE("enter\n");
	int c = 0;
	while (v) { MCC_TRACE("br\n"); v &= v - 1; c++; }
	return c;
}

static void ast_slice_visit_probe(uint64_t ident, int size, uint64_t gates, void *ctx) { MCC_TRACE("enter\n");
	AstSliceProbeCtx *p = (AstSliceProbeCtx *)ctx;
	int i;
	int multi = ast_slice_multi_on();
	(void)gates;
	for (i = 0; i < p->table_n; i++) { MCC_TRACE("br\n");
		if (p->table[i].ident == ident) { MCC_TRACE("br\n");
			int prov = p->table[i].proven ? 1 : 0;
			int surv = multi ? ast_slice_popcount(p->table[i].gates & p->allow) : 0;
			int better = !p->found || prov > p->best_proven ||
									 (prov == p->best_proven && multi && surv > p->best_surv) ||
									 (prov == p->best_proven && (!multi || surv == p->best_surv) &&
										(int64_t)size > p->best_size);
			if (better) { MCC_TRACE("br\n");
				p->found = 1;
				p->best_proven = prov;
				p->best_surv = surv;
				p->best_size = size;
				p->best_gates = p->table[i].gates;
				p->best_elig = p->table[i].eligible;
			}
			if (!multi)
				{ MCC_TRACE("br\n"); break; }
		}
	}
}

static int ast_slice_probe_table_ex(const AstArena *a, const AstSliceMemo *table,
																		int table_n, uint64_t allow,
																		uint64_t *out_gates, uint64_t *out_elig) { MCC_TRACE("enter\n");
	AstSliceProbeCtx p;
	p.table = table;
	p.table_n = table_n;
	p.best_size = -1;
	p.best_gates = 0;
	p.best_elig = 0;
	p.allow = allow;
	p.best_proven = 0;
	p.best_surv = -1;
	p.found = 0;
	if (!a || !table || table_n <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	ast_slice_enum(a, 0, ast_slice_visit_probe, &p);
	if (p.found && out_gates)
		{ MCC_TRACE("br\n"); *out_gates = p.best_gates; }
	if (p.found && out_elig)
		{ MCC_TRACE("br\n"); *out_elig = p.best_elig; }
	return p.found;
}

static int ast_slice_probe_table_cand(const AstArena *a, const AstSliceMemo *table,
																			int table_n, uint64_t *out_chosen,
																			uint64_t *out_elig) { MCC_TRACE("enter\n");
	return ast_slice_probe_table_ex(a, table, table_n, ~(uint64_t)0, out_chosen,
																	out_elig);
}

static int ast_slice_probe_table(const AstArena *a, const AstSliceMemo *table,
																 int table_n, uint64_t *out_gates) { MCC_TRACE("enter\n");
	return ast_slice_probe_table_ex(a, table, table_n, ~(uint64_t)0, out_gates, 0);
}

static int ast_subtree_has_storeval(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal c;
	if (n == AST_NONE || n >= a->count)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_StoreVal)
		{ MCC_TRACE("br\n"); return 1; }
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_subtree_has_storeval(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static AstLocal ast_slice_graft_rec(AstArena *a, const AstArena *k,
																		AstLocal ksrc) { MCC_TRACE("enter\n");
	AstLocal c, cc, n = ast_node(a, ast_kind(k, ksrc));
	ast_set_op(a, n, ast_op(k, ksrc));
	ast_copy_type(a, n, k, ksrc);
	ast_set_ival(a, n, ast_ival(k, ksrc));
	ast_set_fbits(a, n, ast_fbits(k, ksrc));
	ast_set_sym(a, n, ast_sym(k, ksrc));
	ast_set_wide(a, n, ast_wide_hi(k, ksrc), ast_wide_r2(k, ksrc));
	for (c = ast_first_child(k, ksrc); c != AST_NONE; c = ast_next_sib(k, c)) { MCC_TRACE("br\n");
		cc = ast_slice_graft_rec(a, k, c);
		ast_add_child(a, n, cc);
	}
	return n;
}

int ast_slice_splice(AstArena *a, AstLocal site_root, const AstArena *kernel_src,
										 AstLocal kernel_root) { MCC_TRACE("enter\n");
	AstLocal c, cc;
	int spliced;
	if (!a || !kernel_src || !a->count || !kernel_src->count)
		{ MCC_TRACE("br\n"); return 0; }
	if (site_root >= a->count || kernel_root >= kernel_src->count)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_subtree_has_storeval(kernel_src, kernel_root))
		{ MCC_TRACE("br\n"); return 0; }
	ast_clear_children(a, site_root);
	ast_set_kind(a, site_root, ast_kind(kernel_src, kernel_root));
	ast_set_op(a, site_root, ast_op(kernel_src, kernel_root));
	ast_copy_type(a, site_root, kernel_src, kernel_root);
	ast_set_ival(a, site_root, ast_ival(kernel_src, kernel_root));
	ast_set_fbits(a, site_root, ast_fbits(kernel_src, kernel_root));
	ast_set_sym(a, site_root, ast_sym(kernel_src, kernel_root));
	ast_set_wide(a, site_root, ast_wide_hi(kernel_src, kernel_root),
							 ast_wide_r2(kernel_src, kernel_root));
	spliced = 1;
	for (c = ast_first_child(kernel_src, kernel_root); c != AST_NONE;
			 c = ast_next_sib(kernel_src, c)) { MCC_TRACE("br\n");
		cc = ast_slice_graft_rec(a, kernel_src, c);
		ast_add_child(a, site_root, cc);
		spliced += ast_slice_win_nodes(a, cc);
	}
	return spliced;
}

int ast_slice_locate(const AstArena *a, uint64_t ident, AstLocal *sites,
										 int max) { MCC_TRACE("enter\n");
	int found = 0;
	AstLocal n, nn;
	if (!a || !ident || !sites || max <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	nn = ast_count(a);
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int sz;
		uint64_t id;
		if (!ast_slice_win_root_ok(a, n))
			{ MCC_TRACE("br\n"); continue; }
		sz = ast_slice_win_nodes(a, n);
		if (sz < AST_SLICE_MIN_NODES || sz > AST_SLICE_MAX_NODES)
			{ MCC_TRACE("br\n"); continue; }
		id = ast_slice_ident_hash(a, n);
		if (id != ident)
			{ MCC_TRACE("br\n"); continue; }
		if (found < max)
			{ MCC_TRACE("br\n"); sites[found] = n; }
		found++;
	}
	return found;
}

long ast_slice_breakeven_lanes(long nodes) { MCC_TRACE("enter\n");
	static const long thr[6] = {3, 7, 15, 31, 63, 127};
	static const long lanes[6] = {322, 108, 48, 24, 23, 8};
	long best = lanes[0];
	int i;
	for (i = 0; i < 6; i++)
		if (nodes >= thr[i])
			{ MCC_TRACE("br\n"); best = lanes[i]; }
	return best;
}

int64_t ast_slice_width_cost(int64_t nodes, int64_t lanes) { MCC_TRACE("enter\n");
	long need;
	if (nodes < 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (lanes <= 0)
		{ MCC_TRACE("br\n"); return nodes; }
	need = ast_slice_breakeven_lanes((long)nodes);
	if (lanes >= need)
		{ MCC_TRACE("br\n"); return nodes; }
	return nodes * (int64_t)((need + lanes - 1) / lanes);
}

int ast_slice_promote_static(int64_t baseline_cost, int64_t candidate_cost) { MCC_TRACE("enter\n");
	if (candidate_cost < 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (baseline_cost < 0)
		{ MCC_TRACE("br\n"); return 1; }
	return candidate_cost < baseline_cost ? 1 : 0;
}

#define AST_COLOR_MAX 64

static int ast_popcount64(uint64_t x) { MCC_TRACE("enter\n");
	int c = 0;
	while (x) { MCC_TRACE("br\n");
		x &= x - 1;
		c++;
	}
	return c;
}

int ast_color_graph(int n, const uint64_t *adj, const int *cost, int k, int *color) { MCC_TRACE("enter\n");
	for (int i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); color[i] = -1; }
	if (n <= 0 || k <= 0 || n > AST_COLOR_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	int deg[AST_COLOR_MAX];
	int order[AST_COLOR_MAX];
	uint64_t removed = 0;
	int top = 0;
	uint64_t all = (n < 64) ? ((uint64_t)1 << n) - 1 : ~(uint64_t)0;
	for (int i = 0; i < n; i++) { MCC_TRACE("br\n");
		color[i] = -1;
		deg[i] = ast_popcount64(adj[i] & all);
	}
	for (int cnt = 0; cnt < n; cnt++) { MCC_TRACE("br\n");
		int pick = -1;
		for (int i = 0; i < n; i++)
			{ MCC_TRACE("br\n"); if (!(removed & ((uint64_t)1 << i)) && deg[i] < k) { MCC_TRACE("br\n");
				pick = i;
				break;
			} }
		if (pick < 0)
			{ MCC_TRACE("br\n"); for (int i = 0; i < n; i++)
				{ MCC_TRACE("br\n"); if (!(removed & ((uint64_t)1 << i)) &&
						(pick < 0 || cost[i] < cost[pick]))
					{ MCC_TRACE("br\n"); pick = i; } } }
		if (pick < 0)
			{ MCC_TRACE("br\n"); break; }
		removed |= (uint64_t)1 << pick;
		order[top++] = pick;
		for (int j = 0; j < n; j++)
			{ MCC_TRACE("br\n"); if (!(removed & ((uint64_t)1 << j)) && (adj[pick] & ((uint64_t)1 << j)))
				{ MCC_TRACE("br\n"); deg[j]--; } }
	}
	int ncol = 0;
	for (int idx = top - 1; idx >= 0; idx--) { MCC_TRACE("br\n");
		int i = order[idx];
		uint64_t used = 0;
		for (int j = 0; j < n; j++)
			{ MCC_TRACE("br\n"); if (color[j] >= 0 && (adj[i] & ((uint64_t)1 << j)))
				{ MCC_TRACE("br\n"); used |= (uint64_t)1 << color[j]; } }
		int c = -1;
		for (int cc = 0; cc < k; cc++)
			{ MCC_TRACE("br\n"); if (!(used & ((uint64_t)1 << cc))) { MCC_TRACE("br\n");
				c = cc;
				break;
			} }
		color[i] = c;
		if (c >= 0)
			{ MCC_TRACE("br\n"); ncol++; }
	}
	return ncol;
}

#if defined(__GNUC__)
#define AST_VLAT_ATTR __attribute__((unused))
#else
#define AST_VLAT_ATTR
#endif

typedef struct {
	int64_t lo, hi;
	uint64_t kzero, kone;
	int tt;
	uint8_t state;
} AstVLat;

enum { AST_VLAT_TOP = 0, AST_VLAT_FACT = 1, AST_VLAT_BOTTOM = 2 };

static AstVLat ast_vlat_top(void) AST_VLAT_ATTR;
static AstVLat ast_vlat_top(void) { MCC_TRACE("enter\n");
	AstVLat v;
	v.lo = 0;
	v.hi = 0;
	v.kzero = 0;
	v.kone = 0;
	v.tt = 0;
	v.state = AST_VLAT_TOP;
	return v;
}

static AstVLat ast_vlat_bottom(void) AST_VLAT_ATTR;
static AstVLat ast_vlat_bottom(void) { MCC_TRACE("enter\n");
	AstVLat v;
	v.lo = INT64_MIN;
	v.hi = INT64_MAX;
	v.kzero = 0;
	v.kone = 0;
	v.tt = 0;
	v.state = AST_VLAT_BOTTOM;
	return v;
}

static AstVLat ast_vlat_full_fact(int64_t lo, int64_t hi, int tt) AST_VLAT_ATTR;
static AstVLat ast_vlat_full_fact(int64_t lo, int64_t hi, int tt) { MCC_TRACE("enter\n");
	AstVLat v;
	v.lo = lo;
	v.hi = hi;
	v.kzero = 0;
	v.kone = 0;
	v.tt = tt;
	v.state = AST_VLAT_FACT;
	return v;
}

static AstVLat ast_vlat_meet(AstVLat x, AstVLat y) AST_VLAT_ATTR;
static AstVLat ast_vlat_meet(AstVLat x, AstVLat y) { MCC_TRACE("enter\n");
	AstVLat r;
	if (x.state == AST_VLAT_TOP)
		{ MCC_TRACE("br\n"); return y; }
	if (y.state == AST_VLAT_TOP)
		{ MCC_TRACE("br\n"); return x; }
	if (x.state == AST_VLAT_BOTTOM || y.state == AST_VLAT_BOTTOM)
		{ MCC_TRACE("br\n"); return ast_vlat_bottom(); }
	r.lo = x.lo < y.lo ? x.lo : y.lo;
	r.hi = x.hi > y.hi ? x.hi : y.hi;
	r.kzero = x.kzero & y.kzero;
	r.kone = x.kone & y.kone;
	r.tt = x.tt;
	r.state = AST_VLAT_FACT;
	return r;
}

static void ast_vlat_refine_bound(AstVLat *v, int64_t bound, int is_lower) AST_VLAT_ATTR;
static void ast_vlat_refine_bound(AstVLat *v, int64_t bound, int is_lower) { MCC_TRACE("enter\n");
	if (v->state != AST_VLAT_FACT)
		{ MCC_TRACE("br\n"); return; }
	if (is_lower) { MCC_TRACE("br\n");
		if (bound > v->lo)
			{ MCC_TRACE("br\n"); v->lo = bound; }
	} else if (bound < v->hi) { MCC_TRACE("br\n");
		v->hi = bound;
	}
	if (v->lo > v->hi)
		{ MCC_TRACE("br\n"); *v = ast_vlat_bottom(); }
}

static int ast_vlat_narrowing(AstArena *a, int off, int width_tt) AST_VLAT_ATTR;
static int ast_vlat_context(AstArena *a, int off, AstVLat *out) AST_VLAT_ATTR;
static int ast_vlat_context_at(AstArena *a, AstLocal use, AstVLat *out) AST_VLAT_ATTR;

static int ast_vlat_fits_bytes(const AstVLat *v, int width) AST_VLAT_ATTR;
static int ast_vlat_fits_bytes(const AstVLat *v, int width) { MCC_TRACE("enter\n");
	uint64_t umax;
	int64_t smax, smin;
	if (v->state != AST_VLAT_FACT || width <= 0 || width >= 8)
		{ MCC_TRACE("br\n"); return 0; }
	umax = ((uint64_t)1 << (width * 8)) - 1;
	smax = (int64_t)(umax >> 1);
	smin = -smax - 1;
	if (v->lo >= 0 && (uint64_t)v->hi <= umax)
		{ MCC_TRACE("br\n"); return 1; }
	if (v->lo >= smin && v->hi <= smax)
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

#ifdef MCC_INTERNAL

#define gjmp_addr gjmp_addr_acs
#define gjmp gjmp_acs

int ast_env_int(const char *name, int dflt) { MCC_TRACE("enter\n");
	const char *v = getenv(name);
	int n;
	if (!v || !v[0])
		{ MCC_TRACE("br\n"); return dflt; }
	n = atoi(v);
	return n > 0 ? n : dflt;
}
int ast_active;
static int ast_replay_env;
static int ast_rir_nofb_env;
static int ast_rir_nomat_env;
static int ast_rir_noinv_env;
static int ast_replay_dump;
static const char *ast_verify_diff;
static int ast_graft_limit;
static int ast_graft_total;
static int ast_promo_limit;
static int ast_promo_total;
static int ast_opt_limit;
static int ast_opt_total;
static int ast_inline_node_limit = 64;
static int ast_graft_budget_max = 2048;
static int ast_inline_divguard = 1;
static int ast_cost_env;
#ifndef MCC_OPT_TLS
#define MCC_OPT_TLS MCC_THREAD_LOCAL
#endif
static MCC_OPT_TLS int ast_sethi_env;
static MCC_OPT_TLS int ast_sethi_leaf_env;
static int ast_sethi_nary_env;
static MCC_OPT_TLS int ast_bitflag_env;
static int ast_bitflag_report_env;
static int ast_bitflag_min;
static int ast_cprop_join_env;
static MCC_OPT_TLS int ast_narrow_env;
static MCC_OPT_TLS int ast_switch_expr_env;
int ast_trunc32_env;
static MCC_OPT_TLS int ast_narrow_fix_env;
static MCC_OPT_TLS int ast_narrow_c0_env;
static MCC_OPT_TLS int ast_narrow_c1_env;
static MCC_OPT_TLS int ast_narrow_c2_env;
static MCC_OPT_TLS int ast_narrow_c3_env;
static int ast_narrow_elim_env;
static MCC_OPT_TLS int ast_sccp_fix_env;
static MCC_OPT_TLS int ast_ident_conv_env;
static MCC_OPT_TLS int ast_ident_shift_env;
static MCC_OPT_TLS int ast_ident_arith_env;
static MCC_OPT_TLS int ast_ident_bit_env;
static MCC_OPT_TLS int ast_ident_rel_env;
static MCC_OPT_TLS int ast_ident_urange_env;
static MCC_OPT_TLS int ast_strict_overflow_env;
static MCC_OPT_TLS int ast_dse_call_env;
static MCC_OPT_TLS int ast_tco_ptr_env;
static MCC_OPT_TLS int ast_cse_comm_env;
static MCC_OPT_TLS int ast_cse_comm_rel_env;
static MCC_OPT_TLS int ast_range_env;
static MCC_OPT_TLS int ast_divmagic_env;
static MCC_OPT_TLS int ast_divrem_env;
static MCC_OPT_TLS int ast_divrem_folds;
static MCC_OPT_TLS int ast_vectorize_env;
static MCC_OPT_TLS int ast_slp_folds;
static MCC_OPT_TLS int ast_abs_env;
static int ast_select_env;
#define AST_SEL_MARK ((uint64_t)0x5E1EC7)
static MCC_OPT_TLS int ast_reassoc_env;
static MCC_OPT_TLS int ast_sra_env;
static MCC_OPT_TLS int ast_sroa_env;
static MCC_OPT_TLS int ast_sroa_param_env;
static MCC_OPT_TLS int ast_reassoc_assoc_env;
static MCC_OPT_TLS int ast_reassoc_shlshr_env;
static MCC_OPT_TLS int ast_reassoc_shrshl_env;
static MCC_OPT_TLS int ast_reassoc_muldist_env;
static MCC_OPT_TLS int ast_bfold_sqrt_env;
static MCC_OPT_TLS int ast_bfold_sign_env;
static MCC_OPT_TLS int ast_bfold_round_env;
static MCC_OPT_TLS int ast_bfold_minmax_env;
static MCC_OPT_TLS int ast_math_inline_env;
static MCC_OPT_TLS int ast_fabs_inline_env;
static MCC_OPT_TLS int ast_math_inline_prepass_env;
static MCC_OPT_TLS int ast_round_inline_env;
static MCC_OPT_TLS int ast_copysign_env;
static MCC_OPT_TLS int ast_minmax_inline_env;
static MCC_OPT_TLS int ast_fma_env;
static MCC_OPT_TLS int ast_no_math_errno;
static int ast_inline_pass_env;
static int ast_interchange_env;
static int ast_fusion_env;
static int ast_tile_env;
static int ast_unroll_env;
static int ast_loopidiom_env;
static int ast_bswap_idiom_env;
static int ast_rotate_idiom_env;
static int ast_tile_size;
static int ast_vlat_env;
int mccjit_recompiling;
static int ast_jit_env;
static int ast_jit_splice_env;
static int ast_jit_dispatch_env;
int ast_zero_bss_env;
int ast_merge_strings_env;
static int ast_strpool_n;
#define AST_CSE_MAX 256
static int ast_cse_window = 64;
#define AST_CPROP_MAX 512
static int ast_cprop_window = 128;
#define AST_INLINE_MAX_DEPTH 32
static int ast_inline_depth_max = 8;
#define AST_TCO_MAXP 64
static int ast_tco_maxp = 16;
static int ast_cse_join_env;
static int ast_call_window_env;
static MCC_OPT_TLS int ast_licm_temp_env;
static MCC_OPT_TLS int ast_ivsr_env;
static MCC_OPT_TLS int ast_ivsr_ptr_env;
static MCC_OPT_TLS int ast_pre_env;
static int ast_loopnest_dump_env;
static int ast_loopdep_dump_env;
static int ast_dep_alias_oracle_env;
static int ast_perfn_inproc_env;
static int ast_argfwd_env;

#define AST_LTEMP_MAX 128
#define AST_LTEMP_PER_LOOP 8
static int ast_ltemp_off[AST_LTEMP_MAX];
static int ast_ltemp_sz[AST_LTEMP_MAX];
static MCC_OPT_TLS int ast_ltemp_n;
static MCC_OPT_TLS int ast_ltemp_cur;
static int ast_color_env;
static int ast_spill_share_env;
static int ast_search_worker;
typedef unsigned long AstGateMask_fwd_check;
static unsigned long ast_search_floor;
static int ast_search_floor_env;
static unsigned long ast_search_gates_now(void);
static unsigned long ast_search_searchable(unsigned long base);

static MCC_OPT_TLS uint32_t ast_isa_key_term;

static uint64_t ast_intention_acc;
static const char *ast_hash_out;
static const char *ast_refcensus_path;
static void ast_refcensus(AstArena *a, const char *fn);

void ast_hash_out_emit(const char *tag, const char *fn, uint64_t h) { MCC_TRACE("enter\n");
	FILE *f;
	if (!ast_hash_out || !ast_hash_out[0] || !fn)
		{ MCC_TRACE("br\n"); return; }
	f = fopen(ast_hash_out, "a");
	if (!f)
		{ MCC_TRACE("br\n"); return; }
	fprintf(f, "%s%s %016llx\n", tag ? tag : "", fn, (unsigned long long)h);
	fclose(f);
}

uint64_t ast_intention_value(void) { MCC_TRACE("enter\n");
	return ast_intention_acc;
}

#define AST_FNCFG_MAX 256
static struct {
	char name[80];
	int tmpl, promo, inl;
} ast_fncfg[AST_FNCFG_MAX];
static int ast_fncfg_n;

static void ast_fncfg_parse(void) { MCC_TRACE("enter\n");
	const char *s = getenv("MCC_AST_FN_CONFIG");
	ast_fncfg_n = 0;
	if (!s)
		{ MCC_TRACE("br\n"); return; }
	while (*s && ast_fncfg_n < AST_FNCFG_MAX) { MCC_TRACE("br\n");
		const char *name = s;
		int nlen, bits;
		while (*s && *s != '=' && *s != ';')
			{ MCC_TRACE("br\n"); s++; }
		if (*s != '=')
			{ MCC_TRACE("br\n"); break; }
		nlen = (int)(s - name);
		if (nlen >= 80)
			{ MCC_TRACE("br\n"); nlen = 79; }
		bits = atoi(s + 1);
		while (*s && *s != ';')
			{ MCC_TRACE("br\n"); s++; }
		if (*s == ';')
			{ MCC_TRACE("br\n"); s++; }
		memcpy(ast_fncfg[ast_fncfg_n].name, name, nlen);
		ast_fncfg[ast_fncfg_n].name[nlen] = 0;
		ast_fncfg[ast_fncfg_n].tmpl = bits & 1;
		ast_fncfg[ast_fncfg_n].promo = (bits >> 1) & 1;
		ast_fncfg[ast_fncfg_n].inl = (bits >> 2) & 1;
		ast_fncfg_n++;
	}
}

static int ast_fncfg_find(const char *fn) { MCC_TRACE("enter\n");
	int i;
	if (!fn)
		{ MCC_TRACE("br\n"); return -1; }
	for (i = 0; i < ast_fncfg_n; i++)
		{ MCC_TRACE("br\n"); if (!strcmp(ast_fncfg[i].name, fn))
			{ MCC_TRACE("br\n"); return i; } }
	return -1;
}

static char ast_jit_fns[AST_FNCFG_MAX][80];
static int ast_jit_fns_n;
static int ast_fn_switch;

static void ast_jit_fns_default(void) { MCC_TRACE("enter\n");
	ast_jit_fns_n = 0;
}

static void ast_jit_fns_parse(const char *csv) { MCC_TRACE("enter\n");
	ast_jit_fns_n = 0;
	if (!csv) { MCC_TRACE("br\n");
		ast_jit_fns_default();
		return;
	}
	while (*csv && ast_jit_fns_n < AST_FNCFG_MAX) { MCC_TRACE("br\n");
		const char *name = csv;
		int nlen;
		while (*csv && *csv != ',')
			{ MCC_TRACE("br\n"); csv++; }
		nlen = (int)(csv - name);
		if (nlen >= 80)
			{ MCC_TRACE("br\n"); nlen = 79; }
		if (nlen > 0) { MCC_TRACE("br\n");
			memcpy(ast_jit_fns[ast_jit_fns_n], name, nlen);
			ast_jit_fns[ast_jit_fns_n][nlen] = 0;
			ast_jit_fns_n++;
		}
		if (*csv == ',')
			{ MCC_TRACE("br\n"); csv++; }
	}
	if (ast_jit_fns_n == 0)
		{ MCC_TRACE("br\n"); ast_jit_fns_default(); }
}

static int ast_jit_selected(const char *fn) { MCC_TRACE("enter\n");
	int i;
	if (!fn)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_jit_fns_n == 0)
		{ MCC_TRACE("br\n"); return 1; }
	for (i = 0; i < ast_jit_fns_n; i++)
		{ MCC_TRACE("br\n"); if (!strcmp(ast_jit_fns[i], fn))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_jit_type_eligible(int t) { MCC_TRACE("enter\n");
	switch (t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_BOOL:
	case VT_BYTE:
	case VT_SHORT:
	case VT_INT:
	case VT_LLONG:
	case VT_PTR:
	case VT_FUNC:
		return !(t & VT_BITFIELD);
	default:
		return 0;
	}
}

static int ast_jit_type_scalar(int t) { MCC_TRACE("enter\n");
	if (ast_jit_type_eligible(t))
		{ MCC_TRACE("br\n"); return 1; }
	return (t & VT_BTYPE) == VT_DOUBLE;
}

static int ast_jit_eligible(Sym *sym) { MCC_TRACE("enter\n");
	Sym *sig = sym ? sym->type.ref : NULL;
	Sym *p;
	int np = 0;
	if (!sig)
		{ MCC_TRACE("br\n"); return 0; }
	if (sig->f.func_type == FUNC_ELLIPSIS)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_fn_switch)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_jit_type_scalar(sig->type.t))
		{ MCC_TRACE("br\n"); return 0; }
	for (p = sig->next; p; p = p->next) { MCC_TRACE("br\n");
		if (++np > 6)
			{ MCC_TRACE("br\n"); return 0; }
		if (!ast_jit_type_scalar(p->type.t))
			{ MCC_TRACE("br\n"); return 0; }
	}
	return np >= 1;
}

static int ast_jit_body_has_vla(void);
static int ast_body_uses_func_alloca(void);

static int ast_jit_slot_taken(const char *fn) { MCC_TRACE("enter\n");
	MCCState *s1 = mcc_state;
	Section *st;
	char nm[256];
	unsigned long merged, i;
	if (!s1 || !symtab_section || !fn || !fn[0])
		{ MCC_TRACE("br\n"); return 0; }
	st = symtab_section;
	if (!st->link)
		{ MCC_TRACE("br\n"); return 0; }
	merged = (unsigned long)st->sh_offset / sizeof(ElfSym);
	if (merged <= 1)
		{ MCC_TRACE("br\n"); return 0; }
	snprintf(nm, sizeof nm, "%s__mccjit_slot_%s", s1->leading_underscore ? "_" : "",
					 fn);
	for (i = 1; i < merged; i++) { MCC_TRACE("br\n");
		ElfSym *es = (ElfSym *)st->data + i;
		if (!strcmp((const char *)st->link->data + es->st_name, nm))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int ast_jit_want(const char *fn, Sym *sym) { MCC_TRACE("enter\n");
	if (!ast_jit_selected(fn))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_jit_body_has_vla()) { MCC_TRACE("br\n");
		if (mcc_env_on("MCC_JIT_VERBOSE"))
			{ MCC_TRACE("br\n"); fprintf(stderr,
							"mccjit: refuse-to-JIT %s — body allocates a VLA\n",
							fn ? fn : "?"); }
		return 0;
	}
	if (ast_jit_eligible(sym))
		{ MCC_TRACE("br\n"); return 1; }
	if (mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr,
						"mccjit: refuse-to-JIT %s — signature not in the verified GP-int set\n",
						fn ? fn : "?"); }
	return 0;
}

static int ast_jit_eval_refused;
int ast_jit_eval_refused_count(void) { MCC_TRACE("enter\n"); return ast_jit_eval_refused; }

static MCC_OPT_TLS int ast_templates_env;
static MCC_OPT_TLS int ast_cload_env;
static int ast_search_env;
static int ast_slice_env;
static int ast_search_emitsize_env;
static int ast_search_emitiso_env;
static int ast_search_inline_env;
static int ast_search_want_inline;
static int ast_search_axis_ran;
static int ast_search_pick_inline;
static int ast_search_threads_env;
static int ast_search_pthreads_env;
static int ast_search_ordered_env;
static int ast_search_predict_env;
static int ast_search_verbose_env;
static int ast_search_walk_env;
static unsigned ast_search_ticks;

static int ast_search_walk_from_env(void) { MCC_TRACE("enter\n");
	const char *v = getenv("MCC_AST_SEARCH_WALK");
	if (!v || !v[0] || !strcmp(v, "linear"))
		{ MCC_TRACE("br\n"); return COMBO_WALK_LINEAR; }
	if (!strcmp(v, "dfs"))
		{ MCC_TRACE("br\n"); return COMBO_WALK_DFS; }
	if (!strcmp(v, "bfs"))
		{ MCC_TRACE("br\n"); return COMBO_WALK_BFS; }
	if (!strcmp(v, "product"))
		{ MCC_TRACE("br\n"); return COMBO_WALK_PRODUCT; }
	return COMBO_WALK_LINEAR;
}

static void ast_search_walk_trace(const int *sel, int k, int depth, int walk,
																	void *user) { MCC_TRACE("enter\n");
	char seq[COMBO_MAX * 4];
	int i, p = 0;
	(void)user;
	for (i = 0; i < k && p < (int)sizeof seq - 8; i++)
		{ MCC_TRACE("br\n"); p += sprintf(seq + p, "%s%d", i ? "," : "", sel[i]); }
	seq[p] = '\0';
	MCC_TRACE("combo walk=%s depth=%d k=%d seq=%s\n", combo_walk_name(walk), depth, k,
						seq);
}
#define AST_STRAT_COUNT_MAX 26
#define AST_CYCLE_MAX 8
static int ast_search_order_env;
static int ast_search_fullset_env;
static int ast_roi_env;
static int ast_roi_dump;
static int ast_cycle_env;
static int ast_strat_order[AST_STRAT_COUNT_MAX];
static int ast_strat_order_n;
static int ast_strat_order_forced;

static void ast_strat_order_reset(void) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < AST_STRAT_COUNT_MAX; i++)
		{ MCC_TRACE("br\n"); ast_strat_order[i] = i; }
	ast_strat_order_n = AST_STRAT_COUNT_MAX;
}

static void ast_order_seq_str(const int *seq, int n, char *out) { MCC_TRACE("enter\n");
	int i, p = 0;
	for (i = 0; i < n && p < AST_STRAT_COUNT_MAX * 4 - 8; i++)
		{ MCC_TRACE("br\n"); p += sprintf(out + p, "%s%d", i ? "," : "", seq[i]); }
	out[p] = '\0';
}

static void ast_strat_order_from_env(void) { MCC_TRACE("enter\n");
	const char *ov = getenv("MCC_AST_STRAT_ORDER");
	const char *p;
	int n = 0, val = 0, have = 0;
	ast_strat_order_reset();
	ast_strat_order_forced = 0;
	if (!ov || !ov[0])
		{ MCC_TRACE("br\n"); return; }
	for (p = ov;; p++) { MCC_TRACE("br\n");
		if (*p >= '0' && *p <= '9') { MCC_TRACE("br\n");
			val = val * 10 + (*p - '0');
			have = 1;
		} else { MCC_TRACE("br\n");
			if (have && n < AST_STRAT_COUNT_MAX && val < AST_STRAT_COUNT_MAX)
				{ MCC_TRACE("br\n"); ast_strat_order[n++] = val; }
			val = 0;
			have = 0;
			if (!*p)
				{ MCC_TRACE("br\n"); break; }
		}
	}
	if (n > 0) { MCC_TRACE("br\n");
		ast_strat_order_n = n;
		ast_strat_order_forced = 1;
	}
}
static int ast_promote_env;
static int ast_storeval_call_env;
static int ast_storeval_constl_env;
static int ast_storeval_callstore_env;
static int ast_storeval_callup_env;
static int ast_storeval_rot_env;
static int ast_storeval_calllast_env;
static int ast_sv_live_depth;
static int ast_cmp_mat_env;
static int ast_chainstore_live_env;
static int ast_chainstore_member_env;
static int ast_while_comma_env;
static int ast_loopcond_store_env;
static int ast_indirect_call_env;
static int ast_synth_ind0;
static int ast_synth_emitted;
static int ast_cleanup_sv_incall = -1;
static int ast_cleanup_bias;
static int ast_spill_modelled;
static int ast_ltemp_insert_before(AstArena *a, AstLocal parent, AstLocal pivot,
															 AstLocal node);
static int ast_call_dead;
static int ast_chainstore_env;
int ast_promo_incdec_env;
int ast_promo_arrow_env;
static int ast_promo_leaf_xmm_env;
#ifdef MCC_TARGET_X86_64
static int ast_xmm_hi_env;
#endif
int ast_fmov_imm_env;
int ast_reloc_equiv_env;
int ast_regdisp_env;
static int ast_cost_ops_env;
static int ast_cost_spill_env;
static int ast_promo_leaf_callee_env;
static int ast_no_callful_env;
static int ast_no_callful_promo;
static int ast_inline_env;
static int ast_tmpl_folds;
static MCC_OPT_TLS AstArena *ast_cur;
static int ast_reemit_poison;

static int ast_base_depth;
static AstLocal ast_cur_bb;
static int *ast_rp_bsym, *ast_rp_csym;

static int *ast_fconst;
static unsigned char *ast_fconst_cplx;
static unsigned char (*ast_fconst_key)[AST_FCONST_KEY];
static int ast_fconst_n;
static int ast_fconst_cap;
static int ast_fconst_i;
int ast_replaying;

static int *ast_locrec;
static int *ast_locrec_sz, *ast_locrec_al;
static int ast_locrec_n, ast_locrec_cap, ast_locrec_i;
static int ast_fpert_on, ast_fpert_base, ast_fpert_scale, ast_fpert_pad;
static int ast_fpert_dbg;
static int ast_fpert_floor, ast_fpert_live;
#define AST_FPERT_MAX 4096
static int ast_fpert_from[AST_FPERT_MAX], ast_fpert_to[AST_FPERT_MAX];
static int ast_fpert_n;

static void ast_fpert_bind(int from, int to) { MCC_TRACE("enter\n");
	if (ast_fpert_n >= AST_FPERT_MAX)
		{ MCC_TRACE("br\n"); return; }
	ast_fpert_from[ast_fpert_n] = from;
	ast_fpert_to[ast_fpert_n++] = to;
}

static int ast_fpert_map(int off) { MCC_TRACE("enter\n");
	if (!ast_fpert_live)
		{ MCC_TRACE("br\n"); return off; }
	for (int i = ast_fpert_n - 1; i >= 0; i--)
		{ MCC_TRACE("br\n"); if (ast_fpert_from[i] == off)
			{ MCC_TRACE("br\n"); return ast_fpert_to[i]; } }
	if (off >= ast_fpert_floor)
		{ MCC_TRACE("br\n"); return off; }
	return ast_fpert_floor + ast_fpert_base +
				 (off - ast_fpert_floor) * ast_fpert_scale;
}
static int ast_loc_low;
static int ast_graft_base;
static int ast_locrec_min;
static int ast_temp_frontier;
static int ir_cap_replaying;

uint64_t ast_pinned_regs;
int ast_reemit_guard_op;
int ast_func_has_asm;
int ast_func_has_labeladdr;

static int ast_alloc_frontier_loc(int size, int align, int floor);

static int ast_locrec_take(int size, int align) { MCC_TRACE("enter\n");
	int k = ast_locrec_i;
	while (k < ast_locrec_n &&
				 (ast_locrec_sz[k] < size || ast_locrec_al[k] < align))
		{ MCC_TRACE("br\n"); k++; }
	if (k >= ast_locrec_n)
		{ MCC_TRACE("br\n"); return 0; }
	ast_locrec_i = k + 1;
	return 1;
}

int ast_alloc_loc(int size, int align) { MCC_TRACE("enter\n");
	if (rir_c2_active) { MCC_TRACE("br\n");
		int rl;
		if (rir_loc_replay(&rl, size, align)) { MCC_TRACE("br\n");
			loc = rl;
			if (loc < ast_loc_low)
				{ MCC_TRACE("br\n"); ast_loc_low = loc; }
			return loc;
		}
		loc = ast_alloc_frontier_loc(size, align, rir_locrec_min);
		if (loc < ast_loc_low)
			{ MCC_TRACE("br\n"); ast_loc_low = loc; }
		return loc;
	}
	if (ast_replaying && !ir_cap_replaying) { MCC_TRACE("br\n");
		if (ast_locrec_take(size, align)) { MCC_TRACE("br\n");
			int orig = ast_locrec[ast_locrec_i - 1];
			if (ast_fpert_on) { MCC_TRACE("br\n");
				loc = (loc - size - ast_fpert_pad) & -align;
				ast_fpert_bind(orig, loc);
			} else { MCC_TRACE("br\n");
				loc = orig;
			}
			if (loc < ast_loc_low)
				{ MCC_TRACE("br\n"); ast_loc_low = loc; }
			return loc;
		}
		if (ast_locrec_n) { MCC_TRACE("br\n");
			loc = ast_alloc_temp_loc(size, align);
			if (loc < ast_loc_low)
				{ MCC_TRACE("br\n"); ast_loc_low = loc; }
			return loc;
		}
	}
	loc = (loc - size) & -align;
	if (loc < ast_loc_low)
		{ MCC_TRACE("br\n"); ast_loc_low = loc; }
	if (rir_active && !ast_replaying && !ir_cap_replaying)
		{ MCC_TRACE("br\n"); rir_loc_record(loc, size, align); }
	if (ast_active && !ast_replaying) { MCC_TRACE("br\n");
		if (ast_locrec_n == ast_locrec_cap) { MCC_TRACE("br\n");
			ast_locrec_cap = ast_locrec_cap ? ast_locrec_cap * 2 : 16;
			ast_locrec = mcc_realloc(ast_locrec, ast_locrec_cap * sizeof *ast_locrec);
			ast_locrec_sz =
					mcc_realloc(ast_locrec_sz, ast_locrec_cap * sizeof *ast_locrec_sz);
			ast_locrec_al =
					mcc_realloc(ast_locrec_al, ast_locrec_cap * sizeof *ast_locrec_al);
		}
		ast_locrec_sz[ast_locrec_n] = size;
		ast_locrec_al[ast_locrec_n] = align;
		ast_locrec[ast_locrec_n++] = loc;
		if (loc < ast_locrec_min)
			{ MCC_TRACE("br\n"); ast_locrec_min = loc; }
	}
	return loc;
}

/* T-mac-30246: snapshot/restore the loc-record cursor state (all static to this
 * TU) so a SIDE-EFFECT-FREE semantic check -- e.g. type-checking a _Generic
 * NON-selected association -- can be fully rewound and leave zero -O0 bytes and
 * zero AST/RIR replay footprint. The caller additionally saves `loc`/`anon_sym`/
 * `nb_temp_local_vars` (its own globals) and gates ast_active/rir_active/
 * ast_replaying/rir_c2_active off around the check. */
void ast_locrec_snapshot(int out[4]) { MCC_TRACE("enter\n");
	out[0] = ast_loc_low;
	out[1] = ast_locrec_n;
	out[2] = ast_locrec_i;
	out[3] = ast_locrec_min;
}
void ast_locrec_restore(const int in[4]) { MCC_TRACE("enter\n");
	ast_loc_low = in[0];
	ast_locrec_n = in[1];
	ast_locrec_i = in[2];
	ast_locrec_min = in[3];
}

static void ast_locrec_skip(int size, int align) { MCC_TRACE("enter\n");
	if (ast_replaying && !ir_cap_replaying)
		{ MCC_TRACE("br\n"); (void)ast_locrec_take(size, align); }
}

typedef struct {
	int off[AST_LTEMP_MAX];
	int sz[AST_LTEMP_MAX];
	int n;
	int cur;
} AstLtempSave;

static void ast_ltemp_save(AstLtempSave *s) { MCC_TRACE("enter\n");
	s->n = ast_ltemp_n;
	s->cur = ast_ltemp_cur;
	memcpy(s->off, ast_ltemp_off, sizeof s->off);
	memcpy(s->sz, ast_ltemp_sz, sizeof s->sz);
}

static void ast_ltemp_restore(const AstLtempSave *s) { MCC_TRACE("enter\n");
	ast_ltemp_n = s->n;
	ast_ltemp_cur = s->cur;
	memcpy(ast_ltemp_off, s->off, sizeof ast_ltemp_off);
	memcpy(ast_ltemp_sz, s->sz, sizeof ast_ltemp_sz);
}

static int ast_ltemp_add(int off, int sz) { MCC_TRACE("enter\n");
	if (ast_ltemp_n >= AST_LTEMP_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	ast_ltemp_off[ast_ltemp_n] = off;
	ast_ltemp_sz[ast_ltemp_n] = sz > 0 ? sz : 8;
	ast_ltemp_n++;
	return 1;
}

static int ast_ltemp_mint(int size, int align) { MCC_TRACE("enter\n");
	int off;
	if (size <= 0)
		{ MCC_TRACE("br\n"); size = 8; }
	if (align <= 0)
		{ MCC_TRACE("br\n"); align = 8; }
	if (ast_ltemp_n >= AST_LTEMP_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	off = (ast_ltemp_cur - size) & -align;
	if (!ast_ltemp_add(off, size))
		{ MCC_TRACE("br\n"); return 0; }
	ast_ltemp_cur = off;
	return off;
}

static int ast_ltemp_size(int et, uint64_t er, int *palign) { MCC_TRACE("enter\n");
	CType ct;
	ct.t = et;
	ct.ref = (Sym *)(uintptr_t)er;
	int al = 8, sz = type_size(&ct, &al);
	if (sz <= 8)
		{ MCC_TRACE("br\n"); *palign = 8; return 8; }
	if (al < 8)
		{ MCC_TRACE("br\n"); al = 8; }
	*palign = al;
	return sz;
}

int ast_ltemp_overlaps(int lo, int sz) { MCC_TRACE("enter\n");
	for (int t = 0; t < ast_ltemp_n; t++) { MCC_TRACE("br\n");
		int a = ast_ltemp_off[t], as = ast_ltemp_sz[t] > 0 ? ast_ltemp_sz[t] : 8;
		if (lo < a + as && a < lo + sz)
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int ast_alloc_frontier_loc(int size, int align, int floor) { MCC_TRACE("enter\n");
	if (ast_temp_frontier > 0)
		{ MCC_TRACE("br\n"); ast_temp_frontier = floor < loc ? floor : loc; }
	ast_temp_frontier = (ast_temp_frontier - size) & -align;
	if (ast_temp_frontier < ast_loc_low)
		{ MCC_TRACE("br\n"); ast_loc_low = ast_temp_frontier; }
	return ast_temp_frontier;
}

int ast_alloc_temp_loc(int size, int align) { MCC_TRACE("enter\n");
	if (ast_replaying && !ir_cap_replaying) { MCC_TRACE("br\n");
		return ast_alloc_frontier_loc(size, align, ast_locrec_min);
	}
	loc = (loc - size) & -align;
	return loc;
}

static int ast_member_cap;
static int ast_member_arrow;
static int ast_imag_cap;
static int ast_bcplx_cap;

static AstLocal ast_switch_node;
static struct switch_t *ast_rp_switch;

struct ast_rp_label {
	int v, jind, jnext, defined;
};
static struct ast_rp_label *ast_rp_labels;
static int ast_rp_nlabel, ast_rp_caplabel;
static int ast_rp_label_floor;
static int ast_rp_asmops;

static AstLocal ast_dead_value(void) { MCC_TRACE("enter\n");
	AstLocal p = ast_node(ast_cur, AST_Literal);
	ast_set_op(ast_cur, p, VT_CONST);
	ast_set_type(ast_cur, p, VT_INT, 0);
	ast_set_ival(ast_cur, p, 0);
	ast_set_sym(ast_cur, p, 0);
	return p;
}

static int ast_expr_pure(AstArena *a, AstLocal n, int depth);
static int ast_struct_eq(AstArena *a, AstLocal x, AstLocal y, int depth);
#define AST_OP_ADDR 0x40000
#define AST_OP_MEMBER 0x40001
#define AST_OP_MEMBER_ARROW 0x40002
#define AST_OP_IMAG 0x40003
#define AST_OP_VLA 0x40004
#define AST_OP_VLA_RESTORE 0x40005

static int ast_jit_body_has_vla(void) { MCC_TRACE("enter\n");
	AstArena *a = ast_cur;
	AstLocal n, nn;
	if (!a)
		{ MCC_TRACE("br\n"); return 0; }
	nn = ast_count(a);
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) == AST_Unary &&
				(ast_op(a, n) == AST_OP_VLA || ast_op(a, n) == AST_OP_VLA_RESTORE))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int ast_body_uses_func_alloca(void) { MCC_TRACE("enter\n");
	AstArena *a = ast_cur;
	AstLocal n, nn;
	if (!a)
		{ MCC_TRACE("br\n"); return 0; }
	nn = ast_count(a);
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) == AST_Unary &&
				(ast_op(a, n) == AST_OP_VLA || ast_op(a, n) == AST_OP_VLA_RESTORE))
			{ MCC_TRACE("br\n"); return 1; }
		if (ast_kind(a, n) == AST_Invoke) { MCC_TRACE("br\n");
			AstLocal ce = ast_first_child(a, n);
			if (ce != AST_NONE && ast_kind(a, ce) == AST_Ref) { MCC_TRACE("br\n");
				Sym *cs = (Sym *)(uintptr_t)ast_sym(a, ce);
				if (cs && (cs->v == TOK_alloca || cs->asm_label == TOK_alloca))
					{ MCC_TRACE("br\n"); return 1; }
			}
		}
	}
	return 0;
}
#define AST_OP_MULHU 0x40006
#define AST_OP_MULHS 0x40007
#define AST_OP_FABS 0x40008
#define AST_OP_SQRT 0x40009
#define AST_OP_OPASSIGN 0x4000A
#define AST_OP_FLOOR 0x4000B
#define AST_OP_CEIL  0x4000C
#define AST_OP_TRUNC 0x4000D
#define AST_OP_COPYSIGN 0x4000E
#define AST_OP_ROUND 0x4000F
#define AST_OP_FMIN 0x40010
#define AST_OP_FMAX 0x40011
#define AST_OP_RINT 0x40012
#define AST_OP_NEARBYINT 0x40013
#define AST_OP_FMA 0x40014
#define AST_OP_FNEG 0x40015
#define AST_OP_BSWAP 0x40016
#define AST_OP_ROTL 0x40023
#define AST_OP_ROTR 0x40024
#define AST_OP_SIGNBIT 0x40017
#define AST_OP_FFS 0x40018
#define AST_OP_BITSCAN 0x40019
#define AST_OP_AXADD 0x4001c
#define AST_OP_AXCHG 0x4001d
#define AST_OP_ACMPXCHG 0x4001e
#define AST_OP_BITB 0x4001f
#define AST_OP_ACASRMW 0x40020
#define AST_OP_GGOTO 0x40021
#define AST_OP_CPLXBUILD 0x40022
#define AST_OP_VAARG 0x40023
#define AST_OP_VASTART 0x40024
static int ast_bad_type(int tt);
static uint64_t ast_sv_hi(const SValue *sv);

static Sym **ast_sym_deferred;
static int ast_sym_deferred_n, ast_sym_deferred_cap;
static Sym **ast_sym_retained;
static int ast_sym_retained_n, ast_sym_retained_cap;
static int ast_sym_defer_on;

int ast_sym_defer(Sym *sym) { MCC_TRACE("enter\n");
	if (!ast_sym_defer_on)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_sym_deferred_n == ast_sym_deferred_cap) { MCC_TRACE("br\n");
		int nc = ast_sym_deferred_cap ? ast_sym_deferred_cap * 2 : 64;
		ast_sym_deferred = mcc_realloc(ast_sym_deferred, nc * sizeof(*ast_sym_deferred));
		ast_sym_deferred_cap = nc;
	}
	ast_sym_deferred[ast_sym_deferred_n++] = sym;
	return 1;
}

static int ast_reemit_n;
static int ast_inline_n;
static void ast_inline_index_reset(void);

static MCC_OPT_TLS unsigned char ast_opt_user_off[MCC_OPT_COUNT];

static int ast_opt_user_disabled(int id) { MCC_TRACE("enter\n");
	return id >= 0 && id < MCC_OPT_COUNT && ast_opt_user_off[id];
}

static void ast_opt_defaults(MCCState *s1) { MCC_TRACE("enter\n");
	if (s1->jit_always_gpu)
		{ MCC_TRACE("br\n"); ast_ladder_gpu_force(); }
	ast_ladder_gpu_setup();
	int o4 = s1->optimize_search_all != 0;
	int i, dflt[MCC_OPT_COUNT];
	int n = 0;
#define MCC_OPT_ROW(id, name, d) dflt[n++] = (d);
	MCC_OPT_LIST(MCC_OPT_ROW)
#undef MCC_OPT_ROW
	for (i = 0; i < MCC_OPT_COUNT; i++)
		{ MCC_TRACE("br\n"); ast_opt_user_off[i] = (s1->optflag[i] == 0); }
	for (i = 0; i < MCC_OPT_COUNT; i++) { MCC_TRACE("br\n");
		int on, d;
		if (s1->optflag[i] != MCC_OPT_UNSET)
			{ MCC_TRACE("br\n"); continue; }
		if (MCC_OPTD_IS_DEV(dflt[i]) && !mcc_dev_enabled()) { MCC_TRACE("br\n");
			s1->optflag[i] = 0;
			continue;
		}
		d = MCC_OPTD_BASE(dflt[i]);
		if (d == MCC_OPTD_SPECIAL)
			{ MCC_TRACE("br\n"); continue; }
		if (MCC_OPTD_IS_LEVEL(d)) { MCC_TRACE("br\n");
			int want = MCC_OPTD_LEVEL_OF(d);
			if (want >= MCC_OPT_SEARCH_LEVEL)
				{ MCC_TRACE("br\n"); mcc_error("mccopt.h: a knob sits at -O%d, at or past "
																			 "MCC_OPT_SEARCH_LEVEL (%d); bump it",
																			 want, MCC_OPT_SEARCH_LEVEL); }
			on = o4 || s1->optimize_level >= want;
		} else { MCC_TRACE("br\n");
			on = (d == MCC_OPTD_ALWAYS);
		}
		s1->optflag[i] = (unsigned char)on;
	}
}

static int mcc_opt(MCCState *s1, int id) { MCC_TRACE("enter\n");
	unsigned char v = s1->optflag[id];
	return v != 0 && v != MCC_OPT_UNSET;
}

ST_FUNC int ast_math_errno_folds(MCCState *s1) { MCC_TRACE("enter\n");
	unsigned char v;
	if (!s1)
		{ MCC_TRACE("br\n"); return 0; }
	v = s1->optflag[MCC_OPT_BUILTIN_MATH_ERRNO];
	if (v != MCC_OPT_UNSET)
		{ MCC_TRACE("br\n"); return v != 0; }
	return s1->no_math_errno != 0;
}

static int switch_jt_default_on(MCCState *s1) { MCC_TRACE("enter\n");
	const char *e = getenv("MCC_SWITCH_JUMPTABLE");
	if (e && e[0])
		{ MCC_TRACE("br\n"); return e[0] != '0'; }
	return s1->optimize_search_all != 0;
}

void ast_configure(MCCState *s1) { MCC_TRACE("enter\n");
	int opt_promote = 0;
	mcc_isa_init(s1);
	int o4 = s1->optimize_search_all != 0;
	ast_reemit_n = 0;
	ast_inline_n = 0;
	ast_inline_index_reset();
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
	opt_promote = s1->optimize >= 2;
#endif
	ast_opt_defaults(s1);
#define MCC_OPT_SPECIAL(id, expr)                    \
	do {                                               \
		if (s1->optflag[id] == MCC_OPT_UNSET)            \
			{ s1->optflag[id] = (unsigned char)!!(expr); } \
	} while (0)
	MCC_OPT_SPECIAL(MCC_OPT_OPT_SEARCH, o4);
	MCC_OPT_SPECIAL(MCC_OPT_OPT_ROI, o4);
	MCC_OPT_SPECIAL(MCC_OPT_PROMOTE_LOCALS, o4 || opt_promote);
	MCC_OPT_SPECIAL(MCC_OPT_PROMOTE_ARROW,
									o4 || s1->optimize_size || s1->optimize >= 2);
	MCC_OPT_SPECIAL(MCC_OPT_PROMOTE_INCDEC,
									o4 || s1->optimize_size || s1->optimize >= 2);
#ifdef MCC_TARGET_X86_64
	MCC_OPT_SPECIAL(MCC_OPT_REG_DISP,
									o4 || s1->optimize_size || s1->optimize >= 1);
	MCC_OPT_SPECIAL(MCC_OPT_BUILTIN_ROUND, mcc_isa_has(s1, MCC_ISA_SSE41));
#else
	MCC_OPT_SPECIAL(MCC_OPT_REG_DISP, 0);
	MCC_OPT_SPECIAL(MCC_OPT_BUILTIN_ROUND, 0);
#endif
#ifdef MCC_TARGET_ARM64
	MCC_OPT_SPECIAL(MCC_OPT_BUILTIN_MINMAX, o4 || s1->optimize >= 1);
#else
	MCC_OPT_SPECIAL(MCC_OPT_BUILTIN_MINMAX, o4);
#endif
#if defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64)
	MCC_OPT_SPECIAL(MCC_OPT_BUILTIN_FMA, o4 || s1->optimize >= 1);
#else
	MCC_OPT_SPECIAL(MCC_OPT_BUILTIN_FMA, o4);
#endif
	MCC_OPT_SPECIAL(MCC_OPT_INLINE, s1->optimize >= 3 && !s1->optimize_size);
	MCC_OPT_SPECIAL(MCC_OPT_BUILTIN_MATH_ERRNO, ast_math_errno_folds(s1));
	MCC_OPT_SPECIAL(MCC_OPT_IVOPTS,
									o4 || (s1->optimize >= 1 && !s1->optimize_size));
	MCC_OPT_SPECIAL(MCC_OPT_SWITCH_JUMPTABLE, switch_jt_default_on(s1));
#undef MCC_OPT_SPECIAL
	ast_replay_env = s1->optimize >= 1 || s1->embed_jit ||
									 ast_env_int("MCC_FORCE_REPLAY", 0) ||
									 ast_env_int("MCC_RIR_FORCE", 0);
	ast_rir_nofb_env = !mcc_opt(s1, MCC_OPT_REPLAY_FALLBACK);
	ast_rir_nomat_env = !mcc_opt(s1, MCC_OPT_REPLAY_MATERIALIZE);
	ast_rir_noinv_env = !mcc_opt(s1, MCC_OPT_REPLAY_LANDOR_INVERT);
	ast_replay_dump = mcc_opt(s1, MCC_OPT_DUMP_REPLAY);
	ast_verify_diff = getenv("MCC_AST_VERIFY_DIFF");
	ast_templates_env = mcc_opt(s1, MCC_OPT_REEMIT_TEMPLATES);
	ast_cload_env = mcc_opt(s1, MCC_OPT_TREE_CONST_LOAD);
	ast_search_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH);
	ast_search_verbose_env = mcc_opt(s1, MCC_OPT_DUMP_OPT_SEARCH);
	ast_slice_env = mcc_opt(s1, MCC_OPT_OPT_SLICE);
	ast_search_emitsize_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH_EMIT_SIZE);
	ast_search_emitiso_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH_EMIT_ISO);
	ast_search_inline_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH_INLINE);
	ast_search_threads_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH_THREADS);
	ast_search_pthreads_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH_PTHREADS);
	ast_search_ordered_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH_ORDERED);
	ast_search_predict_env = (int)mcc_env_count(
			"MCC_SEARCH_PREDICT",
			(unsigned)(mcc_opt(s1, MCC_OPT_OPT_SEARCH_PREDICT) ? 1 : 0));
	ast_search_order_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH_ORDER);
	ast_search_fullset_env = mcc_opt(s1, MCC_OPT_OPT_SEARCH_FULLSET);
	ast_roi_env = mcc_opt(s1, MCC_OPT_OPT_ROI);
	ast_roi_dump = mcc_opt(s1, MCC_OPT_DUMP_OPT_ROI);
	ast_cycle_env = mcc_opt(s1, MCC_OPT_OPT_CYCLE);
	ast_search_walk_env = ast_search_walk_from_env();
	ast_strat_order_from_env();
	if (ast_strat_order_forced) { MCC_TRACE("br\n");
		char sq[AST_STRAT_COUNT_MAX * 4];
		ast_order_seq_str(ast_strat_order, ast_strat_order_n, sq);
		MCC_TRACE("strat order forced n=%d seq=%s\n", ast_strat_order_n, sq);
	}
	ast_search_ticks = s1->optimize_search_ticks;
	ast_promote_env = mcc_opt(s1, MCC_OPT_PROMOTE_LOCALS);
	ast_promo_arrow_env = mcc_opt(s1, MCC_OPT_PROMOTE_ARROW);
	ast_promo_incdec_env = mcc_opt(s1, MCC_OPT_PROMOTE_INCDEC);
	ast_chainstore_env = mcc_opt(s1, MCC_OPT_CHAIN_STORE);
	ast_storeval_constl_env = mcc_opt(s1, MCC_OPT_STOREVAL_CONSTL);
	ast_storeval_callstore_env = mcc_opt(s1, MCC_OPT_STOREVAL_CALLSTORE);
	ast_storeval_rot_env = mcc_opt(s1, MCC_OPT_STOREVAL_ROT);
	ast_storeval_calllast_env =
			mcc_opt(s1, MCC_OPT_STOREVAL_CALLLAST);
	ast_chainstore_live_env = mcc_opt(s1, MCC_OPT_CHAIN_STORE_LIVE);
	ast_chainstore_member_env = mcc_opt(s1, MCC_OPT_CHAIN_STORE_MEMBER);
	ast_storeval_call_env = mcc_opt(s1, MCC_OPT_STOREVAL_CALL);
	ast_storeval_callup_env = mcc_opt(s1, MCC_OPT_STOREVAL_CALLUP);
	ast_cmp_mat_env = mcc_opt(s1, MCC_OPT_REPLAY_CMP_MATERIALIZE);
	ast_while_comma_env = mcc_opt(s1, MCC_OPT_REPLAY_WHILE_COMMA);
	ast_loopcond_store_env = mcc_opt(s1, MCC_OPT_REPLAY_LOOPCOND_STORE);
	ast_indirect_call_env = mcc_opt(s1, MCC_OPT_REPLAY_INDIRECT_CALL);
	ast_promo_leaf_xmm_env = mcc_opt(s1, MCC_OPT_PROMOTE_LEAF_XMM);
	ast_cost_ops_env = mcc_opt(s1, MCC_OPT_DUMP_COST_OPS);
	ast_cost_spill_env = mcc_opt(s1, MCC_OPT_DUMP_COST_SPILL);
	ast_reloc_equiv_env = mcc_opt(s1, MCC_OPT_RELOC_EQUIV);
#ifdef MCC_TARGET_ARM64
	ast_fmov_imm_env = mcc_opt(s1, MCC_OPT_FMOV_IMM);
#else
	ast_fmov_imm_env = 0;
#endif
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
	ast_regdisp_env = mcc_opt(s1, MCC_OPT_REG_DISP);
#else
	ast_regdisp_env = 0;
#endif
#if defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE)
	ast_xmm_hi_env = mcc_opt(s1, MCC_OPT_XMM_HI);
	for (int hr = MCC_TREG_XMM8; hr <= MCC_TREG_XMM15; hr++) { MCC_TRACE("br\n");
		if (ast_xmm_hi_env)
			{ MCC_TRACE("br\n"); reg_classes[hr] |= MCC_RC_FLOAT; }
		else
			{ MCC_TRACE("br\n"); reg_classes[hr] &= ~MCC_RC_FLOAT; }
	}
#endif
	ast_promo_leaf_callee_env = mcc_opt(s1, MCC_OPT_PROMOTE_LEAF_CALLEE);
	ast_no_callful_env = !mcc_opt(s1, MCC_OPT_PROMOTE_ACROSS_CALLS);
	ast_inline_env = mcc_opt(s1, MCC_OPT_INLINE);
	{
		const char *lim = getenv("MCC_AST_INLINE_LIMIT");
		ast_graft_limit = lim ? atoi(lim) : -1;
		ast_graft_total = 0;
	}
	{
		const char *lim = getenv("MCC_AST_PROMOTE_LIMIT");
		ast_promo_limit = lim ? atoi(lim) : -1;
		ast_promo_total = 0;
	}
	{
		const char *lim = getenv("MCC_AST_OPT_LIMIT");
		ast_opt_limit = lim ? atoi(lim) : -1;
		ast_opt_total = 0;
	}
	ast_inline_node_limit = ast_env_int("MCC_AST_INLINE_NODES", 64);
	ast_graft_budget_max = ast_env_int("MCC_AST_GRAFT", 2048);
	{
		const char *dg = getenv("MCC_AST_INLINE_DIVGUARD");
		ast_inline_divguard = (dg && dg[0]) ? atoi(dg) : 1;
	}
	ast_cost_env = mcc_opt(s1, MCC_OPT_DUMP_COST);
	ast_sethi_env = mcc_opt(s1, MCC_OPT_SETHI_ULLMAN);
	ast_sethi_leaf_env = mcc_opt(s1, MCC_OPT_SETHI_ULLMAN_LEAF);
	ast_sethi_nary_env = mcc_opt(s1, MCC_OPT_SETHI_ULLMAN_NARY);
	ast_bitflag_env = mcc_opt(s1, MCC_OPT_TREE_SWITCH_CONVERSION);
	ast_bitflag_report_env = mcc_opt(s1, MCC_OPT_DUMP_BITFLAG);
	ast_bitflag_min = ast_env_int("MCC_AST_BITFLAG", 5);
	if (ast_bitflag_min < 3)
		{ MCC_TRACE("br\n"); ast_bitflag_min = 5; }
	ast_cprop_join_env = mcc_opt(s1, MCC_OPT_TREE_COPY_PROP);
	ast_narrow_env = mcc_opt(s1, MCC_OPT_NARROW);
	ast_trunc32_env = mcc_opt(s1, MCC_OPT_TRUNC32);
	ast_switch_expr_env = mcc_opt(s1, MCC_OPT_SWITCH_EXPR);
	ast_narrow_fix_env = mcc_opt(s1, MCC_OPT_NARROW_FIX);
	ast_narrow_c0_env = mcc_opt(s1, MCC_OPT_NARROW_CLASS0);
	ast_narrow_c1_env = mcc_opt(s1, MCC_OPT_NARROW_CLASS1);
	ast_narrow_c2_env = mcc_opt(s1, MCC_OPT_NARROW_CLASS2);
	ast_narrow_c3_env = mcc_opt(s1, MCC_OPT_NARROW_CLASS3);
	ast_narrow_elim_env = mcc_opt(s1, MCC_OPT_NARROW_ELIM);
	ast_sccp_fix_env = mcc_opt(s1, MCC_OPT_TREE_CCP_ITERATE);
	ast_ident_conv_env = mcc_opt(s1, MCC_OPT_IDENT_CONV);
	ast_ident_shift_env = mcc_opt(s1, MCC_OPT_IDENT_SHIFT);
	ast_ident_arith_env = mcc_opt(s1, MCC_OPT_IDENT_ARITH);
	ast_ident_bit_env = mcc_opt(s1, MCC_OPT_IDENT_BIT);
	ast_ident_rel_env = mcc_opt(s1, MCC_OPT_IDENT_REL);
	ast_ident_urange_env = mcc_opt(s1, MCC_OPT_IDENT_URANGE);
	ast_strict_overflow_env = !s1->wrapv && s1->optimize_level >= 1;
	ast_dse_call_env = mcc_opt(s1, MCC_OPT_TREE_DSE);
	ast_tco_ptr_env = mcc_opt(s1, MCC_OPT_OPTIMIZE_SIBLING_CALLS);
	ast_cse_comm_env = mcc_opt(s1, MCC_OPT_GCSE);
	ast_cse_comm_rel_env = mcc_opt(s1, MCC_OPT_GCSE_COMM_REL);
	ast_range_env = mcc_opt(s1, MCC_OPT_TREE_VRP);
	ast_divmagic_env = mcc_opt(s1, MCC_OPT_DIVMAGIC);
	ast_divrem_env = mcc_opt(s1, MCC_OPT_DIVREM_PAIRS);
	ast_abs_env = mcc_opt(s1, MCC_OPT_IF_CONVERSION_ABS);
	ast_select_env = mcc_opt(s1, MCC_OPT_IF_CONVERSION);
	ast_reassoc_env = mcc_opt(s1, MCC_OPT_TREE_REASSOC);
	ast_sra_env = mcc_opt(s1, MCC_OPT_TREE_SRA);
	ast_sroa_env = mcc_opt(s1, MCC_OPT_TREE_SROA);
	ast_sroa_param_env = mcc_opt(s1, MCC_OPT_TREE_SROA_PARAMS);
	ast_reassoc_assoc_env = mcc_opt(s1, MCC_OPT_REASSOC_ASSOC);
	ast_reassoc_shlshr_env = mcc_opt(s1, MCC_OPT_REASSOC_SHLSHR);
	ast_reassoc_shrshl_env = mcc_opt(s1, MCC_OPT_REASSOC_SHRSHL);
	ast_reassoc_muldist_env = mcc_opt(s1, MCC_OPT_REASSOC_MULDIST);
	ast_bfold_sqrt_env = mcc_opt(s1, MCC_OPT_BFOLD_SQRT) && !stdc_fenv_access(s1);
	ast_bfold_sign_env = mcc_opt(s1, MCC_OPT_BFOLD_SIGN);
	ast_bfold_round_env = mcc_opt(s1, MCC_OPT_BFOLD_ROUND);
	ast_bfold_minmax_env = mcc_opt(s1, MCC_OPT_BFOLD_MINMAX);
	ast_math_inline_env = mcc_opt(s1, MCC_OPT_BUILTIN_MATH);
	ast_fabs_inline_env = mcc_opt(s1, MCC_OPT_BUILTIN_MATH_FABS);
	ast_math_inline_prepass_env = mcc_opt(s1, MCC_OPT_BUILTIN_MATH_PREPASS);
	ast_round_inline_env = mcc_opt(s1, MCC_OPT_BUILTIN_ROUND);
	ast_copysign_env = mcc_opt(s1, MCC_OPT_BUILTIN_COPYSIGN);
	ast_minmax_inline_env = mcc_opt(s1, MCC_OPT_BUILTIN_MINMAX);
	ast_fma_env = mcc_opt(s1, MCC_OPT_BUILTIN_FMA);
	ast_no_math_errno = mcc_opt(s1, MCC_OPT_BUILTIN_MATH_ERRNO);
	ast_inline_pass_env = mcc_opt(s1, MCC_OPT_INLINE_FUNCTIONS);
	ast_interchange_env = mcc_opt(s1, MCC_OPT_LOOP_INTERCHANGE);
	ast_fusion_env = mcc_opt(s1, MCC_OPT_LOOP_FUSION);
	ast_tile_env = mcc_opt(s1, MCC_OPT_LOOP_BLOCK);
	ast_tile_size = ast_env_int("MCC_AST_TILE_SIZE", 32);
	if (ast_tile_size < 2)
		{ MCC_TRACE("br\n"); ast_tile_size = 32; }
	ast_vlat_env = mcc_opt(s1, MCC_OPT_LOOP_VLAT);
	ast_unroll_env = mcc_opt(s1, MCC_OPT_LOOP_UNROLL);
	ast_loopidiom_env = mcc_opt(s1, MCC_OPT_LOOP_IDIOM);
	ast_vectorize_env = mcc_opt(s1, MCC_OPT_SLP_VECTORIZE);
	ast_bswap_idiom_env = mcc_opt(s1, MCC_OPT_BSWAP_IDIOM);
	ast_rotate_idiom_env = mcc_opt(s1, MCC_OPT_ROTATE_IDIOM);
	ast_jit_env = s1 && !mccjit_recompiling &&
			(s1->embed_jit || s1->output_type == MCC_OUTPUT_MEMORY);
	ast_jit_splice_env = mcc_opt(s1, MCC_OPT_JIT_SPLICE);
	ast_jit_dispatch_env = ast_env_int("MCC_AST_JIT_DISPATCH",
			(ast_jit_env || mcc_env_on("MCC_JIT_SUBMIT_AOT")) ? 6 : 0);
	ast_zero_bss_env = mcc_opt(s1, MCC_OPT_ZERO_INITIALIZED_IN_BSS);
	ast_merge_strings_env = mcc_opt(s1, MCC_OPT_MERGE_CONSTANTS);
	ast_strpool_n = 0;
	ast_cse_window = ast_env_int("MCC_AST_CSE_WINDOW", 64);
	if (ast_cse_window < 1)
		{ MCC_TRACE("br\n"); ast_cse_window = 1; }
	if (ast_cse_window > AST_CSE_MAX)
		{ MCC_TRACE("br\n"); ast_cse_window = AST_CSE_MAX; }
	ast_cprop_window = ast_env_int("MCC_AST_CPROP_WINDOW", 128);
	if (ast_cprop_window < 1)
		{ MCC_TRACE("br\n"); ast_cprop_window = 1; }
	if (ast_cprop_window > AST_CPROP_MAX)
		{ MCC_TRACE("br\n"); ast_cprop_window = AST_CPROP_MAX; }
	ast_inline_depth_max = ast_env_int("MCC_AST_INLINE_DEPTH", 8);
	if (ast_inline_depth_max < 1)
		{ MCC_TRACE("br\n"); ast_inline_depth_max = 1; }
	if (ast_inline_depth_max > AST_INLINE_MAX_DEPTH)
		{ MCC_TRACE("br\n"); ast_inline_depth_max = AST_INLINE_MAX_DEPTH; }
	ast_tco_maxp = ast_env_int("MCC_AST_TCO_MAXP", 16);
	if (ast_tco_maxp < 1)
		{ MCC_TRACE("br\n"); ast_tco_maxp = 1; }
	if (ast_tco_maxp > AST_TCO_MAXP)
		{ MCC_TRACE("br\n"); ast_tco_maxp = AST_TCO_MAXP; }
	ast_cse_join_env = mcc_opt(s1, MCC_OPT_GCSE_JOIN);
	ast_call_window_env = mcc_opt(s1, MCC_OPT_CALL_WINDOW);
	ast_licm_temp_env = mcc_opt(s1, MCC_OPT_TREE_LOOP_IM);
	ast_ivsr_env = mcc_opt(s1, MCC_OPT_IVOPTS);
	ast_ivsr_ptr_env = mcc_opt(s1, MCC_OPT_IVOPTS_PTR);
	ast_pre_env = mcc_opt(s1, MCC_OPT_TREE_PRE);
	ast_loopnest_dump_env = mcc_opt(s1, MCC_OPT_DUMP_LOOPNEST);
	ast_loopdep_dump_env = mcc_opt(s1, MCC_OPT_DUMP_LOOPDEP);
	ast_dep_alias_oracle_env = mcc_opt(s1, MCC_OPT_DEP_ALIAS_ORACLE);
	ast_perfn_inproc_env = mcc_opt(s1, MCC_OPT_OPT_PERFN_INPROC);
	ast_argfwd_env = mcc_opt(s1, MCC_OPT_ARG_FORWARD);
	ast_color_env = mcc_opt(s1, MCC_OPT_REG_COLOR);
	ast_spill_share_env = mcc_opt(s1, MCC_OPT_SPILL_SHARE);
	ast_intention_acc = 0;
	ast_hash_out = getenv("MCC_AST_HASH_OUT");
	ast_rirproddump_cached = getenv("RIRPRODDUMP");
	ast_rvattr_cached = getenv("RVATTR");
	ast_reparent_dbg_cached = mcc_env_on("MCC_REPARENT_DBG") ? 1 : 0;
	ast_refcensus_path = getenv("MCC_AST_REFCENSUS");
	{
		const char *fp = getenv("MCC_AST_FRAMEPERT");
		ast_fpert_on = 0;
		ast_fpert_base = 0;
		ast_fpert_pad = 0;
		ast_fpert_scale = 1;
		if (fp && fp[0]) { MCC_TRACE("br\n");
			const char *c = strchr(fp, ':');
			ast_fpert_base = atoi(fp);
			if (c) { MCC_TRACE("br\n");
				const char *c2 = strchr(c + 1, ':');
				int sc = atoi(c + 1);
				if (sc > 0)
					{ MCC_TRACE("br\n"); ast_fpert_scale = sc; }
				if (c2)
					{ MCC_TRACE("br\n"); ast_fpert_pad = atoi(c2 + 1); }
			}
			ast_fpert_on = 1;
		}
		ast_fpert_dbg = getenv("MCC_AST_FRAMEPERT_DBG") != NULL;
	}
	ast_search_worker = mcc_env_on("MCC_SEARCH_WORKER");
	ast_search_floor_env = mcc_env_on("MCC_AST_SEARCH_FLOOR");
	ast_search_floor = ast_search_floor_env ? ast_search_gates_now() : 0;
	ast_slice_eligible_now = ast_search_searchable(ast_search_gates_now());
	ast_fncfg_parse();
	ast_jit_fns_parse(mcc_state ? mcc_state->jit_functions : 0);
}

static int ast_fconst_reuse_off;

static int ir_cap_fconst_take(int *out);
static void ir_cap_fconst_note(int c);
static void ir_cap_reset(void);
static int ir_cap_active;
static void ir_cap_gap(void);

int ast_ircap_suspend(void) { MCC_TRACE("enter\n");
	int prev = ir_cap_active;
	ir_cap_active = 0;
	return prev;
}
void ast_ircap_resume(int prev) { MCC_TRACE("enter\n"); ir_cap_active = prev; }

void ast_fconst_reuse_disable(int off) { MCC_TRACE("enter\n"); ast_fconst_reuse_off = off; }

int ast_fconst_reuse(int cplx, const unsigned char *key) { MCC_TRACE("enter\n");
	int jfc;
	{
		int rfc = rir_hook_fconst_reuse(cplx, key);
		if (rfc >= 0)
			{ MCC_TRACE("br\n"); return rfc; }
	}
	if (ir_cap_fconst_take(&jfc))
		{ MCC_TRACE("br\n"); return jfc; }
	if (ast_replaying && !ast_fconst_reuse_off) { MCC_TRACE("br\n");
		while (ast_fconst_i < ast_fconst_n &&
					 ast_fconst_cplx[ast_fconst_i] != (unsigned char)cplx)
			{ MCC_TRACE("br\n"); ast_fconst_i++; }
		if (ast_fconst_i < ast_fconst_n &&
				!memcmp(ast_fconst_key[ast_fconst_i], key, AST_FCONST_KEY))
			{ MCC_TRACE("br\n"); return ast_fconst[ast_fconst_i++]; }
	}
	return 0;
}
void ast_fconst_record(int c, int cplx, const unsigned char *key) { MCC_TRACE("enter\n");
	ir_cap_fconst_note(c);
	rir_hook_fconst_record(c, cplx, key);
	if (!ast_active || ast_replaying)
		{ MCC_TRACE("br\n"); return; }
	if (!c)
		{ MCC_TRACE("br\n"); return; }
	if (ast_fconst_n == ast_fconst_cap) { MCC_TRACE("br\n");
		ast_fconst_cap = ast_fconst_cap ? ast_fconst_cap * 2 : 16;
		ast_fconst = mcc_realloc(ast_fconst, ast_fconst_cap * sizeof *ast_fconst);
		ast_fconst_cplx = mcc_realloc(ast_fconst_cplx,
																	ast_fconst_cap * sizeof *ast_fconst_cplx);
		ast_fconst_key = mcc_realloc(ast_fconst_key,
																 ast_fconst_cap * sizeof *ast_fconst_key);
	}
	ast_fconst_cplx[ast_fconst_n] = (unsigned char)cplx;
	memcpy(ast_fconst_key[ast_fconst_n], key, AST_FCONST_KEY);
	ast_fconst[ast_fconst_n++] = c;
}
void ast_fconst_push_ref(CType *type, int fc) { MCC_TRACE("enter\n");
	Sym *fs = sym_push(anon_sym++, type, VT_CONST | VT_SYM, fc);
	fs->type.t |= VT_STATIC;
	vpushsym(type, fs);
	vtop->r |= VT_LVAL;
}


static void ast_finalize_leaf(AstLocal n, SValue *sv) { MCC_TRACE("enter\n");
	uint16_t k = ast_kind(ast_cur, n);
	if (k != AST_Literal && k != AST_Ref)
		{ MCC_TRACE("br\n"); return; }
	ast_set_op(ast_cur, n, sv->r);
	ast_set_type_bf(ast_cur, n, sv->type.t, (uint64_t)(uintptr_t)sv->type.ref, sv->type.bp, sv->type.bs);
	ast_set_ival(ast_cur, n, (uint64_t)sv->c.i);
	ast_set_wide(ast_cur, n, ast_sv_hi(sv),
							 sv->r2 >= VT_CONST ? (unsigned)VT_CONST : (unsigned)sv->r2);
	ast_set_sym(ast_cur, n, (uint64_t)(uintptr_t)sv->sym);
	if (((sv->r & VT_VALMASK) == VT_CONST && (sv->r & VT_SYM) && sv->sym && sv->sym->sym_scope) ||
			(sv->type.ref && (sv->type.t & VT_BTYPE) == VT_STRUCT && sym_scope_ex(sv->type.ref)))
		{ MCC_TRACE("br\n"); ast_reemit_poison = 1; }
}

static int ast_op_modeled(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case TOK_SHL:
	case TOK_SAR:
	case TOK_SHR:
	case TOK_LT:
	case TOK_GT:
	case TOK_LE:
	case TOK_GE:
	case TOK_EQ:
	case TOK_NE:
	case TOK_ULT:
	case TOK_UGE:
	case TOK_ULE:
	case TOK_UGT:
		return 1;
	default:
		return 0;
	}
}

static int ast_bad_type(int tt) { MCC_TRACE("enter\n");
	int bt = tt & VT_BTYPE;
	return bt == VT_STRUCT || (tt & VT_BITFIELD) ||
				 bt == VT_LDOUBLE || bt == VT_QFLOAT || bt == VT_INT128;
}

typedef char ast_r2_none_check[AST_R2_NONE == VT_CONST ? 1 : -1];

static uint64_t ast_sv_hi(const SValue *sv) { MCC_TRACE("enter\n");
	int bt = sv->type.t & VT_BTYPE;
	if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	if (sv->type.t & VT_BITFIELD)
		{ MCC_TRACE("br\n"); return 0; }
	if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
		if (sizeof(long double) <= 8)
			{ MCC_TRACE("br\n"); return 0; }
		if (LDBL_MANT_DIG == 64)
			{ MCC_TRACE("br\n"); return sv->c.q.hi & 0xffffu; }
		return sv->c.q.hi;
	}
	if (bt == VT_INT128 || bt == VT_QLONG || bt == VT_QFLOAT)
		{ MCC_TRACE("br\n"); return sv->c.q.hi; }
	return 0;
}

static int ast_cmp_invert_late(AstArena *a, AstLocal n, int op) { MCC_TRACE("enter\n");
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
	uint32_t k;
	if (op != TOK_LT && op != TOK_GE && op != TOK_LE && op != TOK_GT)
		return 0;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		return 0;
	for (k = 0; k < 2; k++)
		if (is_float(ast_type_t(a, ast_child(a, n, k))))
			return 1;
	return 0;
#else
	(void)a, (void)n, (void)op;
	return 0;
#endif
}

static AstLocal ast_while_prefix = AST_NONE;
static AstLocal ast_while_savebb = AST_NONE;

static AstLocal ast_for_prefix_pending = AST_NONE;

#define AST_LBLMAP_MAX 512
static void *ast_lblmap_sym[AST_LBLMAP_MAX];
static int ast_lblmap_id[AST_LBLMAP_MAX];
static int ast_lblmap_n;
static int ast_lblseq;

int ast_label_id(void *s) { MCC_TRACE("enter\n");
	int i;
	if (!s)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < ast_lblmap_n; i++)
		{ MCC_TRACE("br\n"); if (ast_lblmap_sym[i] == s)
			{ MCC_TRACE("br\n"); return ast_lblmap_id[i]; } }
	if (ast_lblmap_n >= AST_LBLMAP_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	ast_lblmap_sym[ast_lblmap_n] = s;
	ast_lblmap_id[ast_lblmap_n] = ++ast_lblseq;
	return ast_lblmap_id[ast_lblmap_n++];
}

void ast_label_forget(void *s) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < ast_lblmap_n; i++)
		{ MCC_TRACE("br\n"); if (ast_lblmap_sym[i] == s) { MCC_TRACE("br\n");
			ast_lblmap_sym[i] = ast_lblmap_sym[ast_lblmap_n - 1];
			ast_lblmap_id[i] = ast_lblmap_id[ast_lblmap_n - 1];
			ast_lblmap_n--;
			return;
		} }
}

static AstLocal ast_cleanup_localref(AstArena *a, int off, int tt,
																		 uint64_t tref) { MCC_TRACE("enter\n");
	AstLocal r = ast_node(a, AST_Ref);
	ast_set_op(a, r, VT_LOCAL | VT_LVAL);
	ast_set_ival(a, r, (uint64_t)off);
	ast_set_type(a, r, tt, tref);
	return r;
}

#if (defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64))
#define AST_PROMO_MAX 5
#if defined(MCC_TARGET_RISCV64)
#define AST_PROMO_CALLER_N 0
#define AST_PROMO_CALLEE_N 11
#define AST_PROMO_XMM_N 0
static const int ast_promo_caller[1] = {0};
static const int ast_promo_callee[AST_PROMO_CALLEE_N] = {19, 20, 21, 22, 23,
																												 24, 25, 26, 27, 28, 29};
static const int ast_promo_xmm[1] = {0};
#define AST_PROMO_XMM_LEAF_N AST_PROMO_XMM_N
#define ast_promo_xmm_leaf ast_promo_xmm
#elif defined(MCC_TARGET_X86_64)
#define AST_PROMO_CALLER_N 3
#define AST_PROMO_CALLEE_N 5
#define AST_PROMO_XMM_N 2
static const int ast_promo_caller[AST_PROMO_CALLER_N] = {10, 9, 8};
static const int ast_promo_callee[AST_PROMO_CALLEE_N] = {3, 12, 13, 14, 15};
#ifdef MCC_TARGET_PE
static const int ast_promo_xmm[AST_PROMO_XMM_N] = {21, 20};
#define AST_PROMO_XMM_LEAF_N 4
static const int ast_promo_xmm_leaf[AST_PROMO_XMM_LEAF_N] = {21, 20, 19, 18};
#else
static const int ast_promo_xmm[AST_PROMO_XMM_N] = {22, 23};
#define AST_PROMO_XMM_LEAF_N 14
static const int ast_promo_xmm_leaf[AST_PROMO_XMM_LEAF_N] = {24, 25, 26, 27, 28, 29, 30, 31,
			22, 23, 21, 20, 19, 18};
#endif
#else
#define AST_PROMO_CALLER_N 7
#define AST_PROMO_CALLEE_N 10
#define AST_PROMO_XMM_N 4
static const int ast_promo_caller[AST_PROMO_CALLER_N] = {9, 10, 11, 12, 13, 14, 15};
static const int ast_promo_callee[AST_PROMO_CALLEE_N] = {28, 29, 30, 31, 32,
																												 33, 34, 35, 36, 37};
static const int ast_promo_xmm[AST_PROMO_XMM_N] = {22, 23, 24, 25};
#define AST_PROMO_XMM_LEAF_N AST_PROMO_XMM_N
#define ast_promo_xmm_leaf ast_promo_xmm
#endif
#define AST_PROMO_SLOTS (AST_PROMO_MAX * 8)
static int ast_promo_off[AST_PROMO_SLOTS];
static int ast_promo_typ[AST_PROMO_SLOTS];
static int ast_promo_reg[AST_PROMO_SLOTS];
static int ast_promo_n;
static int ast_promo_callful;
static int ast_promo_leaf_pool[AST_PROMO_CALLER_N + AST_PROMO_CALLEE_N];
static int ast_promo_reg_is_callee(int reg) { MCC_TRACE("enter\n");
	for (int i = 0; i < AST_PROMO_CALLEE_N; i++)
		{ MCC_TRACE("br\n"); if (ast_promo_callee[i] == reg) { MCC_TRACE("br\n"); return 1; } }
	return 0;
}
static int ast_promo_regpool_at(int i) { MCC_TRACE("enter\n");
	return ast_promo_reg[i];
}
#endif

static unsigned char *ir_cap_raw;
static SValue *ir_cap_vs;
static void ast_replay_value(AstArena *a, AstLocal n);
static void ast_replay_bb(AstArena *a, AstLocal bb);
static int ast_local_is_readonly(AstArena *a, int off);
static int ast_cprop_escapes(AstArena *a, int off);
static int ast_ref_is_local_off(AstArena *a, AstLocal n, int off);
static int ast_argfwd_read_count(AstArena *a, int off) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	int cnt = 0;
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_Ref && ast_ref_is_local_off(a, n, off))
			{ MCC_TRACE("br\n"); cnt++; } }
	return cnt;
}
#define AST_INLINE_MAX 512
#define AST_INLINE_MAX_PARAMS 6
struct AstInlineFn {
	void *sym;
	AstArena *ast;
	int frame_size;
	int nparams;
	int param_off[AST_INLINE_MAX_PARAMS];
	int param_typ[AST_INLINE_MAX_PARAMS];
	void *param_ref[AST_INLINE_MAX_PARAMS];
	int param_size[AST_INLINE_MAX_PARAMS];
	int param_stack[AST_INLINE_MAX_PARAMS];
	int param_indirect[AST_INLINE_MAX_PARAMS];
	int graftable;
};
static struct AstInlineFn *ast_inline_pool;
static int ast_inline_pool_cap;
static int ast_inline_hi;
static int ast_inline_n;

static int ast_body_ind_sv;
static addr_t ast_reloc0_sv;

static struct AstReemitFn {
	Sym *sym;
	AstArena *ast;
	int inline_n_at_gen;
	int body_ind;
	int body_len;
	addr_t reloc0;
	int rel_len;
} ast_reemit_pool[AST_INLINE_MAX];
static int ast_reemit_n;

static int ast_inline_cap_np;
static int ast_inline_cap_off[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_typ[AST_INLINE_MAX_PARAMS];
static void *ast_inline_cap_ref[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_size[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_stack[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_ind[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_ok;

static int *ast_inline_hash;
static int ast_inline_hash_cap;

static void ast_inline_index_reset(void) { MCC_TRACE("enter\n");
	if (ast_inline_hash)
		{ MCC_TRACE("br\n"); memset(ast_inline_hash, 0,
															 (size_t)ast_inline_hash_cap * sizeof(int)); }
}

static int ast_inline_hash_slot(void *sym, int cap) { MCC_TRACE("enter\n");
	uintptr_t h = (uintptr_t)sym >> 4;
	h *= (uintptr_t)0x9e3779b97f4a7c15ull;
	return (int)((h >> 32) & (uintptr_t)(cap - 1));
}

static int ast_inline_index(void *sym) { MCC_TRACE("enter\n");
	int s;
	if (!ast_inline_hash_cap)
		{ MCC_TRACE("br\n"); return -1; }
	s = ast_inline_hash_slot(sym, ast_inline_hash_cap);
	for (;;) { MCC_TRACE("br\n");
		int v = ast_inline_hash[s];
		if (!v)
			{ MCC_TRACE("br\n"); return -1; }
		if (ast_inline_pool[v - 1].sym == sym)
			{ MCC_TRACE("br\n"); return v - 1; }
		s = (s + 1) & (ast_inline_hash_cap - 1);
	}
}

static void ast_inline_index_add(int i) { MCC_TRACE("enter\n");
	int s = ast_inline_hash_slot(ast_inline_pool[i].sym, ast_inline_hash_cap);
	while (ast_inline_hash[s])
		{ MCC_TRACE("br\n"); s = (s + 1) & (ast_inline_hash_cap - 1); }
	ast_inline_hash[s] = i + 1;
}

static int ast_inline_grow(void) { MCC_TRACE("enter\n");
	int i;
	if (ast_inline_n == ast_inline_pool_cap) { MCC_TRACE("br\n");
		int nc = ast_inline_pool_cap ? ast_inline_pool_cap * 2 : 128;
		struct AstInlineFn *np =
				mcc_realloc(ast_inline_pool, (size_t)nc * sizeof(*ast_inline_pool));
		if (!np)
			{ MCC_TRACE("br\n"); return 0; }
		ast_inline_pool = np;
		ast_inline_pool_cap = nc;
	}
	if ((ast_inline_n + 1) * 2 > ast_inline_hash_cap) { MCC_TRACE("br\n");
		int nc = ast_inline_hash_cap ? ast_inline_hash_cap * 2 : 512;
		int *nh = mcc_realloc(ast_inline_hash, (size_t)nc * sizeof(int));
		if (!nh)
			{ MCC_TRACE("br\n"); return 0; }
		ast_inline_hash = nh;
		ast_inline_hash_cap = nc;
		memset(ast_inline_hash, 0, (size_t)nc * sizeof(int));
		for (i = 0; i < ast_inline_n; i++)
			{ MCC_TRACE("br\n"); ast_inline_index_add(i); }
	}
	return 1;
}

static struct AstInlineFn *ast_inline_find(void *sym) { MCC_TRACE("enter\n");
	int i = ast_inline_index(sym);
	return i < 0 ? NULL : &ast_inline_pool[i];
}

static AstArena *ast_inline_lookup(void *sym) { MCC_TRACE("enter\n");
	struct AstInlineFn *e = ast_inline_find(sym);
	return e ? e->ast : NULL;
}

/* Does this body call setjmp?
 *
 * A setjmp call can return twice, and the second return arrives on an edge no
 * pass in this file models: control re-enters the middle of the function from
 * an arbitrary deeper frame. Two consequences, both observed as wrong answers
 * on vendor/gcc-c-torture-execute/pr60003.c and both fixed by consulting this:
 *
 *   - a local promoted into a callee-saved register is restored to its
 *     setjmp-time value by longjmp, losing every assignment made in between
 *     (the promotion pool IS the callee-saved set longjmp restores);
 *   - copy propagation forwards the value a local held BEFORE the call into
 *     uses after it, which the abnormal edge invalidates.
 *
 * Matching by name is the same hack ast_fn_inlinable() already uses two
 * functions up, and inherits its limits: it fires on any callee whose name
 * contains "setjmp" and misses one reached through a function pointer. It
 * covers setjmp, _setjmp, sigsetjmp, __sigsetjmp and __mcc_setjmp, which is
 * every spelling this tree can currently produce.
 */
static int ast_body_has_setjmp(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Invoke)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal ce = ast_first_child(a, n);
		void *cs = (ce != AST_NONE && ast_kind(a, ce) == AST_Ref)
									 ? (void *)(uintptr_t)ast_sym(a, ce)
									 : NULL;
		if (cs) { MCC_TRACE("br\n");
			const char *cn = get_tok_str(((Sym *)cs)->v, NULL);
			if (cn && strstr(cn, "setjmp"))
				{ MCC_TRACE("br\n"); return 1; }
		}
	}
	return 0;
}

static int ast_fn_inlinable(AstArena *a, Sym *sym) { MCC_TRACE("enter\n");
	if (!ast_inline_env && !ast_inline_pass_env)
		{ MCC_TRACE("br\n"); return 0; }
	if (!(sym->type.t & VT_STATIC) &&
			!(mcc_state->c99_inline_body && sym->a.weak))
		{ MCC_TRACE("br\n"); return 0; }
	if (sym->type.ref->f.func_type != FUNC_NEW)
		{ MCC_TRACE("br\n"); return 0; }
	if (sym->type.ref->f.func_noinl)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal nn = ast_count(a);
	if (nn == 0 || nn > ast_inline_node_limit)
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, n);
		if (k == AST_Unary &&
				(ast_op(a, n) == AST_OP_VLA || ast_op(a, n) == AST_OP_VLA_RESTORE))
			{ MCC_TRACE("br\n"); return 0; }
		if (k == AST_Invoke) { MCC_TRACE("br\n");
			AstLocal ce = ast_first_child(a, n);
			void *cs = (ce != AST_NONE && ast_kind(a, ce) == AST_Ref)
										 ? (void *)(uintptr_t)ast_sym(a, ce)
										 : NULL;
			if (cs) { MCC_TRACE("br\n");
				const char *cn = get_tok_str(((Sym *)cs)->v, NULL);
				if (cn && strstr(cn, "setjmp"))
					{ MCC_TRACE("br\n"); return 0; }
			}
		}
	}
	return 1;
}

static void *ast_slice_self_sym;

static void ast_inline_capture(Sym *fnsym) { MCC_TRACE("enter\n");
	ast_inline_cap_np = 0;
	ast_inline_cap_ok = 0;
	if (!ast_inline_env && !ast_inline_pass_env)
		{ MCC_TRACE("br\n"); return; }
	int n = 0;
	for (Sym *p = fnsym->type.ref->next; p; p = p->next) { MCC_TRACE("br\n");
		int v = p->v & ~SYM_FIELD;
		if (v < TOK_IDENT || n >= AST_INLINE_MAX_PARAMS)
			{ MCC_TRACE("br\n"); return; }
		Sym *ls = sym_find(v);
		if (!ls)
			{ MCC_TRACE("br\n"); return; }
		int bt = ls->type.t & VT_BTYPE;
		int indirect = 0;
		if ((ls->r & VT_VALMASK) == VT_LLOCAL && (ls->r & VT_LVAL) &&
				bt == VT_STRUCT) { MCC_TRACE("br\n");
			indirect = 1;
		} else if ((ls->r & VT_VALMASK) != VT_LOCAL) { MCC_TRACE("br\n");
			return;
		}
		if ((bt != VT_INT && bt != VT_LLONG && bt != VT_PTR &&
				 bt != VT_FLOAT && bt != VT_DOUBLE && bt != VT_STRUCT) ||
				(ls->type.t & (VT_ARRAY | VT_VLA)))
			{ MCC_TRACE("br\n"); return; }
		if (bt == VT_STRUCT && is_complex_type(&ls->type))
			{ MCC_TRACE("br\n"); return; }
		if (bt == VT_PTR && ls->type.ref &&
				((((Sym *)ls->type.ref)->type.t & VT_VLA) ||
				 ((Sym *)ls->type.ref)->vla_array_str))
			{ MCC_TRACE("br\n"); return; }
		int palign, psize = type_size(&ls->type, &palign);
		if (psize < 0)
			{ MCC_TRACE("br\n"); return; }
		ast_inline_cap_off[n] = (int)ls->c;
		ast_inline_cap_typ[n] = ls->type.t;
		ast_inline_cap_ref[n] = ls->type.ref;
		ast_inline_cap_size[n] = psize;
		ast_inline_cap_stack[n] = (int)ls->c >= 0;
		ast_inline_cap_ind[n] = indirect;
		n++;
	}
	ast_inline_cap_np = n;
	ast_inline_cap_ok = 1;
}

int ast_arena_has_asm(const AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_op_is_asm(ast_op(a, n)))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

typedef struct AstAsmEff {
	uint64_t reads;
	uint64_t writes;
	uint64_t clobbers;
	int eff;
	int nlabels;
	int unknown;
} AstAsmEff;

static int ast_asm_eff_node(const AstArena *a, AstLocal n, AstAsmEff *e) { MCC_TRACE("enter\n");
	const unsigned char *p;
	int hdr[4], nall, i, out_reg;
	if (ast_op(a, n) != AST_OP_ASMGEN)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ir_cap_raw)
		{ MCC_TRACE("br\n"); e->unknown = 1; return 1; }
	p = ir_cap_raw + (int)(ast_ival(a, n) & 0xffffffffu);
	memcpy(hdr, p, sizeof hdr);
	p += sizeof hdr;
	if (hdr[0] < 0 || hdr[2] < 0 || hdr[0] + hdr[2] > MAX_ASM_OPERANDS ||
			hdr[1] < 0 || hdr[1] > hdr[0])
		{ MCC_TRACE("br\n"); e->unknown = 1; return 1; }
	nall = hdr[0] + hdr[2];
	for (i = 0; i < hdr[0]; i++) { MCC_TRACE("br\n");
		ASMOperand op;
		memcpy(&op, p + (size_t)i * sizeof op, sizeof op);
		if (op.is_memory)
			{ MCC_TRACE("br\n"); e->eff |= MCC_ASM_EFF_MEM; }
		if (op.reg >= 0 && op.reg < 64) { MCC_TRACE("br\n");
			if (i < hdr[1]) { MCC_TRACE("br\n");
				e->writes |= (uint64_t)1 << op.reg;
				if (op.is_rw)
					{ MCC_TRACE("br\n"); e->reads |= (uint64_t)1 << op.reg; }
			} else { MCC_TRACE("br\n");
				e->reads |= (uint64_t)1 << op.reg;
			}
		}
	}
	p += (size_t)nall * sizeof(ASMOperand);
	for (i = 0; i < MCC_NB_ASM_REGS && i < 64; i++)
		{ MCC_TRACE("br\n"); if (p[i]) { MCC_TRACE("br\n"); e->clobbers |= (uint64_t)1 << i; } }
	out_reg = (int)(uint32_t)(ast_sym(a, n) >> 32);
	if (out_reg >= 0 && out_reg < 64)
		{ MCC_TRACE("br\n"); e->clobbers |= (uint64_t)1 << out_reg; }
	e->eff |= hdr[3];
	if (hdr[2] > e->nlabels)
		{ MCC_TRACE("br\n"); e->nlabels = hdr[2]; }
	return 1;
}

static int ast_body_asm_eff(const AstArena *a, AstAsmEff *e) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n;
	int any = 0, texts = 0, gens = 0;
	memset(e, 0, sizeof *e);
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int op = ast_op(a, n);
		if (op == AST_OP_ASM)
			{ MCC_TRACE("br\n"); texts++; }
		if (ast_asm_eff_node(a, n, e))
			{ MCC_TRACE("br\n"); any = 1; gens++; }
	}
	if (texts && !gens)
		{ MCC_TRACE("br\n"); e->unknown = 1; any = 1; }
	return any;
}

static int ast_asm_eff_is_fence(const AstAsmEff *e) { MCC_TRACE("enter\n");
	return e->unknown || (e->eff & (MCC_ASM_EFF_MEM | MCC_ASM_EFF_GOTO));
}

static int ast_node_is_asm(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return ast_op_is_asm(ast_op(a, n));
}

static int ast_fn_asm_live;

static int ast_ref_reg_dangle(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int r;
	if (ast_kind(a, n) != AST_Ref || ast_nchild(a, n) != 0)
		{ MCC_TRACE("br\n"); return 0; }
	r = ast_op(a, n);
	if (!(r & VT_LVAL) || (r & VT_VALMASK) >= VT_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_parent(a, n) != AST_NONE;
}

static int ast_arena_has_hole(const AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int op = ast_op(a, n);
		if (op == AST_OP_ASM || op == AST_OP_ASMGEN || op == AST_OP_ASMOPS)
			{ MCC_TRACE("br\n"); return 1; }
		if (ast_ref_reg_dangle(a, n))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int ast_arena_has_dangle(const AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_ref_reg_dangle(a, n))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

enum {
	AST_LOW_OK = 0,
	AST_LOW_ASM,
	AST_LOW_REG,
	AST_LOW_OPAQUE,
	AST_LOW_CALL,
	AST_LOW_TYPE,
	AST_LOW_FRAME,
	AST_LOW_GLOBAL,
	AST_LOW_NCLASS
};

typedef char ast_low_nclass_check[AST_LOW_NCLASS == RIR_LOW_NCLASS ? 1 : -1];

static const char *const ast_low_class_name[AST_LOW_NCLASS] = {
		"ok", "asm", "reg", "opaque", "call", "type", "frame", "global"};

static int ast_low_ptrish(int t) { MCC_TRACE("enter\n");
	int bt = t & VT_BTYPE;
	return bt == VT_PTR || bt == VT_FUNC || (t & VT_ARRAY) != 0;
}

static int ast_low_scalar(int t) { MCC_TRACE("enter\n");
	int bt = t & VT_BTYPE;
	if (ast_bad_type(t) || (t & (VT_ARRAY | VT_VLA)))
		{ MCC_TRACE("br\n"); return 0; }
	return bt == VT_BOOL || bt == VT_BYTE || bt == VT_SHORT || bt == VT_INT ||
				 bt == VT_LLONG || bt == VT_PTR || bt == VT_FLOAT || bt == VT_DOUBLE ||
				 bt == VT_FLOAT16 || bt == VT_BF16;
}

static int ast_low_base_ptr(const AstArena *a, AstLocal n, int depth) { MCC_TRACE("enter\n");
	uint16_t k;
	AstLocal c;
	int t;
	if (n == AST_NONE || depth > 8)
		{ MCC_TRACE("br\n"); return 0; }
	t = ast_stype_t(a, n);
	if (t)
		{ MCC_TRACE("br\n"); return ast_low_ptrish(t) || (t & VT_BTYPE) == VT_STRUCT; }
	k = ast_kind(a, n);
	if (k != AST_Binary && k != AST_Unary && k != AST_If && k != AST_Load &&
			k != AST_Convert)
		{ MCC_TRACE("br\n"); return 0; }
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_low_base_ptr(a, c, depth + 1))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_low_op_opaque(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case AST_OP_VLA:
	case AST_OP_VLA_RESTORE:
	case AST_OP_VAARG:
	case AST_OP_VASTART:
	case AST_OP_GGOTO:
	case AST_OP_AXADD:
	case AST_OP_AXCHG:
	case AST_OP_ACMPXCHG:
	case AST_OP_ACASRMW:
	case AST_OP_BITB:
	case AST_OP_CPLXBUILD:
	case AST_OP_IMAG:
		return 1;
	}
	return 0;
}

static int ast_low_node(const AstArena *a, AstLocal n, int pinned, int lvl) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), t = ast_stype_t(a, n);
	uint16_t k = ast_kind(a, n);
	AstLocal c;
	if (ast_op_is_asm(op))
		{ MCC_TRACE("br\n"); return AST_LOW_ASM; }
	if (k == AST_Poison)
		{ MCC_TRACE("br\n"); return AST_LOW_OPAQUE; }
	if (ast_low_op_opaque(op))
		{ MCC_TRACE("br\n"); return AST_LOW_OPAQUE; }
	if (k == AST_Invoke)
		{ MCC_TRACE("br\n"); return AST_LOW_CALL; }
	if (k == AST_Ref || k == AST_Literal) { MCC_TRACE("br\n");
		int v = op & VT_VALMASK;
		if (v < VT_CONST || v == VT_CMP || v == VT_JMP || v == VT_JMPI)
			{ MCC_TRACE("br\n"); return AST_LOW_REG; }
		if (v == VT_LLOCAL)
			{ MCC_TRACE("br\n"); return AST_LOW_FRAME; }
		if (op & VT_SYM)
			{ MCC_TRACE("br\n"); return AST_LOW_GLOBAL; }
		if (v == VT_LOCAL) { MCC_TRACE("br\n");
			if (lvl == 0)
				{ MCC_TRACE("br\n"); return AST_LOW_FRAME; }
			if (lvl == 1 && (pinned || !ast_low_scalar(t)))
				{ MCC_TRACE("br\n"); return AST_LOW_FRAME; }
		}
	}
	if (op == AST_OP_ADDR)
		{ MCC_TRACE("br\n"); return AST_LOW_FRAME; }
	if (ast_bad_type(t))
		{ MCC_TRACE("br\n"); return AST_LOW_TYPE; }
	if (k == AST_Convert && ast_first_child(a, n) == AST_NONE)
		{ MCC_TRACE("br\n"); return AST_LOW_TYPE; }
	if (k == AST_Load || k == AST_Store || op == AST_OP_MEMBER ||
			op == AST_OP_MEMBER_ARROW) { MCC_TRACE("br\n");
		c = ast_first_child(a, n);
		if (c == AST_NONE)
			{ MCC_TRACE("br\n"); return AST_LOW_TYPE; }
		if (ast_kind(a, c) != AST_Ref && !ast_low_base_ptr(a, c, 0))
			{ MCC_TRACE("br\n"); return AST_LOW_TYPE; }
	}
	return AST_LOW_OK;
}

static const char *ast_low_dump_fn;

static long ast_low_kn[AST_KIND_COUNT];
static long ast_low_ku[AST_KIND_COUNT];
static long ast_low_kv[AST_KIND_COUNT];

const long *ast_low_kind_n(void) { MCC_TRACE("enter\n"); return ast_low_kn; }
const long *ast_low_kind_untyped(void) { MCC_TRACE("enter\n"); return ast_low_ku; }
const long *ast_low_kind_void(void) { MCC_TRACE("enter\n"); return ast_low_kv; }

static int ast_low_walk(const AstArena *a, AstLocal n, int pinned, int depth,
												long *nodes, long *clean, long *why,
												unsigned char *cl, uint32_t *sz) { MCC_TRACE("enter\n");
	int ok[RIR_LOW_NLEVEL], me = AST_LOW_OK, i, bits = 0;
	uint32_t size = 1;
	AstLocal c;
	(*nodes)++;
	{ MCC_TRACE("br\n");
		uint16_t kk = ast_kind(a, n);
		if (kk < AST_KIND_COUNT) { MCC_TRACE("br\n");
			ast_low_kn[kk]++;
			if (!ast_stype_t(a, n)) { MCC_TRACE("br\n");
				if (ast_stype_known(a, n))
					{ MCC_TRACE("br\n"); ast_low_kv[kk]++; }
				else
					{ MCC_TRACE("br\n"); ast_low_ku[kk]++; }
			}
		}
	}
	for (i = 0; i < RIR_LOW_NLEVEL; i++) { MCC_TRACE("br\n");
		int r = ast_low_node(a, n, pinned, i);
		ok[i] = r == AST_LOW_OK;
		if (i == 1)
			{ MCC_TRACE("br\n"); me = r; why[r]++; }
	}
	if (ast_low_dump_fn) { MCC_TRACE("br\n");
		fprintf(stderr, "[rir-low-node] %*s%-11s op=%#x t=%#x -> %s\n", depth * 2, "",
						ast_kind_name(ast_kind(a, n)), (unsigned)ast_op(a, n),
						(unsigned)ast_stype_t(a, n), ast_low_class_name[me]);
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		int sub = ast_low_walk(a, c, pinned, depth + 1, nodes, clean, why, cl, sz);
		size += sz[c];
		for (i = 0; i < RIR_LOW_NLEVEL; i++)
			{ MCC_TRACE("br\n"); if (!((sub >> i) & 1))
				{ MCC_TRACE("br\n"); ok[i] = 0; } }
	}
	for (i = 0; i < RIR_LOW_NLEVEL; i++)
		{ MCC_TRACE("br\n"); if (ok[i])
			{ MCC_TRACE("br\n"); clean[i]++; bits |= 1 << i; } }
	sz[n] = size;
	cl[n] = (unsigned char)bits;
	return bits;
}

static void ast_low_census(const AstArena *a) { MCC_TRACE("enter\n");
	long nodes = 0, clean[RIR_LOW_NLEVEL], why[AST_LOW_NCLASS];
	long regions[RIR_LOW_NLEVEL], big[RIR_LOW_NLEVEL], huge[RIR_LOW_NLEVEL];
	unsigned char *cl;
	uint32_t *sz;
	AstLocal nn, n;
	int pinned = 0, i;
	const char *dm;
	if (!a)
		{ MCC_TRACE("br\n"); return; }
	for (i = 0; i < RIR_LOW_NLEVEL; i++)
		{ MCC_TRACE("br\n"); clean[i] = regions[i] = big[i] = huge[i] = 0; }
	for (i = 0; i < AST_LOW_NCLASS; i++)
		{ MCC_TRACE("br\n"); why[i] = 0; }
	nn = ast_count(a);
	if (!nn)
		{ MCC_TRACE("br\n"); return; }
	cl = mcc_mallocz((unsigned long)nn);
	sz = mcc_mallocz((unsigned long)nn * sizeof *sz);
	if (!cl || !sz) { MCC_TRACE("br\n");
		mcc_free(cl);
		mcc_free(sz);
		return;
	}
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int op = ast_op(a, n);
		if (op == AST_OP_ADDR ||
				(ast_kind(a, n) == AST_Ref && (op & VT_VALMASK) == VT_LLOCAL))
			{ MCC_TRACE("br\n"); pinned = 1; break; }
	}
	dm = getenv("MCC_RIR_LOW_DUMP");
	ast_low_dump_fn =
			(dm && funcname && (!strcmp(dm, "*") || !strcmp(dm, funcname))) ? funcname : NULL;
	if (ast_low_dump_fn)
		{ MCC_TRACE("br\n"); fprintf(stderr, "[rir-low-dump] %s pinned=%d\n", funcname, pinned); }
	for (n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_parent(a, n) == AST_NONE)
			{ MCC_TRACE("br\n"); ast_low_walk(a, n, pinned, 0, &nodes, clean, why, cl, sz); } }
	ast_low_dump_fn = NULL;
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstLocal p = ast_parent(a, n);
		for (i = 0; i < RIR_LOW_NLEVEL; i++) { MCC_TRACE("br\n");
			if (!((cl[n] >> i) & 1))
				{ MCC_TRACE("br\n"); continue; }
			if (p != AST_NONE && ((cl[p] >> i) & 1))
				{ MCC_TRACE("br\n"); continue; }
			regions[i]++;
			if (sz[n] >= AST_LOW_MIN_REGION)
				{ MCC_TRACE("br\n"); big[i] += sz[n]; }
			if (sz[n] >= AST_LOW_BIG_REGION)
				{ MCC_TRACE("br\n"); huge[i] += sz[n]; }
		}
	}
	mcc_free(cl);
	mcc_free(sz);
	rir_low_set(nodes, clean, why, AST_LOW_NCLASS);
	rir_low_regions(regions, big, huge);
}

static int ast_inline_graftable(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal root = ast_root(a);
	if (ast_kind(a, root) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_arena_has_hole(a))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal nn = ast_count(a);
	int totret = 0;
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, n);
		if (k == AST_Return) { MCC_TRACE("br\n");
			totret++;
			if (ast_nchild(a, n) < 1)
				{ MCC_TRACE("br\n"); return 0; }
		}
	}
	return totret >= 1;
}

static int ast_inline_active;
static int ast_inline_bias;
static int ast_argsub_n;
static int ast_argsub_off[AST_INLINE_MAX_PARAMS];
static SValue ast_argsub_val[AST_INLINE_MAX_PARAMS];
static int ast_in_graft;
static int ast_in_reemit;
static void ast_reemit_scrub_leaf_syms(AstArena *a);
static int ast_fn_faithful;
static int ast_fn_tco;
static CType ast_graft_rt;
static int ast_inline_ret_sym;
static int ast_inline_ret_slot;
static void *ast_inline_stack[AST_INLINE_MAX_DEPTH];
static MCC_OPT_TLS int ast_graft_budget;
static MCC_OPT_TLS int ast_inline_depth;

static int ast_inline_graft(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (!ast_inline_active)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal cref = ast_child(a, n, 0);
	if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	void *csym = (void *)(uintptr_t)ast_sym(a, cref);
	struct AstInlineFn *e = ast_inline_find(csym);
	if (!e || !e->graftable)
		{ MCC_TRACE("br\n"); return 0; }
	int nargs = (int)ast_nchild(a, n) - 1;
	int hidden = ((ast_type_t(a, n) & VT_BTYPE) == VT_STRUCT && nargs == e->nparams + 1) ? 1 : 0;
	if (nargs - hidden != e->nparams)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_inline_depth >= ast_inline_depth_max)
		{ MCC_TRACE("br\n"); return 0; }
	for (int i = 0; i < ast_inline_depth; i++)
		{ MCC_TRACE("br\n"); if (ast_inline_stack[i] == csym)
			{ MCC_TRACE("br\n"); return 0; } }
	if (ast_graft_budget < (int)ast_count(e->ast))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_graft_limit >= 0 && ast_graft_total >= ast_graft_limit)
		{ MCC_TRACE("br\n"); return 0; }
	ast_graft_total++;
	ast_graft_budget -= (int)ast_count(e->ast);
	ast_inline_stack[ast_inline_depth++] = csym;
	int hi = 0;
	for (int i = 0; i < e->nparams; i++) { MCC_TRACE("br\n");
		CType pt;
		pt.t = e->param_typ[i];
		pt.ref = (Sym *)e->param_ref[i];
		int pa, ps = type_size(&pt, &pa);
		if (ps < 1)
			{ MCC_TRACE("br\n"); ps = 1; }
		if (e->param_off[i] + ps > hi)
			{ MCC_TRACE("br\n"); hi = e->param_off[i] + ps; }
	}
	int gbase = loc;
	if (ast_replaying && ast_locrec_min < gbase) { MCC_TRACE("br\n");
		gbase = ast_locrec_min;
	}
	if (ast_replaying && ast_graft_base < gbase) { MCC_TRACE("br\n");
		gbase = ast_graft_base;
	}
	if (ast_temp_frontier <= 0 && ast_temp_frontier < gbase) { MCC_TRACE("br\n");
		gbase = ast_temp_frontier;
	}
	int bias = hi > 0 ? ((gbase - hi) & -16) : gbase;
	loc = bias - e->frame_size;
	if (loc < ast_loc_low) { MCC_TRACE("br\n");
		ast_loc_low = loc;
	}
	if (ast_temp_frontier <= 0 && ast_temp_frontier > loc) { MCC_TRACE("br\n");
		ast_temp_frontier = loc;
	}
	int nsub = 0, suboff[AST_INLINE_MAX_PARAMS];
	SValue subval[AST_INLINE_MAX_PARAMS];
	for (int i = 0; i < e->nparams; i++) { MCC_TRACE("br\n");
		int dst = e->param_off[i] + bias;
		AstLocal arg = ast_child(a, n, hidden + i + 1);
		if (e->param_indirect[i]) { MCC_TRACE("br\n");
			CType st;
			st.t = e->param_typ[i];
			st.ref = (Sym *)e->param_ref[i];
			int sal, ssz = type_size(&st, &sal);
			if (ssz < 1)
				{ MCC_TRACE("br\n"); ssz = 1; }
			if (sal < 1)
				{ MCC_TRACE("br\n"); sal = 1; }
			loc = (loc - ssz) & -sal;
			int copy_off = loc;
			ast_replay_value(a, arg);
			SValue cslot;
			memset(&cslot, 0, sizeof cslot);
			cslot.type.t = e->param_typ[i];
			cslot.type.ref = e->param_ref[i];
			cslot.r = VT_LOCAL | VT_LVAL;
			cslot.r2 = VT_CONST;
			cslot.c.i = copy_off;
			vpushv(&cslot);
			vswap();
			vstore();
			vpop();
			SValue aslot;
			memset(&aslot, 0, sizeof aslot);
			aslot.type.t = e->param_typ[i];
			aslot.type.ref = e->param_ref[i];
			aslot.r = VT_LOCAL | VT_LVAL;
			aslot.r2 = VT_CONST;
			aslot.c.i = copy_off;
			vpushv(&aslot);
			gaddrof();
			vtop->type = char_pointer_type;
			SValue pslot;
			memset(&pslot, 0, sizeof pslot);
			pslot.type = char_pointer_type;
			pslot.r = VT_LOCAL | VT_LVAL;
			pslot.r2 = VT_CONST;
			pslot.c.i = dst;
			vpushv(&pslot);
			vswap();
			vstore();
			vpop();
			continue;
		}
		AstLocal cbase = arg;
		while (ast_kind(a, cbase) == AST_Convert && ast_nchild(a, cbase) == 1)
			{ MCC_TRACE("br\n"); cbase = ast_first_child(a, cbase); }
		int pbt = e->param_typ[i] & VT_BTYPE, cbt = ast_type_t(a, cbase) & VT_BTYPE;
		if (ast_templates_env && ast_kind(a, cbase) == AST_Literal &&
				(ast_op(a, cbase) & VT_VALMASK) == VT_CONST && !(ast_op(a, cbase) & VT_SYM) &&
				(pbt == VT_INT || pbt == VT_LLONG) && (cbt == VT_INT || cbt == VT_LLONG) &&
				ast_local_is_readonly(e->ast, e->param_off[i])) { MCC_TRACE("br\n");
			SValue lv;
			memset(&lv, 0, sizeof lv);
			lv.type.t = e->param_typ[i];
			lv.type.ref = (Sym *)e->param_ref[i];
			lv.r = VT_CONST;
			lv.r2 = VT_CONST;
			lv.c.i = value64(ast_ival(a, cbase), e->param_typ[i]);
			suboff[nsub] = e->param_off[i];
			subval[nsub] = lv;
			nsub++;
			continue;
		}
		ast_replay_value(a, ast_child(a, n, hidden + i + 1));
		if (ast_argfwd_env && vtop->r2 == VT_CONST &&
				(e->param_typ[i] & VT_BTYPE) != VT_STRUCT &&
				ast_local_is_readonly(e->ast, e->param_off[i]) &&
				!ast_cprop_escapes(e->ast, e->param_off[i]) &&
				ast_argfwd_read_count(e->ast, e->param_off[i]) == 1) { MCC_TRACE("br\n");
			int vr = vtop->r, fwd = 0;
			if ((vr & VT_VALMASK) == VT_CONST && !(vr & VT_LVAL)) { MCC_TRACE("br\n");
				fwd = 1;
			} else if ((vr & VT_VALMASK) == VT_LOCAL && (vr & VT_LVAL) &&
								 !(vr & VT_SYM) && ast_kind(a, arg) == AST_Ref &&
								 !ast_cprop_escapes(a, (int)(int64_t)ast_ival(a, arg))) { MCC_TRACE("br\n");
				fwd = 1;
			}
			if (fwd) { MCC_TRACE("br\n");
				SValue fv = *vtop;
				fv.type.t = e->param_typ[i];
				fv.type.ref = (Sym *)e->param_ref[i];
				vpop();
				suboff[nsub] = e->param_off[i];
				subval[nsub] = fv;
				nsub++;
				MCC_TRACE("argfwd %s param#%d off=%d %s\n",
									get_tok_str(((Sym *)csym)->v, NULL), i, e->param_off[i],
									(vr & VT_VALMASK) == VT_CONST ? "const" : "local");
				continue;
			}
		}
		SValue slot;
		Sym *tum;
		memset(&slot, 0, sizeof slot);
		slot.type.t = e->param_typ[i];
		slot.type.ref = e->param_ref[i];
		tum = transparent_union_member(&slot.type);
		if (tum)
			{ MCC_TRACE("br\n"); slot.type = tum->type; }
		slot.r = VT_LOCAL | VT_LVAL;
		slot.r2 = VT_CONST;
		slot.c.i = dst;
		vpushv(&slot);
		vswap();
		vstore();
		vpop();
	}
	CType save_rt = ast_graft_rt;
	int save_bias = ast_inline_bias, save_ig = ast_in_graft;
	int save_rsym = ast_inline_ret_sym, save_rslot = ast_inline_ret_slot;
	int save_floor = ast_rp_label_floor, save_nlabel = ast_rp_nlabel;
	struct switch_t *save_switch = ast_rp_switch;
	int *save_bsym = ast_rp_bsym, *save_csym = ast_rp_csym;
	int save_argsub_n = ast_argsub_n;
	int save_argsub_off[AST_INLINE_MAX_PARAMS];
	SValue save_argsub_val[AST_INLINE_MAX_PARAMS];
	memcpy(save_argsub_off, ast_argsub_off, sizeof save_argsub_off);
	memcpy(save_argsub_val, ast_argsub_val, sizeof save_argsub_val);
	ast_graft_rt.t = ast_type_t(a, n);
	ast_graft_rt.bp = ast_type_bp(a, n);
	ast_graft_rt.bs = ast_type_bs(a, n);
	ast_graft_rt.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
	int ral, rsz = type_size(&ast_graft_rt, &ral);
	if (rsz < 1)
		{ MCC_TRACE("br\n"); rsz = 8; }
	if ((ast_type_t(a, n) & VT_BTYPE) == VT_STRUCT)
		{ MCC_TRACE("br\n"); ast_locrec_skip(rsz, ral > 0 ? ral : 1); }
	loc = (loc - rsz) & -(ral > 0 ? ral : 1);
	ast_inline_bias = bias;
	ast_in_graft = 1;
	ast_inline_ret_sym = 0;
	ast_inline_ret_slot = loc;
	ast_rp_label_floor = ast_rp_nlabel;
	ast_rp_switch = NULL;
	ast_rp_bsym = ast_rp_csym = NULL;
	ast_argsub_n = nsub;
	memcpy(ast_argsub_off, suboff, sizeof suboff);
	memcpy(ast_argsub_val, subval, sizeof subval);
	save_regs(0);
	/* A callee grafted during re-emit runs after its own frame is gone, so its
	   leaves' captured local syms dangle exactly as the re-emit root's do. */
	if (ast_in_reemit)
		{ MCC_TRACE("br\n"); ast_reemit_scrub_leaf_syms(e->ast); }
	ast_replay_bb(e->ast, ast_root(e->ast));
	gsym(ast_inline_ret_sym);
	SValue res;
	memset(&res, 0, sizeof res);
	res.type = ast_graft_rt;
	res.r = VT_LOCAL | VT_LVAL;
	res.r2 = VT_CONST;
	res.c.i = ast_inline_ret_slot;
	vpushv(&res);
	ast_inline_bias = save_bias;
	ast_in_graft = save_ig;
	ast_inline_ret_sym = save_rsym;
	ast_inline_ret_slot = save_rslot;
	ast_graft_rt = save_rt;
	ast_rp_label_floor = save_floor;
	ast_rp_nlabel = save_nlabel;
	ast_rp_switch = save_switch;
	ast_rp_bsym = save_bsym;
	ast_rp_csym = save_csym;
	ast_argsub_n = save_argsub_n;
	memcpy(ast_argsub_off, save_argsub_off, sizeof save_argsub_off);
	memcpy(ast_argsub_val, save_argsub_val, sizeof save_argsub_val);
	ast_inline_depth--;
	if (ast_replay_dump) { MCC_TRACE("br\n");
		fprintf(stderr, "[ast-inline] grafted %s\n", get_tok_str(((Sym *)csym)->v, NULL));
		if (nsub)
			{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-inline] specialized %s (%d const arg%s)\n",
							get_tok_str(((Sym *)csym)->v, NULL), nsub, nsub == 1 ? "" : "s"); }
	}
	return 1;
}

static int ast_has_graftable_call(AstArena *a) { MCC_TRACE("enter\n");
	if (!ast_inline_env)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Invoke)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal cref = ast_child(a, n, 0);
		if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		void *csym = (void *)(uintptr_t)ast_sym(a, cref);
		struct AstInlineFn *e = ast_inline_find(csym);
		if (e && e->graftable)
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int ast_inline_retain(AstArena *a, Sym *sym) { MCC_TRACE("enter\n");
	if (!ast_fn_inlinable(a, sym) || ast_inline_lookup(sym) ||
			ast_arena_has_hole(a) || !ast_inline_grow())
		{ MCC_TRACE("br\n"); return 0; }
	struct AstInlineFn *e = &ast_inline_pool[ast_inline_n++];
	if (ast_inline_n > ast_inline_hi)
		{ MCC_TRACE("br\n"); ast_inline_hi = ast_inline_n; }
	e->sym = sym;
	e->ast = a;
	ast_inline_index_add(ast_inline_n - 1);
	e->frame_size = loc < 0 ? -loc : 0;
	e->nparams = ast_inline_cap_np;
	for (int i = 0; i < ast_inline_cap_np; i++) { MCC_TRACE("br\n");
		e->param_off[i] = ast_inline_cap_off[i];
		e->param_typ[i] = ast_inline_cap_typ[i];
		e->param_ref[i] = ast_inline_cap_ref[i];
		e->param_size[i] = ast_inline_cap_size[i];
		e->param_stack[i] = ast_inline_cap_stack[i];
		e->param_indirect[i] = ast_inline_cap_ind[i];
	}
	e->graftable = ast_inline_cap_ok && !ast_fn_tco && ast_inline_graftable(a);
	if (ast_replay_dump)
		{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-inline] candidate %s (%d nodes, %d params, frame %d, %s)\n",
						get_tok_str(sym->v, NULL), (int)ast_count(a), e->nparams, e->frame_size,
						e->graftable ? "graftable" : "retained-only"); }
	return 1;
}

static int ast_reemit_retain(AstArena *a, Sym *sym) { MCC_TRACE("enter\n");
	if (!ast_inline_env || ast_reemit_poison ||
			ast_reemit_n >= AST_INLINE_MAX || ast_arena_has_hole(a))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal nn = ast_count(a);
	int has_static_call = 0;
	for (AstLocal n = 0; n < nn && !has_static_call; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Invoke)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal ce = ast_first_child(a, n);
		void *cs = ce != AST_NONE ? (void *)(uintptr_t)ast_sym(a, ce) : NULL;
		if (cs && (((Sym *)cs)->type.t & VT_STATIC) &&
				(((Sym *)cs)->type.t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); has_static_call = 1; }
	}
	if (!has_static_call)
		{ MCC_TRACE("br\n"); return 0; }
	ast_reemit_pool[ast_reemit_n].sym = sym;
	ast_reemit_pool[ast_reemit_n].ast = a;
	ast_reemit_pool[ast_reemit_n].inline_n_at_gen = ast_inline_n;
	ast_reemit_pool[ast_reemit_n].body_ind = ast_body_ind_sv;
	ast_reemit_pool[ast_reemit_n].body_len = ind - ast_body_ind_sv;
	ast_reemit_pool[ast_reemit_n].reloc0 = ast_reloc0_sv;
	ast_reemit_pool[ast_reemit_n].rel_len =
			cur_text_section->reloc
					? (int)(cur_text_section->reloc->data_offset - ast_reloc0_sv)
					: 0;
	ast_reemit_n++;
	return 1;
}

void ast_reemit_finalize_span(Sym *sym) { MCC_TRACE("enter\n");
	if (ast_reemit_n > 0 && ast_reemit_pool[ast_reemit_n - 1].sym == sym)
		{ MCC_TRACE("br\n"); ast_reemit_pool[ast_reemit_n - 1].body_len =
				ind - ast_reemit_pool[ast_reemit_n - 1].body_ind; }
}

static struct AstBaselineFn {
	Sym *sym;
	AstArena *ast;
	unsigned char *code;
	unsigned char *rel;
	int code_len;
	int rel_len;
	int body_ind;
	addr_t reloc0;
	int chain_head;
} ast_baseline_pool[AST_INLINE_MAX];
static int ast_baseline_n;

static int ast_baseline_retain(Sym *sym, AstArena *a, int src_base, addr_t reloc0,
															 int chain_head) { MCC_TRACE("enter\n");
	if (!ast_jit_env || ast_baseline_n >= AST_INLINE_MAX || ast_arena_has_hole(a))
		{ MCC_TRACE("br\n"); return 0; }
	Section *ts = cur_text_section;
	Section *rs = ts->reloc;
	int code_len = ind - src_base;
	int rel_len = rs ? (int)(rs->data_offset - reloc0) : 0;
	struct AstBaselineFn *e = &ast_baseline_pool[ast_baseline_n++];
	e->sym = sym;
	e->ast = a;
	e->code_len = code_len;
	e->rel_len = rel_len;
	e->body_ind = src_base;
	e->reloc0 = reloc0;
	e->chain_head = chain_head;
	e->code = mcc_malloc(code_len > 0 ? code_len : 1);
	memcpy(e->code, ts->data + src_base, code_len > 0 ? code_len : 1);
	e->rel = mcc_malloc(rel_len > 0 ? rel_len : 1);
	if (rel_len > 0)
		{ MCC_TRACE("br\n"); memcpy(e->rel, rs->data + reloc0, rel_len); }
	MCC_TRACE("jit-baseline retain %s (%d code, %d rel)\n", get_tok_str(sym->v, NULL),
						code_len, rel_len);
	return 1;
}

static void ast_baseline_splice(const unsigned char *code, int code_len,
																const unsigned char *rel, int rel_len, int src_base,
																int chain_head) { MCC_TRACE("enter\n");
	Section *ts = cur_text_section;
	int dst = ind;
	if (code_len > 0) { MCC_TRACE("br\n");
		if ((unsigned long)(ind + code_len) > ts->data_allocated)
			{ MCC_TRACE("br\n"); section_realloc(ts, ind + code_len); }
		memcpy(ts->data + ind, code, code_len);
		ind += code_len;
	}
	for (int roff = 0; roff + (int)sizeof(ElfW_Rel) <= rel_len; roff += (int)sizeof(ElfW_Rel)) { MCC_TRACE("br\n");
		const ElfW_Rel *r = (const ElfW_Rel *)(rel + roff);
		put_elf_reloca(symtab_section, ts, dst + (r->r_offset - src_base),
									 ELFW(R_TYPE)(r->r_info), ELFW(R_SYM)(r->r_info),
									 ELFW_R_ADDEND(r));
	}
	for (int t = chain_head; t;) { MCC_TRACE("br\n");
		int boff = t - src_base;
		if (boff < 0 || boff + 4 > code_len)
			{ MCC_TRACE("br\n"); break; }
		int next = (int)read32le((unsigned char *)code + boff);
		write32le(ts->data + dst + boff, (uint32_t)rsym);
		rsym = dst + boff;
		t = next;
	}
}


static int ast_reemit_has_forward(struct AstReemitFn *f) { MCC_TRACE("enter\n");
	AstArena *a = f->ast;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Invoke)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal ce = ast_first_child(a, n);
		void *cs = ce != AST_NONE ? (void *)(uintptr_t)ast_sym(a, ce) : NULL;
		if (!cs)
			{ MCC_TRACE("br\n"); continue; }
		int i = ast_inline_index(cs);
		if (i >= f->inline_n_at_gen && ast_inline_pool[i].graftable)
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}
static int ast_ref_is_local_off(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	int r = ast_op(a, n);
	return (r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM) &&
				 (int)(int64_t)ast_ival(a, n) == off;
}

static int ast_local_is_readonly_scan(AstArena *a, int off) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, n);
		if (k == AST_Store && ast_ref_is_local_off(a, ast_child(a, n, 0), off))
			{ MCC_TRACE("br\n"); return 0; }
		if (k == AST_Unary && ast_ref_is_local_off(a, ast_first_child(a, n), off))
			{ MCC_TRACE("br\n"); return 0; }
	}
	return 1;
}

int ast_data_all_zero(void *sec, long off, long size) { MCC_TRACE("enter\n");
	const unsigned char *bytes = ((Section *)sec)->data + off;
	long i;
	for (i = 0; i < size; i++)
		{ MCC_TRACE("br\n"); if (bytes[i])
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

#define AST_STRPOOL_CAP 8192
typedef struct {
	uint32_t hash;
	long addr;
	long size;
	int align;
} AstStrRec;
static AstStrRec ast_strpool[AST_STRPOOL_CAP];

static uint32_t ast_str_hash(const unsigned char *b, long n) { MCC_TRACE("enter\n");
	uint32_t h = 2166136261u;
	long i;
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		h ^= b[i];
		h *= 16777619u;
	}
	return h;
}

long ast_strpool_find_or_add(void *sec, long addr, long size, int align) { MCC_TRACE("enter\n");
	unsigned char *data = ((Section *)sec)->data;
	const unsigned char *bytes = data + addr;
	uint32_t h = ast_str_hash(bytes, size);
	int i;
	for (i = 0; i < ast_strpool_n; i++) { MCC_TRACE("br\n");
		AstStrRec *r = &ast_strpool[i];
		long off;
		if (r->size < size)
			{ MCC_TRACE("br\n"); continue; }
		off = r->addr + (r->size - size);
		if (off % align != 0)
			{ MCC_TRACE("br\n"); continue; }
		if ((r->size == size ? r->hash == h : 1) &&
				memcmp(data + off, bytes, (size_t)size) == 0) { MCC_TRACE("br\n");
			MCC_TRACE("strpool %s addr=%ld size=%ld -> shared=%ld (in %ld@%ld)\n",
								off == r->addr ? "exact" : "suffix", addr, size, off, r->size, r->addr);
			return off;
		}
	}
	if (ast_strpool_n < AST_STRPOOL_CAP) { MCC_TRACE("br\n");
		ast_strpool[ast_strpool_n].hash = h;
		ast_strpool[ast_strpool_n].addr = addr;
		ast_strpool[ast_strpool_n].size = size;
		ast_strpool[ast_strpool_n].align = align;
		ast_strpool_n++;
	}
	return -1;
}

#define AST_DU_WRITTEN 1u
#define AST_DU_ESCAPED 2u
static MCC_OPT_TLS const AstArena *ast_du_arena;
static MCC_OPT_TLS uint64_t ast_du_epoch;
static MCC_OPT_TLS int ast_du_state;
static MCC_OPT_TLS int ast_du_n;
static MCC_OPT_TLS int ast_du_cap;
static MCC_OPT_TLS int *ast_du_off;
static MCC_OPT_TLS uint8_t *ast_du_flags;
static MCC_OPT_TLS int ast_du_hn;
static MCC_OPT_TLS int *ast_du_hash;

static unsigned ast_du_bucket(int off) { MCC_TRACE("enter\n");
	unsigned h = (unsigned)off * 2654435761u;
	return h ^ (h >> 15);
}

static void ast_du_rehash(void) { MCC_TRACE("enter\n");
	unsigned m = (unsigned)ast_du_hn - 1;
	memset(ast_du_hash, 0, (size_t)ast_du_hn * sizeof *ast_du_hash);
	for (int i = 0; i < ast_du_n; i++) { MCC_TRACE("br\n");
		unsigned j = ast_du_bucket(ast_du_off[i]) & m;
		while (ast_du_hash[j])
			{ MCC_TRACE("br\n"); j = (j + 1) & m; }
		ast_du_hash[j] = i + 1;
	}
}

static uint8_t *ast_du_find(int off, int create) { MCC_TRACE("enter\n");
	unsigned m, j;
	if (!ast_du_hn) { MCC_TRACE("br\n");
		ast_du_hn = 256;
		ast_du_hash =
				mcc_realloc(ast_du_hash, (size_t)ast_du_hn * sizeof *ast_du_hash);
		memset(ast_du_hash, 0, (size_t)ast_du_hn * sizeof *ast_du_hash);
	}
	m = (unsigned)ast_du_hn - 1;
	j = ast_du_bucket(off) & m;
	while (ast_du_hash[j]) { MCC_TRACE("br\n");
		int i = ast_du_hash[j] - 1;
		if (ast_du_off[i] == off)
			{ MCC_TRACE("br\n"); return &ast_du_flags[i]; }
		j = (j + 1) & m;
	}
	if (!create)
		{ MCC_TRACE("br\n"); return NULL; }
	if (ast_du_n >= ast_du_cap) { MCC_TRACE("br\n");
		ast_du_cap = ast_du_cap ? ast_du_cap * 2 : 256;
		ast_du_off = mcc_realloc(ast_du_off, (size_t)ast_du_cap * sizeof *ast_du_off);
		ast_du_flags =
				mcc_realloc(ast_du_flags, (size_t)ast_du_cap * sizeof *ast_du_flags);
	}
	ast_du_off[ast_du_n] = off;
	ast_du_flags[ast_du_n] = 0;
	ast_du_n++;
	if (ast_du_n * 2 >= ast_du_hn) { MCC_TRACE("br\n");
		ast_du_hn *= 2;
		ast_du_hash =
				mcc_realloc(ast_du_hash, (size_t)ast_du_hn * sizeof *ast_du_hash);
		ast_du_rehash();
	} else { MCC_TRACE("br\n");
		ast_du_hash[j] = ast_du_n;
	}
	return &ast_du_flags[ast_du_n - 1];
}

static void ast_du_build(const AstArena *a) { MCC_TRACE("enter\n");
	ast_du_n = 0;
	if (ast_du_hn)
		{ MCC_TRACE("br\n"); memset(ast_du_hash, 0, (size_t)ast_du_hn * sizeof *ast_du_hash); }
	ast_du_state = 1;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, n);
		AstLocal c;
		int r, off;
		uint8_t *f;
		if (k == AST_Store) { MCC_TRACE("br\n");
			c = ast_first_child(a, n);
			if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
				{ MCC_TRACE("br\n"); continue; }
			r = ast_op(a, c);
			if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
				{ MCC_TRACE("br\n"); continue; }
			f = ast_du_find((int)(int64_t)ast_ival(a, c), 1);
			if (!f) { MCC_TRACE("br\n");
				ast_du_state = -1;
				return;
			}
			*f |= AST_DU_WRITTEN;
		} else if (k == AST_Unary) { MCC_TRACE("br\n");
			c = ast_first_child(a, n);
			if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
				{ MCC_TRACE("br\n"); continue; }
			r = ast_op(a, c);
			off = (int)(int64_t)ast_ival(a, c);
			if ((r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM)) { MCC_TRACE("br\n");
				f = ast_du_find(off, 1);
				if (!f) { MCC_TRACE("br\n");
					ast_du_state = -1;
					return;
				}
				*f |= AST_DU_WRITTEN;
			}
			int op = ast_op(a, n);
			if ((op == AST_OP_ADDR || op == AST_OP_MEMBER ||
					 op == AST_OP_MEMBER_ARROW) &&
					(r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) { MCC_TRACE("br\n");
				f = ast_du_find(off, 1);
				if (!f) { MCC_TRACE("br\n");
					ast_du_state = -1;
					return;
				}
				*f |= AST_DU_ESCAPED;
			}
		}
	}
}

static void ast_du_sync(const AstArena *a) { MCC_TRACE("enter\n");
	if (ast_du_state && ast_du_arena == a && ast_du_epoch == a->epoch)
		{ MCC_TRACE("br\n"); return; }
	ast_du_arena = a;
	ast_du_epoch = a->epoch;
	ast_du_build(a);
}

static void ast_du_invalidate(const AstArena *a) { MCC_TRACE("enter\n");
	if (ast_du_arena == a) { MCC_TRACE("br\n");
		ast_du_arena = NULL;
		ast_du_state = 0;
	}
}

static unsigned ast_du_slot_flags(const AstArena *a, int off) { MCC_TRACE("enter\n");
	ast_du_sync(a);
	uint8_t *f = ast_du_find(off, 0);
	return f ? *f : 0u;
}

#if MCC_DEV
static void ast_du_diverge(const char *q, int off, int tab, int scan) { MCC_TRACE("enter\n");
	fprintf(stderr,
					"mcc: AST side-car divergence: %s(off=%d) table=%d scan=%d\n", q,
					off, tab, scan);
	abort();
}
static int ast_du_verify(void) { MCC_TRACE("enter\n");
	static int cached = -1;
	if (cached < 0)
		{ MCC_TRACE("br\n"); cached = mcc_env_on("MCC_DU_VERIFY"); }
	return cached;
}
#endif

enum {
	AST_MEMO_PURE,
	AST_MEMO_CPROPSAFE,
	AST_MEMO_HASLABEL,
	AST_MEMO_HASCASE,
	AST_MEMO_REGPURE,
	AST_MEMO_PRED_COUNT
};
static MCC_OPT_TLS const AstArena *ast_memo_arena;
static MCC_OPT_TLS uint64_t ast_memo_epoch;
static MCC_OPT_TLS int ast_memo_cap;
static MCC_OPT_TLS int8_t *ast_memo[AST_MEMO_PRED_COUNT];
static MCC_OPT_TLS uint64_t *ast_memo_stamp;

static void ast_memo_sync(const AstArena *a) { MCC_TRACE("enter\n");
	int cnt = (int)ast_count(a);
	ast_memo_epoch = a->epoch + 1;
	if (ast_memo_arena == a && ast_memo_cap >= cnt)
		{ MCC_TRACE("br\n"); return; }
	if (ast_memo_cap < cnt) { MCC_TRACE("br\n");
		int ncap = cnt ? cnt : 1;
		for (int i = 0; i < AST_MEMO_PRED_COUNT; i++)
			{ MCC_TRACE("br\n"); ast_memo[i] =
					mcc_realloc(ast_memo[i], (size_t)ncap * sizeof *ast_memo[i]); }
		ast_memo_stamp = mcc_realloc(ast_memo_stamp,
																 (size_t)ncap * sizeof *ast_memo_stamp);
		memset(ast_memo_stamp + ast_memo_cap, 0,
					 (size_t)(ncap - ast_memo_cap) * sizeof *ast_memo_stamp);
		ast_memo_cap = ncap;
	}
	if (ast_memo_arena != a) { MCC_TRACE("br\n");
		if (ast_memo_stamp)
			{ MCC_TRACE("br\n"); memset(ast_memo_stamp, 0,
																 (size_t)ast_memo_cap * sizeof *ast_memo_stamp); }
		ast_memo_arena = a;
	}
}

static void ast_memo_touch(AstLocal n) { MCC_TRACE("enter\n");
	if (ast_memo_stamp[n] == ast_memo_epoch)
		{ MCC_TRACE("br\n"); return; }
	ast_memo_stamp[n] = ast_memo_epoch;
	for (int i = 0; i < AST_MEMO_PRED_COUNT; i++)
		{ MCC_TRACE("br\n"); if (ast_memo[i])
			{ MCC_TRACE("br\n"); ast_memo[i][n] = 0; } }
}

static void ast_memo_invalidate(const AstArena *a) { MCC_TRACE("enter\n");
	if (ast_memo_arena == a) { MCC_TRACE("br\n");
		ast_memo_arena = NULL;
		ast_memo_epoch = 0;
	}
}

#if MCC_DEV
static void ast_memo_diverge(const char *q, AstLocal n, int memo, int scan) { MCC_TRACE("enter\n");
	fprintf(stderr,
					"mcc: AST side-car divergence: %s(node=%u) memo=%d scan=%d\n", q,
					(unsigned)n, memo, scan);
	abort();
}
#define AST_MEMO_SHADOW(NAME, N, R)                    \
	do {                                                 \
		int s_ = ast_##NAME##_compute(a, (N));             \
		if ((R) != s_)                                     \
			ast_memo_diverge(#NAME, (N), (R), s_);           \
	} while (0)
#else
#define AST_MEMO_SHADOW(NAME, N, R) ((void)0)
#endif

#define AST_MEMO_QUERY(NAME, SLOT)                             \
	static int ast_##NAME##_compute(AstArena *a, AstLocal n);    \
	static int ast_##NAME(AstArena *a, AstLocal n) {             \
		ast_memo_sync(a);                                         \
		int8_t *m = ast_memo[SLOT];                               \
		if (m && n < (AstLocal)ast_memo_cap) {                    \
			ast_memo_touch(n);                                      \
			if (m[n]) {                                             \
				int r = m[n] == 1;                                    \
				AST_MEMO_SHADOW(NAME, n, r);                          \
				return r;                                             \
			}                                                       \
		}                                                         \
		int r = ast_##NAME##_compute(a, n);                       \
		if (m && n < (AstLocal)ast_memo_cap)                      \
			m[n] = r ? 1 : 2;                                       \
		return r;                                                 \
	}

AST_MEMO_QUERY(ident_pure, AST_MEMO_PURE)
AST_MEMO_QUERY(cprop_safe, AST_MEMO_CPROPSAFE)
AST_MEMO_QUERY(sccp_has_label, AST_MEMO_HASLABEL)
AST_MEMO_QUERY(sccp_has_case, AST_MEMO_HASCASE)
AST_MEMO_QUERY(cse_regpure, AST_MEMO_REGPURE)

static MCC_OPT_TLS const AstArena *ast_hash_arena;
static MCC_OPT_TLS uint64_t ast_hash_epoch;
static MCC_OPT_TLS int ast_hash_cap;
static MCC_OPT_TLS uint64_t *ast_hash;
static MCC_OPT_TLS uint8_t *ast_hash_done;

static void ast_hash_sync(const AstArena *a) { MCC_TRACE("enter\n");
	int cnt = (int)ast_count(a);
	if (ast_hash_arena == a && ast_hash_epoch == a->epoch && ast_hash_cap >= cnt)
		{ MCC_TRACE("br\n"); return; }
	if (ast_hash_cap < cnt) { MCC_TRACE("br\n");
		int ncap = cnt ? cnt : 1;
		ast_hash = mcc_realloc(ast_hash, (size_t)ncap * sizeof *ast_hash);
		ast_hash_done = mcc_realloc(ast_hash_done, (size_t)ncap * sizeof *ast_hash_done);
		ast_hash_cap = ncap;
	}
	if (ast_hash_done)
		{ MCC_TRACE("br\n"); memset(ast_hash_done, 0, (size_t)ast_hash_cap); }
	ast_hash_arena = a;
	ast_hash_epoch = a->epoch;
}

static const AstArena *ast_sattr_arena;
static int *ast_sattr;
static AstLocal ast_sattr_cap;
static signed char *ast_slc_memo;
static AstLocal ast_slc_memo_cap;

void ast_teardown(void) { MCC_TRACE("enter\n");
	int i;
	mcc_free(ast_locrec);
	mcc_free(ast_locrec_sz);
	mcc_free(ast_locrec_al);
#if MCC_DIAG
	for (i = 0; i < ast_sym_deferred_n; i++)
		{ MCC_TRACE("br\n"); mcc_free(ast_sym_deferred[i]); }
	for (i = 0; i < ast_sym_retained_n; i++)
		{ MCC_TRACE("br\n"); mcc_free(ast_sym_retained[i]); }
#endif
	ast_sym_deferred_n = 0;
	ast_sym_retained_n = 0;
	mcc_free(ast_sym_deferred);
	mcc_free(ast_sym_retained);
	ast_sym_retained = NULL;
	ast_sym_retained_cap = 0;
	mcc_free(ast_fconst);
	mcc_free(ast_fconst_cplx);
	mcc_free(ast_fconst_key);
	mcc_free(ast_du_hash);
	mcc_free(ast_du_off);
	mcc_free(ast_du_flags);
	mcc_free(ast_memo_stamp);
	mcc_free(ast_hash);
	mcc_free(ast_hash_done);
	mcc_free(ast_rp_labels);
	mcc_free(ast_sattr);
	mcc_free(ast_slc_memo);
	mcc_free(ast_inline_pool);
	mcc_free(ast_inline_hash);
	ast_inline_pool = NULL;
	ast_inline_hash = NULL;
	ast_inline_pool_cap = 0;
	ast_inline_hash_cap = 0;
	ast_inline_n = 0;
	ast_sattr = NULL;
	ast_sattr_cap = 0;
	ast_sattr_arena = NULL;
	ast_slc_memo = NULL;
	ast_slc_memo_cap = 0;
	for (i = 0; i < AST_MEMO_PRED_COUNT; i++)
		{ MCC_TRACE("br\n"); mcc_free(ast_memo[i]); ast_memo[i] = NULL; }
	ast_locrec = NULL;
	ast_locrec_sz = NULL;
	ast_locrec_al = NULL;
	ast_sym_deferred = NULL;
	ast_fconst = NULL;
	ast_fconst_cplx = NULL;
	ast_fconst_key = NULL;
	ast_du_hash = NULL;
	ast_du_off = NULL;
	ast_du_flags = NULL;
	ast_memo_stamp = NULL;
	ast_hash = NULL;
	ast_hash_done = NULL;
	ast_rp_labels = NULL;
	ast_locrec_n = ast_locrec_cap = ast_locrec_i = 0;
	ast_fconst_cap = 0;
	ast_du_cap = 0;
	ast_du_hn = 0;
	ast_memo_cap = 0;
	ast_memo_arena = NULL;
	ast_hash_cap = 0;
	ast_rp_nlabel = ast_rp_caplabel = 0;
	ast_sym_deferred_cap = 0;
}

static void ast_hash_invalidate(const AstArena *a) { MCC_TRACE("enter\n");
	if (ast_hash_arena == a) { MCC_TRACE("br\n");
		ast_hash_arena = NULL;
		ast_hash_epoch = 0;
	}
}

static uint64_t ast_hash_mix(uint64_t h, uint64_t v) { MCC_TRACE("enter\n");
	h ^= v;
	h *= 0x100000001b3ull;
	return h;
}

static uint64_t ast_hash_of(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n >= (AstLocal)ast_hash_cap)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_hash_done[n])
		{ MCC_TRACE("br\n"); return ast_hash[n]; }
	uint64_t v = 0xcbf29ce484222325ull;
	v = ast_hash_mix(v, a->kind[n]);
	v = ast_hash_mix(v, (uint64_t)(uint32_t)a->op[n]);
	v = ast_hash_mix(v, (uint64_t)(uint32_t)a->type_t[n]);
	v = ast_hash_mix(v, (uint64_t)a->type_bp[n] | ((uint64_t)a->type_bs[n] << 8));
	v = ast_hash_mix(v, a->type_ref[n]);
	v = ast_hash_mix(v, a->ival[n]);
	v = ast_hash_mix(v, a->fbits[n]);
	v = ast_hash_mix(v, a->sym[n]);
	if (a->wide_hi && (a->wide_hi[n] || a->wide_r2[n] != AST_R2_NONE)) { MCC_TRACE("br\n");
		v = ast_hash_mix(v, a->wide_hi[n]);
		v = ast_hash_mix(v, a->wide_r2[n]);
	}
	v = ast_hash_mix(v, a->nchild[n]);
	for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c])
		{ MCC_TRACE("br\n"); v = ast_hash_mix(v, ast_hash_of(a, c)); }
	ast_hash[n] = v;
	ast_hash_done[n] = 1;
	return v;
}

static int ast_local_is_readonly(AstArena *a, int off) { MCC_TRACE("enter\n");
	int r;
	ast_du_sync(a);
	if (ast_du_state < 0)
		{ MCC_TRACE("br\n"); r = ast_local_is_readonly_scan(a, off); }
	else
		{ MCC_TRACE("br\n"); r = (ast_du_slot_flags(a, off) & AST_DU_WRITTEN) ? 0 : 1; }
#if MCC_DEV
	if (ast_du_verify()) { MCC_TRACE("br\n");
		int s = ast_local_is_readonly_scan(a, off);
		if (r != s)
			{ MCC_TRACE("br\n"); ast_du_diverge("readonly", off, r, s); }
	}
#endif
	return r;
}

static void ast_subtree_span(AstArena *a, AstLocal n, int *lo, int *hi) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	if ((int)n < *lo)
		{ MCC_TRACE("br\n"); *lo = (int)n; }
	if ((int)n > *hi)
		{ MCC_TRACE("br\n"); *hi = (int)n; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_subtree_span(a, c, lo, hi); }
}

/* A struct type the arena recorded without its Sym. type_size() reads
 * `type->ref->r` unconditionally for VT_STRUCT, so asking it for the size of one
 * of these is a null dereference inside the compiler rather than a wrong answer.
 * The promotion planner only ever wants the size to decide how much of the frame
 * an object covers, and every caller here can treat "unknown" as "covers
 * everything from its offset up", which is the safe direction. */
static int ast_promo_size_unknown(const CType *t) { MCC_TRACE("enter\n");
	return (t->t & VT_BTYPE) == VT_STRUCT && !t->ref;
}

#if (defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64))
static int ast_promo_reg_of(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return -1; }
	int r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
		{ MCC_TRACE("br\n"); return -1; }
	int off = (int)(int64_t)ast_ival(a, n);
	for (int i = 0; i < ast_promo_n; i++)
		{ MCC_TRACE("br\n"); if (ast_promo_off[i] == off)
			{ MCC_TRACE("br\n"); return ast_promo_reg[i]; } }
	return -1;
}

static int ast_subtree_has_call(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Invoke)
		{ MCC_TRACE("br\n"); return 1; }
	uint32_t nc = ast_nchild(a, n);
	for (uint32_t i = 0; i < nc; i++)
		{ MCC_TRACE("br\n"); if (ast_subtree_has_call(a, ast_child(a, n, i)))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_promo_store_late(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	if (!ast_promo_n || ast_in_graft || ast_nchild(a, s) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_promo_reg_of(a, ast_child(a, s, 0)) < 0)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_subtree_has_call(a, ast_child(a, s, 1));
}

static int ast_subtree_refs_local(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM) &&
				(int)(int64_t)ast_ival(a, n) == off)
			{ MCC_TRACE("br\n"); return 1; }
	}
	uint32_t nc = ast_nchild(a, n);
	for (uint32_t i = 0; i < nc; i++)
		{ MCC_TRACE("br\n"); if (ast_subtree_refs_local(a, ast_child(a, n, i), off))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static void ast_promo_weigh(AstArena *a, AstLocal n, int depth, const int *coff, int nc,
														int *cweight) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	if (k == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) { MCC_TRACE("br\n");
			int off = (int)(int64_t)ast_ival(a, n);
			for (int j = 0; j < nc; j++)
				{ MCC_TRACE("br\n"); if (coff[j] == off) { MCC_TRACE("br\n");
					cweight[j] += 1 << (depth < 12 ? depth : 12);
					break;
				} }
		}
	}
	int cd = (k == AST_If && (ast_op(a, n) == 2 || ast_op(a, n) == 3 ||
														ast_op(a, n) == 4 || ast_op(a, n) == 8))
							 ? depth + 1
							 : depth;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_promo_weigh(a, c, cd, coff, nc, cweight); }
}

static int ast_subtree_reads_local(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM) &&
				(int)(int64_t)ast_ival(a, n) == off)
			{ MCC_TRACE("br\n"); return 1; }
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_subtree_reads_local(a, c, off))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_libcall_divmod_op(int op) { MCC_TRACE("enter\n");
	return op == '/' || op == '%' || op == TOK_PDIV || op == TOK_UDIV ||
				 op == TOK_UMOD;
}

static int ast_libcall_shift_op(int op) { MCC_TRACE("enter\n");
	return op == TOK_SHL || op == TOK_SAR || op == TOK_SHR;
}

static int ast_node_libcall(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	uint16_t k = ast_kind(a, n);
	int t = ast_type_t(a, n);
	int bt = t & VT_BTYPE;
	if (t & VT_ATOMIC_BIT)
		{ MCC_TRACE("br\n"); return 1; }
	if (k == AST_Binary) { MCC_TRACE("br\n");
		int op = ast_op(a, n);
		AstLocal c0 = ast_first_child(a, n);
		int lbt = c0 == AST_NONE ? 0 : (ast_type_t(a, c0) & VT_BTYPE);
		if ((bt == VT_INT128 || lbt == VT_INT128 || bt == VT_QLONG ||
				 lbt == VT_QLONG) &&
				(op == '*' || ast_libcall_divmod_op(op) || ast_libcall_shift_op(op)))
			{ MCC_TRACE("br\n"); return 1; }
		if ((bt == VT_LDOUBLE || lbt == VT_LDOUBLE || bt == VT_QFLOAT ||
				 lbt == VT_QFLOAT) &&
				(op == '*' || op == '/' || op == '+' || op == '-'))
			{ MCC_TRACE("br\n"); return 1; }
		if (IS_HALF_BT(bt) || IS_HALF_BT(lbt))
			{ MCC_TRACE("br\n"); return 1; }
		return 0;
	}
	if (k == AST_Convert) { MCC_TRACE("br\n");
		AstLocal c0 = ast_first_child(a, n);
		int st = c0 == AST_NONE ? 0 : ast_type_t(a, c0);
		int d = t & (VT_BTYPE | VT_UNSIGNED);
		int s = st & (VT_BTYPE | VT_UNSIGNED);
		if (c0 == AST_NONE)
			{ MCC_TRACE("br\n"); return 0; }
		if (((d & VT_BTYPE) == VT_INT128 && is_float(st)) ||
				((s & VT_BTYPE) == VT_INT128 && is_float(t)))
			{ MCC_TRACE("br\n"); return 1; }
		if (s == (VT_LLONG | VT_UNSIGNED) && is_float(t))
			{ MCC_TRACE("br\n"); return 1; }
		if (d == (VT_LLONG | VT_UNSIGNED) && is_float(st))
			{ MCC_TRACE("br\n"); return 1; }
		if ((s & VT_BTYPE) == VT_LDOUBLE && !is_float(t))
			{ MCC_TRACE("br\n"); return 1; }
		if ((d & VT_BTYPE) == VT_LDOUBLE && !is_float(st))
			{ MCC_TRACE("br\n"); return 1; }
		if ((IS_HALF_BT(s & VT_BTYPE) || IS_HALF_BT(d & VT_BTYPE)) &&
				(s & VT_BTYPE) != (d & VT_BTYPE))
			{ MCC_TRACE("br\n"); return 1; }
		return 0;
	}
	if (k == AST_Unary) { MCC_TRACE("br\n");
		int op = ast_op(a, n);
		if (op == AST_OP_BSWAP || op == AST_OP_SIGNBIT || op == AST_OP_FFS ||
				op == AST_OP_BITSCAN)
			{ MCC_TRACE("br\n"); return 1; }
		if (op == AST_OP_FNEG && (bt == VT_LDOUBLE || IS_HALF_BT(bt)))
			{ MCC_TRACE("br\n"); return 1; }
		return 0;
	}
#ifndef MCC_TARGET_NATIVE_STRUCT_COPY
	if (k == AST_Store && bt == VT_STRUCT)
		{ MCC_TRACE("br\n"); return 1; }
#endif
	return 0;
}

static int ast_cond_has_store(AstArena *a, AstLocal n, int depth) {
	MCC_TRACE("enter\n");
	if (n == AST_NONE || depth > 32)
		{ MCC_TRACE("br\n"); return 0; }
	{
		uint16_t k = ast_kind(a, n);
		if (k == AST_Store || k == AST_StoreVal)
			{ MCC_TRACE("br\n"); return 1; }
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_cond_has_store(a, c, depth + 1))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static void ast_promo_poison_off(int off, const int *coff, int nc,
																 int *cpoison) { MCC_TRACE("enter\n");
	for (int j = 0; j < nc; j++)
		{ MCC_TRACE("br\n"); if (coff[j] == off)
			{ MCC_TRACE("br\n"); cpoison[j] = 1; } }
}

static void ast_promo_poison_vla_size(AstArena *a, AstLocal n, const int *coff,
																			int nc, int *cpoison) {
	MCC_TRACE("enter\n");
	CType ct;
	ct.t = ast_type_t(a, n);
	ct.bp = ast_type_bp(a, n);
	ct.bs = ast_type_bs(a, n);
	ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
	for (int d = 0; d < 16 && ct.ref; d++) { MCC_TRACE("br\n");
		if ((ct.t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			if (ct.ref->a.has_vla_member)
				{ MCC_TRACE("br\n"); ast_promo_poison_off(ct.ref->vla_dyn_slot, coff, nc,
																									cpoison); }
			return;
		}
		if ((ct.t & VT_BTYPE) != VT_PTR)
			{ MCC_TRACE("br\n"); return; }
		if (ct.t & VT_VLA)
			{ MCC_TRACE("br\n"); ast_promo_poison_off((int)ct.ref->c, coff, nc,
																							 cpoison); }
		ct = ct.ref->type;
	}
}

static int ast_plan_promotion(AstArena *a) { MCC_TRACE("enter\n");
	ast_promo_n = 0;
	ast_promo_callful = 0;
	/* setjmp: longjmp restores exactly the callee-saved registers this pass
	 * promotes into, so any local it promotes loses every assignment made
	 * between the setjmp and the longjmp. See ast_body_has_setjmp(). */
	if (ast_body_has_setjmp(a))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_promote_env || ast_func_has_asm || ast_func_has_labeladdr)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal nn = ast_count(a);
	int has_call = 0, has_vla = 0, has_goto = 0, has_loop = 0, has_landor = 0;
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, n);
		if (k == AST_Invoke)
			{ MCC_TRACE("br\n"); has_call = 1; }
		else if (k == AST_Unary && ast_op(a, n) == AST_OP_VLA)
			{ MCC_TRACE("br\n"); has_vla = 1; }
		else if (k == AST_Jump && (ast_op(a, n) == 4 || ast_op(a, n) == 5))
			{ MCC_TRACE("br\n"); has_goto = 1; }
		else if (k == AST_BasicBlock) { MCC_TRACE("br\n");
			for (AstLocal s = ast_first_child(a, n); s != AST_NONE;
					 s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
				int so = ast_op(a, s);
				if (ast_kind(a, s) == AST_If &&
						(so == 2 || so == 3 || so == 4 || so == 5))
					{ MCC_TRACE("br\n"); has_loop = 1; }
				if (ast_kind(a, s) == AST_Binary &&
						(so == TOK_LAND || so == TOK_LOR))
					{ MCC_TRACE("br\n"); has_landor = 1; }
				if (ast_kind(a, s) == AST_If && (so == 2 || so == 3 || so == 4) &&
						ast_nchild(a, s) >= 1 &&
						ast_cond_has_store(a, ast_child(a, s, 0), 0))
					{ MCC_TRACE("br\n"); has_landor = 1; }
			}
		}
#if defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64)
		if ((ast_type_t(a, n) & VT_BTYPE) == VT_LDOUBLE)
			{ MCC_TRACE("br\n"); has_call = 1; }
#endif
		if (!has_call && ast_node_libcall(a, n))
			{ MCC_TRACE("br\n"); has_call = 1; }
	}
	if (has_vla)
		{ MCC_TRACE("br\n"); return 0; }
	if (has_landor)
		{ MCC_TRACE("br\n"); return 0; }
	if (has_call && (ast_no_callful_env || ast_no_callful_promo))
		{ MCC_TRACE("br\n"); return 0; }
	int coff[AST_PROMO_MAX * 8], ctyp[AST_PROMO_MAX * 8], cpoison[AST_PROMO_MAX * 8];
	int cweight[AST_PROMO_MAX * 8];
	int nc = 0;
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		int off = (int)(int64_t)ast_ival(a, n);
		int tt = ast_type_t(a, n);
		int bt = tt & VT_BTYPE;
		int scalar = (bt == VT_INT || bt == VT_LLONG || bt == VT_PTR ||
									bt == VT_FLOAT || bt == VT_DOUBLE) &&
								 !(tt & (VT_ARRAY | VT_BITFIELD | VT_VOLATILE));
		int j;
		for (j = 0; j < nc; j++)
			{ MCC_TRACE("br\n"); if (coff[j] == off)
				{ MCC_TRACE("br\n"); break; } }
		if (j == nc) { MCC_TRACE("br\n");
			if (nc >= (int)(sizeof coff / sizeof *coff))
				{ MCC_TRACE("br\n"); continue; }
			coff[nc] = off, ctyp[nc] = ast_type_t(a, n), cpoison[nc] = 0, nc++;
		}
		if (!scalar)
			{ MCC_TRACE("br\n"); cpoison[j] = 1; }
		else if (!(ctyp[j] & VT_BTYPE))
			{ MCC_TRACE("br\n"); ctyp[j] = ast_type_t(a, n); }
	}
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); ast_promo_poison_vla_size(a, n, coff, nc, cpoison); }
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Unary)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		int r = ast_op(a, c);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		int off = (int)(int64_t)ast_ival(a, c);
		int sz = 0, unbounded = 0;
		if (ast_op(a, n) == AST_OP_ADDR) { MCC_TRACE("br\n");
			CType ct, lt;
			int al;
			ct.t = ast_type_t(a, n);
			ct.bp = ast_type_bp(a, n);
			ct.bs = ast_type_bs(a, n);
			ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			/* Both of these can be handed a struct whose ref the arena did not
			 * carry -- for the pointer case it is the *pointee* that is refless,
			 * which the `ct.ref` test does not cover. type_size would then
			 * dereference a null Sym and take the compiler down; `-O2` on a
			 * variadic `unsigned __int256` did exactly that on arm64. An unknown
			 * size is not a small size, so widen to unbounded instead of
			 * guessing. */
			if ((ct.t & VT_BTYPE) == VT_PTR && ct.ref) { MCC_TRACE("br\n");
				if (ast_promo_size_unknown(&ct.ref->type))
					{ MCC_TRACE("br\n"); unbounded = 1; }
				else
					{ MCC_TRACE("br\n"); sz = type_size(&ct.ref->type, &al); }
			}
			lt.t = ast_type_t(a, c);
			lt.bp = ast_type_bp(a, c);
			lt.bs = ast_type_bs(a, c);
			lt.ref = (Sym *)(uintptr_t)ast_type_ref(a, c);
			if (ast_promo_size_unknown(&lt)) { MCC_TRACE("br\n");
				unbounded = 1;
			} else if ((lt.t & VT_BTYPE) != VT_PTR) { MCC_TRACE("br\n");
				int lsz = type_size(&lt, &al);
				if (lsz > sz)
					{ MCC_TRACE("br\n"); sz = lsz; }
			} else if (sz <= 1) { MCC_TRACE("br\n");
				unbounded = 1;
			}
		}
		int skip_arrow = ast_promo_arrow_env && ast_op(a, n) == AST_OP_MEMBER_ARROW;
		if (ast_promo_incdec_env &&
				(ast_op(a, n) == TOK_INC || ast_op(a, n) == TOK_DEC) &&
				ast_parent(a, n) != AST_NONE &&
				ast_kind(a, ast_parent(a, n)) == AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		for (int j = 0; j < nc; j++)
			{ MCC_TRACE("br\n"); if (!skip_arrow &&
					(coff[j] == off || (unbounded && coff[j] >= off) ||
					 (sz > 0 && coff[j] >= off && coff[j] < off + sz)))
				{ MCC_TRACE("br\n"); cpoison[j] = 1; } }
	}
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM) || (r & VT_LVAL))
			{ MCC_TRACE("br\n"); continue; }
		int off = (int)(int64_t)ast_ival(a, n);
		CType ct;
		ct.t = ast_type_t(a, n);
		ct.bp = ast_type_bp(a, n);
		ct.bs = ast_type_bs(a, n);
		ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		int al, sz = 8, sz_unknown = 0;
		if ((ct.t & VT_BTYPE) == VT_PTR && ct.ref)
			{ MCC_TRACE("br\n"); if (ast_promo_size_unknown(&ct.ref->type))
					{ MCC_TRACE("br\n"); sz_unknown = 1; }
				else
					{ MCC_TRACE("br\n"); sz = type_size(&ct.ref->type, &al); } }
		else if (ast_promo_size_unknown(&ct))
			{ MCC_TRACE("br\n"); sz_unknown = 1; }
		else if ((ct.t & VT_BTYPE) == VT_STRUCT || (ct.t & VT_ARRAY))
			{ MCC_TRACE("br\n"); sz = type_size(&ct, &al); }
		if (sz <= 0)
			{ MCC_TRACE("br\n"); sz = 8; }
		for (int j = 0; j < nc; j++)
			{ MCC_TRACE("br\n"); if (coff[j] >= off &&
					(sz_unknown || coff[j] < off + sz))
				{ MCC_TRACE("br\n"); cpoison[j] = 1; } }
	}
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		int r = ast_op(a, n), t = ast_type_t(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		if (!(t & VT_ARRAY) && (t & VT_BTYPE) != VT_STRUCT)
			{ MCC_TRACE("br\n"); continue; }
		CType ct;
		ct.t = t;
		ct.bp = ast_type_bp(a, n);
		ct.bs = ast_type_bs(a, n);
		ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		/* A struct-typed Ref whose ref the arena did not carry. type_size would
		 * dereference the null Sym (`*a = s->r`) and take the compiler down --
		 * `-O2` on a variadic `unsigned __int256` did exactly that on arm64,
		 * where gen_va_arg asks for the size on entry. An unknown size cannot be
		 * guessed at: the object may cover anything from its own offset up, so
		 * poison that whole range rather than the 8 bytes the fallback below
		 * would have assumed. */
		int al, size, size_unknown = 0;
		if ((t & VT_BTYPE) == VT_STRUCT && !ct.ref)
			{ MCC_TRACE("br\n"); size = 0; size_unknown = 1; }
		else
			{ MCC_TRACE("br\n"); size = type_size(&ct, &al); }
		if (size <= 0 && !size_unknown)
			{ MCC_TRACE("br\n"); size = 8; }
		int base = (int)(int64_t)ast_ival(a, n);
		for (int j = 0; j < nc; j++)
			{ MCC_TRACE("br\n"); if (coff[j] >= base &&
					(size_unknown || coff[j] < base + size))
				{ MCC_TRACE("br\n"); cpoison[j] = 1; } }
	}
	for (int j = 0; j < nc; j++)
		{ MCC_TRACE("br\n"); for (int t = 0; t < ast_ltemp_n; t++)
			{ MCC_TRACE("br\n"); if (coff[j] == ast_ltemp_off[t]) { MCC_TRACE("br\n");
				cpoison[j] = 1;
				break;
			} } }
	if (ast_chainstore_env) { MCC_TRACE("br\n");
		for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
			if (ast_kind(a, n) != AST_Store || !(ast_fbits(a, n) & 1u))
				{ MCC_TRACE("br\n"); continue; }
			AstLocal prev = AST_NONE, p = ast_parent(a, n);
			if (p == AST_NONE)
				{ MCC_TRACE("br\n"); continue; }
			for (AstLocal c = ast_first_child(a, p); c != AST_NONE && c != n;
					 c = ast_next_sib(a, c))
				{ MCC_TRACE("br\n"); prev = c; }
			if (prev == AST_NONE || ast_kind(a, prev) != AST_Store)
				{ MCC_TRACE("br\n"); continue; }
			AstLocal tgt = ast_child(a, prev, 0);
			if (tgt == AST_NONE || ast_kind(a, tgt) != AST_Ref)
				{ MCC_TRACE("br\n"); continue; }
			int toff = (int)(int64_t)ast_ival(a, tgt);
			for (int j = 0; j < nc; j++)
				{ MCC_TRACE("br\n"); if (coff[j] == toff)
					{ MCC_TRACE("br\n"); cpoison[j] = 1; } }
			AstLocal val = ast_child(a, prev, 1);
			if (val != AST_NONE && ast_kind(a, val) == AST_Ref &&
					(ast_op(a, val) & VT_VALMASK) == VT_LOCAL &&
					!(ast_op(a, val) & VT_SYM)) { MCC_TRACE("br\n");
				int voff = (int)(int64_t)ast_ival(a, val);
				for (int j = 0; j < nc; j++)
					{ MCC_TRACE("br\n"); if (coff[j] == voff)
						{ MCC_TRACE("br\n"); cpoison[j] = 1; } }
			}
		}
	}
	for (int j = 0; j < nc; j++)
		{ MCC_TRACE("br\n"); cweight[j] = 0; }
	ast_promo_weigh(a, ast_root(a), 0, coff, nc, cweight);
	ast_promo_callful = has_call;
	const int *gp_pool = has_call ? ast_promo_callee : ast_promo_caller;
	int gp_max = has_call ? AST_PROMO_CALLEE_N : AST_PROMO_CALLER_N;
	const int *xmm_pool = ast_promo_xmm;
	int xmm_max = has_call ? 0 : AST_PROMO_XMM_N;
	if (!has_call && ast_promo_leaf_xmm_env) { MCC_TRACE("br\n");
		xmm_pool = ast_promo_xmm_leaf;
		xmm_max = AST_PROMO_XMM_LEAF_N;
	}
	if (!has_call && ast_promo_leaf_callee_env) { MCC_TRACE("br\n");
		int k = 0;
		for (int i = 0; i < AST_PROMO_CALLER_N; i++)
			{ MCC_TRACE("br\n"); ast_promo_leaf_pool[k++] = ast_promo_caller[i]; }
		for (int i = 0; i < AST_PROMO_CALLEE_N; i++)
			{ MCC_TRACE("br\n"); ast_promo_leaf_pool[k++] = ast_promo_callee[i]; }
		gp_pool = ast_promo_leaf_pool;
		gp_max = k;
	}
	int gp_n = 0, xmm_n = 0;
	if (ast_color_env && !has_goto) { MCC_TRACE("br\n");
		int cfirst[AST_PROMO_MAX * 8], clast[AST_PROMO_MAX * 8];
		int cdeffirst[AST_PROMO_MAX * 8];
		for (int j = 0; j < nc; j++)
			{ MCC_TRACE("br\n"); cfirst[j] = -1, clast[j] = -1, cdeffirst[j] = 0; }
		for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
			if (ast_kind(a, n) != AST_Ref)
				{ MCC_TRACE("br\n"); continue; }
			int r = ast_op(a, n);
			if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
				{ MCC_TRACE("br\n"); continue; }
			int off = (int)(int64_t)ast_ival(a, n);
			for (int j = 0; j < nc; j++)
				{ MCC_TRACE("br\n"); if (coff[j] == off) { MCC_TRACE("br\n");
					if (cfirst[j] < 0)
						{ MCC_TRACE("br\n"); cfirst[j] = (int)n; }
					clast[j] = (int)n;
					break;
				} }
		}
		int first_branch = (int)nn;
		for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
			uint16_t k = ast_kind(a, n);
			if (k == AST_If || k == AST_Jump) { MCC_TRACE("br\n");
				first_branch = (int)n;
				break;
			}
		}
		for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
			if (ast_kind(a, n) != AST_Store)
				{ MCC_TRACE("br\n"); continue; }
			AstLocal tgt = ast_child(a, n, 0);
			if (tgt == AST_NONE || ast_kind(a, tgt) != AST_Ref)
				{ MCC_TRACE("br\n"); continue; }
			int off = (int)(int64_t)ast_ival(a, tgt);
			if ((int)n >= first_branch || ast_subtree_reads_local(a, ast_child(a, n, 1), off))
				{ MCC_TRACE("br\n"); continue; }
			for (int j = 0; j < nc; j++)
				{ MCC_TRACE("br\n"); if (coff[j] == off && cfirst[j] == (int)tgt)
					{ MCC_TRACE("br\n"); cdeffirst[j] = 1; } }
		}
		int lo[AST_PROMO_MAX * 8], hi[AST_PROMO_MAX * 8];
		for (int j = 0; j < nc; j++) { MCC_TRACE("br\n");
			lo[j] = cdeffirst[j] ? cfirst[j] : 0;
			hi[j] = clast[j];
		}
		if (has_loop) { MCC_TRACE("br\n");
			for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
				if (ast_kind(a, n) != AST_BasicBlock)
					{ MCC_TRACE("br\n"); continue; }
				for (AstLocal s = ast_first_child(a, n); s != AST_NONE;
						 s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
					int so = ast_op(a, s);
					if (ast_kind(a, s) != AST_If ||
							!(so == 2 || so == 3 || so == 4 || so == 5))
						{ MCC_TRACE("br\n"); continue; }
					int llo = (int)s, lhi = (int)s;
					ast_subtree_span(a, s, &llo, &lhi);
					for (int j = 0; j < nc; j++) { MCC_TRACE("br\n");
						if (cfirst[j] < 0)
							{ MCC_TRACE("br\n"); continue; }
						if (clast[j] < llo || cfirst[j] > lhi)
							{ MCC_TRACE("br\n"); continue; }
						if (llo < lo[j])
							{ MCC_TRACE("br\n"); lo[j] = llo; }
						if (lhi > hi[j])
							{ MCC_TRACE("br\n"); hi[j] = lhi; }
					}
				}
			}
		}
		int careg[AST_PROMO_MAX * 8];
		for (int j = 0; j < nc; j++)
			{ MCC_TRACE("br\n"); careg[j] = -1; }
		for (int cls = 0; cls < 2; cls++) { MCC_TRACE("br\n");
			const int *pool = cls == 0 ? gp_pool : xmm_pool;
			int kcol = cls == 0 ? gp_max : xmm_max;
			int idx[AST_COLOR_MAX], m = 0;
			for (int j = 0; j < nc && m < AST_COLOR_MAX; j++) { MCC_TRACE("br\n");
				if (cpoison[j] || coff[j] >= 0 || cfirst[j] < 0)
					{ MCC_TRACE("br\n"); continue; }
				if ((cls == 0) == (is_float(ctyp[j]) != 0))
					{ MCC_TRACE("br\n"); continue; }
				idx[m++] = j;
			}
			uint64_t adj[AST_COLOR_MAX];
			int cost[AST_COLOR_MAX], col[AST_COLOR_MAX];
			for (int i = 0; i < m; i++) { MCC_TRACE("br\n");
				adj[i] = 0;
				cost[i] = cweight[idx[i]];
			}
			for (int i = 0; i < m; i++)
				{ MCC_TRACE("br\n"); for (int j = i + 1; j < m; j++) { MCC_TRACE("br\n");
					int a1 = idx[i], b1 = idx[j];
					int disjoint = (hi[a1] < lo[b1]) || (hi[b1] < lo[a1]);
					if (!disjoint) { MCC_TRACE("br\n");
						adj[i] |= (uint64_t)1 << j;
						adj[j] |= (uint64_t)1 << i;
					}
				} }
			ast_color_graph(m, adj, cost, kcol, col);
			for (int i = 0; i < m; i++)
				{ MCC_TRACE("br\n"); if (col[i] >= 0)
					{ MCC_TRACE("br\n"); careg[idx[i]] = pool[col[i]]; } }
		}
		int ord[AST_PROMO_MAX * 8], no = 0;
		for (int j = 0; j < nc; j++)
			{ MCC_TRACE("br\n"); if (careg[j] >= 0)
				{ MCC_TRACE("br\n"); ord[no++] = j; } }
		for (int i = 0; i < no; i++)
			{ MCC_TRACE("br\n"); for (int j = i + 1; j < no; j++)
				{ MCC_TRACE("br\n"); if (lo[ord[j]] < lo[ord[i]]) { MCC_TRACE("br\n");
					int t = ord[i];
					ord[i] = ord[j];
					ord[j] = t;
				} } }
		for (int i = 0; i < no; i++) { MCC_TRACE("br\n");
			ast_promo_off[ast_promo_n] = coff[ord[i]];
			ast_promo_typ[ast_promo_n] = ctyp[ord[i]];
			ast_promo_reg[ast_promo_n] = careg[ord[i]];
			ast_promo_n++;
		}
		return ast_promo_n;
	}
	int colorable[AST_PROMO_MAX * 8];
	for (int j = 0; j < nc; j++)
		{ MCC_TRACE("br\n"); colorable[j] = 1; }
	if (ast_color_env) { MCC_TRACE("br\n");
		for (int cls = 0; cls < 2; cls++) { MCC_TRACE("br\n");
			int idx[AST_COLOR_MAX], m = 0;
			int kcol = cls == 0 ? gp_max : xmm_max;
			for (int j = 0; j < nc && m < AST_COLOR_MAX; j++) { MCC_TRACE("br\n");
				if (cpoison[j] || coff[j] >= 0)
					{ MCC_TRACE("br\n"); continue; }
				if ((cls == 0) == (is_float(ctyp[j]) != 0))
					{ MCC_TRACE("br\n"); continue; }
				idx[m++] = j;
			}
			uint64_t adj[AST_COLOR_MAX];
			int cost[AST_COLOR_MAX], col[AST_COLOR_MAX];
			uint64_t full = (m < 64) ? ((uint64_t)1 << m) - 1 : ~(uint64_t)0;
			for (int i = 0; i < m; i++) { MCC_TRACE("br\n");
				adj[i] = full & ~((uint64_t)1 << i);
				cost[i] = cweight[idx[i]];
			}
			ast_color_graph(m, adj, cost, kcol, col);
			for (int i = 0; i < m; i++)
				{ MCC_TRACE("br\n"); if (col[i] < 0)
					{ MCC_TRACE("br\n"); colorable[idx[i]] = 0; } }
		}
	}
	for (;;) { MCC_TRACE("br\n");
		int best = -1;
		for (int j = 0; j < nc; j++) { MCC_TRACE("br\n");
			if (cpoison[j] || coff[j] >= 0 || !colorable[j])
				{ MCC_TRACE("br\n"); continue; }
			if (is_float(ctyp[j]) ? (xmm_n >= xmm_max) : (gp_n >= gp_max))
				{ MCC_TRACE("br\n"); continue; }
			if (best < 0 || cweight[j] > cweight[best])
				{ MCC_TRACE("br\n"); best = j; }
		}
		if (best < 0)
			{ MCC_TRACE("br\n"); break; }
		ast_promo_off[ast_promo_n] = coff[best];
		ast_promo_typ[ast_promo_n] = ctyp[best];
		ast_promo_reg[ast_promo_n] =
				is_float(ctyp[best]) ? xmm_pool[xmm_n++] : gp_pool[gp_n++];
		ast_promo_n++;
		cpoison[best] = 1;
	}
	return ast_promo_n;
}

static int ast_promo_save_loc;
static int ast_promo_save_slot[AST_PROMO_SLOTS];
static int ast_promo_save_n;

static void ast_promo_save_plan(void) { MCC_TRACE("enter\n");
	if (ast_spill_share_env) { MCC_TRACE("br\n");
		ast_promo_save_n = 0;
		for (int i = 0; i < ast_promo_n; i++) { MCC_TRACE("br\n");
			int s = -1;
			for (int p = 0; p < i; p++)
				{ MCC_TRACE("br\n"); if (ast_promo_reg[p] == ast_promo_reg[i]) { MCC_TRACE("br\n");
					s = ast_promo_save_slot[p];
					break;
				} }
			ast_promo_save_slot[i] = s >= 0 ? s : ast_promo_save_n++;
		}
		if (ast_promo_callful && ast_promo_save_n < ast_promo_n)
			{ MCC_TRACE("br\n"); MCC_TRACE("spillshare slots=%d->%d\n", ast_promo_n, ast_promo_save_n); }
	} else { MCC_TRACE("br\n");
		for (int i = 0; i < ast_promo_n; i++)
			{ MCC_TRACE("br\n"); ast_promo_save_slot[i] = i; }
		ast_promo_save_n = ast_promo_n;
	}
}

static void ast_promo_save_sv(SValue *sv, int i) { MCC_TRACE("enter\n");
	memset(sv, 0, sizeof *sv);
	sv->type.t = VT_LLONG;
	sv->r = VT_LOCAL | VT_LVAL;
	sv->r2 = VT_CONST;
	sv->c.i = ast_promo_save_loc + 8 * ast_promo_save_slot[i];
}

static void ast_promo_write(int reg, CType *ct) { MCC_TRACE("enter\n");
	gen_cast(ct);
	if (reg_classes[reg]) { MCC_TRACE("br\n");
		ast_pinned_regs &= ~((uint64_t)1 << reg);
		gv(reg_classes[reg] & ~(MCC_RC_INT | MCC_RC_FLOAT));
		ast_pinned_regs |= ((uint64_t)1 << reg);
	} else { MCC_TRACE("br\n");
		gv(MCC_RC_INT);
		load(reg, vtop);
		vtop->r = reg;
		vtop->r2 = VT_CONST;
		vtop->c.i = 0;
		vtop->sym = NULL;
	}
}

static void ast_promo_store_reg(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	AstLocal t = ast_child(a, s, 0);
	CType tct;
	tct.t = ast_type_t(a, t) & ~(VT_ARRAY | VT_VLA);
	tct.bp = ast_type_bp(a, t);
	tct.bs = ast_type_bs(a, t);
	tct.ref = (Sym *)(uintptr_t)ast_type_ref(a, t);
	ast_promo_write(ast_promo_reg_of(a, t), &tct);
}

static void ast_promo_entry_init(void) { MCC_TRACE("enter\n");
	ast_promo_save_plan();
	int any_save = 0;
	for (int i = 0; i < ast_promo_n; i++)
		{ MCC_TRACE("br\n"); if (ast_promo_reg_is_callee(ast_promo_reg[i])) { MCC_TRACE("br\n");
			any_save = 1;
			break;
		} }
	if (any_save) { MCC_TRACE("br\n");
		SValue sv;
		ast_promo_save_loc = ast_alloc_temp_loc(8 * ast_promo_save_n, 8);
		for (int i = 0; i < ast_promo_n; i++) { MCC_TRACE("br\n");
			if (!ast_promo_reg_is_callee(ast_promo_reg[i]))
				{ MCC_TRACE("br\n"); continue; }
			int dup = 0;
			for (int p = 0; p < i; p++)
				{ MCC_TRACE("br\n"); if (ast_promo_save_slot[p] == ast_promo_save_slot[i]) { MCC_TRACE("br\n");
					dup = 1;
					break;
				} }
			if (dup)
				{ MCC_TRACE("br\n"); continue; }
			ast_promo_save_sv(&sv, i);
			store(ast_promo_reg[i], &sv);
		}
	}
	for (int i = 0; i < ast_promo_n; i++) { MCC_TRACE("br\n");
		int reg = ast_promo_reg[i];
		int shared = 0;
		for (int p = 0; p < i; p++)
			{ MCC_TRACE("br\n"); if (ast_promo_reg[p] == reg) { MCC_TRACE("br\n");
				shared = 1;
				break;
			} }
		if (shared)
			{ MCC_TRACE("br\n"); continue; }
		SValue sv;
		memset(&sv, 0, sizeof sv);
		sv.type.t = ast_promo_typ[i];
		sv.r = VT_LOCAL | VT_LVAL;
		sv.r2 = VT_CONST;
		sv.c.i = ast_promo_off[i];
		vpushv(&sv);
		ast_pinned_regs &= ~((uint64_t)1 << reg);
		if (reg_classes[reg])
			{ MCC_TRACE("br\n"); gv(reg_classes[reg] & ~(MCC_RC_INT | MCC_RC_FLOAT)); }
		else
			{ MCC_TRACE("br\n"); load(reg, vtop); }
		ast_pinned_regs |= ((uint64_t)1 << reg);
		vpop();
	}
}

static void ast_promo_exit_restore(void) { MCC_TRACE("enter\n");
	SValue sv;
	for (int i = ast_promo_n - 1; i >= 0; i--) { MCC_TRACE("br\n");
		if (!ast_promo_reg_is_callee(ast_promo_reg[i]))
			{ MCC_TRACE("br\n"); continue; }
		int dup = 0;
		for (int p = i + 1; p < ast_promo_n; p++)
			{ MCC_TRACE("br\n"); if (ast_promo_save_slot[p] == ast_promo_save_slot[i]) { MCC_TRACE("br\n");
				dup = 1;
				break;
			} }
		if (dup)
			{ MCC_TRACE("br\n"); continue; }
		ast_promo_save_sv(&sv, i);
		load(ast_promo_reg[i], &sv);
	}
}
#else
static int ast_plan_promotion(AstArena *a) { MCC_TRACE("enter\n");
	(void)a;
	return 0;
}
static int ast_promo_reg_of(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	(void)a;
	(void)n;
	return -1;
}
static void ast_promo_entry_init(void) { MCC_TRACE("enter\n");
}
static void ast_promo_exit_restore(void) { MCC_TRACE("enter\n");
}
static int ast_promo_n;
static int ast_promo_callful;
static int ast_promo_save_loc;
static int ast_promo_regpool_at(int i) { MCC_TRACE("enter\n");
	(void)i;
	return 0;
}
static int ast_promo_store_late(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	(void)a;
	(void)s;
	return 0;
}
static void ast_promo_store_reg(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	(void)a;
	(void)s;
}
#endif

static void ast_error_sink(void *opaque, const char *msg) { MCC_TRACE("enter\n");
	(void)opaque;
	if (mcc_env_on("MCC_RIR_ABORTWHY"))
		{ MCC_TRACE("br\n"); fprintf(stderr, "[rir-abort] %s: %s\n",
						funcname ? funcname : "?", msg ? msg : "?"); }
}

static int ast_val_has_call(AstArena *a, AstLocal n, int depth) {
	MCC_TRACE("enter\n");
	if (n == AST_NONE || depth > 32)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Invoke)
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_val_has_call(a, c, depth + 1))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_storeval_reload_ok(AstArena *a, AstLocal n, int depth) {
	MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 1; }
	if (depth <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	int k = ast_kind(a, n);
	if (k == AST_Invoke || k == AST_Store || k == AST_StoreVal ||
			k == AST_If || k == AST_Poison || (ast_type_t(a, n) & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	if (k == AST_Unary &&
			(ast_op(a, n) == AST_OP_VLA || ast_op(a, n) == AST_OP_VLA_RESTORE))
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_storeval_reload_ok(a, c, depth - 1))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static int ast_load_over_member(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal c = ast_nchild(a, n) == 1 ? ast_child(a, n, 0) : AST_NONE;
	while (c != AST_NONE && ast_kind(a, c) == AST_Convert && ast_nchild(a, c) == 1)
		{ MCC_TRACE("br\n"); c = ast_child(a, c, 0); }
	return c != AST_NONE && ast_kind(a, c) == AST_Unary &&
				 (ast_op(a, c) == AST_OP_MEMBER || ast_op(a, c) == AST_OP_MEMBER_ARROW);
}

/* T-win-50028: an indexed element of a reversed-scalar_storage_order array
 * member -- Load(Binary '+'(MEMBER[VT_REVSO,VT_ARRAY], index)).  The parser
 * saves the array member's VT_REVSO across the +/indir and re-applies it on the
 * element (mccgen.c:13998); replay must do the same or the element misses its
 * byte-swap.  Returns 1 if the load address is such an element. */
static int ast_addr_over_revso_member(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	while (n != AST_NONE && ast_kind(a, n) == AST_Convert && ast_nchild(a, n) == 1)
		n = ast_child(a, n, 0);
	if (n == AST_NONE || ast_kind(a, n) != AST_Binary ||
			ast_op(a, n) != '+' || ast_nchild(a, n) != 2)
		return 0;
	for (int i = 0; i < 2; i++) { MCC_TRACE("br\n");
		AstLocal m = ast_child(a, n, i);
		while (m != AST_NONE && ast_kind(a, m) == AST_Convert && ast_nchild(a, m) == 1)
			m = ast_child(a, m, 0);
		if (m != AST_NONE && ast_kind(a, m) == AST_Unary &&
				(ast_op(a, m) == AST_OP_MEMBER || ast_op(a, m) == AST_OP_MEMBER_ARROW) &&
				(ast_fbits(a, m) & (uint64_t)AST_FB_MEMBER_REVSO))
			return 1;
	}
	return 0;
}

static void ast_replay_value_inner(AstArena *a, AstLocal n);


static void ast_replay_value(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	const char *e = ast_rvattr_cached;
	int before;
	if (!e || !funcname || strcmp(e, funcname)) { MCC_TRACE("br\n");
		ast_replay_value_inner(a, n);
		return;
	}
	before = ind;
	ast_replay_value_inner(a, n);
	if (ind != before) { MCC_TRACE("br\n");
		fprintf(stderr, "[rv] n=%d %s op=%d %d..%d (+%d)\n", (int)n,
						ast_kind_name(ast_kind(a, n)), (int)ast_op(a, n), before, ind,
						ind - before);
	}
}

static void ast_replay_value_inner(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	MCC_TRACE_IF("RV n=%d kind=%d nchild=%d parent=%d ind=%d vtop=%d\n", (int)n,
							 (int)ast_kind(a, n), (int)ast_nchild(a, n), (int)ast_parent(a, n),
							 (int)ind, (int)(vtop - vstack));
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_BasicBlock: {
		AstLocal c, last = AST_NONE;
		for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
			{ MCC_TRACE("br\n"); last = c; }
		for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
			int d0;
			if (c == last) { MCC_TRACE("br\n");
				ast_replay_value(a, c);
				break;
			}
			d0 = (int)(vtop - vstack);
			if (ast_kind(a, c) == AST_Store && ast_nchild(a, c) == 2) { MCC_TRACE("br\n");
				if (ast_promo_store_late(a, c)) { MCC_TRACE("br\n");
					ast_replay_value(a, ast_child(a, c, 1));
					ast_promo_store_reg(a, c);
				} else { MCC_TRACE("br\n");
					ast_replay_value(a, ast_child(a, c, 0));
					ast_replay_value(a, ast_child(a, c, 1));
					vstore();
				}
			} else { MCC_TRACE("br\n");
				ast_replay_value(a, c);
			}
			while ((int)(vtop - vstack) > d0)
				{ MCC_TRACE("br\n"); vpop(); }
		}
		break;
	}
	case AST_Store: {
		if (ast_nchild(a, n) == 2) { MCC_TRACE("br\n");
			if (ast_promo_store_late(a, n)) { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, n, 1));
				ast_promo_store_reg(a, n);
			} else { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, n, 0));
				ast_replay_value(a, ast_child(a, n, 1));
				vstore();
			}
		}
		break;
	}
	case AST_StoreVal: {
		AstLocal st = (AstLocal)ast_ival(a, n);
		if (st != AST_NONE && st < a->count && ast_kind(a, st) == AST_Store) {
			MCC_TRACE("br\n");
			if (ast_fbits(a, st) & AST_FB_STORE_VALUE_LIVE) { MCC_TRACE("br\n");
				if (ast_fbits(a, st) & AST_FB_STOREVAL_CONST_LEFT)
					{ MCC_TRACE("br\n"); vswap(); }
				else if ((ast_fbits(a, st) & AST_FB_STORE_LIVE_ROT) &&
								 ast_sv_live_depth > 0) { MCC_TRACE("br\n");
					int here = (int)(vtop - vstack + 1);
					int k = here - ast_sv_live_depth;
					if (k > 0 && k < here)
						{ MCC_TRACE("br\n"); vrotb(k + 1); }
					ast_sv_live_depth = 0;
				}
				break;
			}
			if (ast_nchild(a, st) == 2) { MCC_TRACE("br\n");
				if (ast_parent(a, st) == AST_NONE)
					{ MCC_TRACE("br\n"); ast_replay_value(a, st); }
				else if (ast_val_has_call(a, ast_child(a, st, 1), 0) ||
								 ast_storeval_reload_ok(a, ast_child(a, st, 0), 16))
					{ MCC_TRACE("br\n"); ast_replay_value(a, ast_child(a, st, 0)); }
				else
					{ MCC_TRACE("br\n"); ast_replay_value(a, ast_child(a, st, 1)); }
			}
		}
		break;
	}
	case AST_Literal:
	case AST_Ref: {
		SValue sv;
		memset(&sv, 0, sizeof sv);
		sv.type.t = ast_type_t(a, n);
		sv.type.bp = ast_type_bp(a, n);
		sv.type.bs = ast_type_bs(a, n);
		sv.type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		sv.r = (unsigned short)ast_op(a, n);
		if (ast_op(a, n) & VT_REVSO)
			sv.r |= VT_REVSO;
		MCC_TRACE_IF("LEAF n=%d r=%#x t=%#x ival=%lld\n", (int)n, sv.r, sv.type.t,
								 (long long)ast_ival(a, n));
		sv.r2 = (unsigned short)ast_wide_r2(a, n);
		sv.c.i = ast_ival(a, n);
		sv.c.q.hi = ast_wide_hi(a, n);
		sv.sym = (Sym *)(uintptr_t)ast_sym(a, n);
		if (ast_fpert_on && !ast_in_graft && !(sv.r & VT_SYM) &&
				((sv.r & VT_VALMASK) == VT_LOCAL || (sv.r & VT_VALMASK) == VT_LLOCAL))
			{ MCC_TRACE("br\n"); sv.c.i = (uint64_t)(int64_t)ast_fpert_map((int)(int64_t)sv.c.i); }
		if ((sv.r & VT_VALMASK) == VT_LOCAL && !(sv.r & VT_SYM) &&
				(ast_inline_bias || ast_argsub_n)) { MCC_TRACE("br\n");
			int off = (int)sv.c.i, subst = 0;
			for (int j = 0; j < ast_argsub_n; j++)
				{ MCC_TRACE("br\n"); if (ast_argsub_off[j] == off) { MCC_TRACE("br\n");
					sv = ast_argsub_val[j];
					subst = 1;
					break;
				} }
			if (!subst)
				{ MCC_TRACE("br\n"); sv.c.i += ast_inline_bias; }
		} else if ((sv.r & VT_VALMASK) == VT_LLOCAL && !(sv.r & VT_SYM) &&
							 ast_in_graft) { MCC_TRACE("br\n");
			sv.c.i += ast_inline_bias;
		}
		if (ast_promo_n && !ast_in_graft) { MCC_TRACE("br\n");
			int preg = ast_promo_reg_of(a, n);
			if (preg >= 0) { MCC_TRACE("br\n");
				sv.r = (unsigned short)preg;
				sv.r2 = VT_CONST;
				sv.c.i = 0;
				sv.c.q.hi = 0;
				sv.sym = NULL;
			}
		}
		if (ast_cmp_mat_env)
			{ MCC_TRACE("br\n"); vcheck_cmp(); }
		vpushv(&sv);
		break;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		if (bop == AST_OP_CPLXBUILD) { MCC_TRACE("br\n");
			CType ccplx, cbase;
			SValue r;
			ccplx.t = ast_type_t(a, n);
			ccplx.bp = ast_type_bp(a, n);
			ccplx.bs = ast_type_bs(a, n);
			ccplx.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			cbase = ccplx.ref->next->type;
			ast_replay_value(a, ast_child(a, n, 0));
			ast_replay_value(a, ast_child(a, n, 1));
			cplx_local(&ccplx, &r);
			gen_cast(&cbase);
			cplx_store_part(&r, 1);
			gen_cast(&cbase);
			cplx_store_part(&r, 0);
			vpushv(&r);
			break;
		}
		if (bop == AST_OP_ACASRMW) { MCC_TRACE("br\n");
			uint32_t alow = (uint32_t)ast_ival(a, n);
			ast_replay_value(a, ast_child(a, n, 0));
			ast_replay_value(a, ast_child(a, n, 1));
			loc = (int)(uint32_t)(ast_ival(a, n) >> 32);
			gen_atomic_cas_rmw((int)(alow >> 1), (int)(alow & 1));
			break;
		}
#ifdef MCC_IR_HAVE_X86_PRIMS
		if (bop == AST_OP_AXADD || bop == AST_OP_AXCHG ||
				bop == AST_OP_ACMPXCHG) { MCC_TRACE("br\n");
			uint32_t nc = ast_nchild(a, n), k;
			for (k = 0; k < nc; k++)
				{ MCC_TRACE("br\n"); ast_replay_value(a, ast_child(a, n, k)); }
			if (bop == AST_OP_AXADD)
				{ MCC_TRACE("br\n"); gen_atomic_xadd((int)ast_ival(a, n)); }
			else if (bop == AST_OP_AXCHG)
				{ MCC_TRACE("br\n"); gen_atomic_xchg((int)ast_ival(a, n)); }
			else
				{ MCC_TRACE("br\n"); gen_atomic_cmpxchg((int)ast_ival(a, n)); }
			vswap();
			vpop();
			if (bop == AST_OP_ACMPXCHG) { MCC_TRACE("br\n");
				vswap();
				vpop();
			}
			if (ast_type_t(a, n)) { MCC_TRACE("br\n");
				vtop->type.t = ast_type_t(a, n);
				vtop->type.bp = ast_type_bp(a, n);
				vtop->type.bs = ast_type_bs(a, n);
				vtop->type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			}
			break;
		}
#endif
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_I386)
		if (bop == AST_OP_MULHU || bop == AST_OP_MULHS) { MCC_TRACE("br\n");
			ast_replay_value(a, ast_child(a, n, 0));
			ast_replay_value(a, ast_child(a, n, 1));
			gen_mulh(bop == AST_OP_MULHS);
			vtop->type.t = ast_type_t(a, n);
			vtop->type.bp = ast_type_bp(a, n);
			vtop->type.bs = ast_type_bs(a, n);
			vtop->type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			break;
		}
#endif
#if defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
		if (bop == AST_OP_COPYSIGN) { MCC_TRACE("br\n");
			ast_replay_value(a, ast_child(a, n, 0));
			ast_replay_value(a, ast_child(a, n, 1));
			gen_copysign();
			vtop->type.t = ast_type_t(a, n);
			vtop->type.bp = ast_type_bp(a, n);
			vtop->type.bs = ast_type_bs(a, n);
			vtop->type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			break;
		}
#endif
#if defined(MCC_TARGET_ARM64)
		if (bop == AST_OP_FMIN || bop == AST_OP_FMAX) { MCC_TRACE("br\n");
			ast_replay_value(a, ast_child(a, n, 0));
			ast_replay_value(a, ast_child(a, n, 1));
			gen_fminmax(bop == AST_OP_FMAX);
			vtop->type.t = ast_type_t(a, n);
			vtop->type.bp = ast_type_bp(a, n);
			vtop->type.bs = ast_type_bs(a, n);
			vtop->type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			break;
		}
#endif
#if defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64)
		if (bop == AST_OP_FMA) { MCC_TRACE("br\n");
			ast_replay_value(a, ast_child(a, n, 0));
			ast_replay_value(a, ast_child(a, n, 1));
			ast_replay_value(a, ast_child(a, n, 2));
			gen_fma();
			vtop->type.t = ast_type_t(a, n);
			vtop->type.bp = ast_type_bp(a, n);
			vtop->type.bs = ast_type_bs(a, n);
			vtop->type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			break;
		}
#endif
		if (bop == TOK_LAND || bop == TOK_LOR) { MCC_TRACE("br\n");
			int i = bop == TOK_LAND, t = 0;
			uint32_t nc = ast_nchild(a, n), k;
			if ((ast_fbits(a, n) & AST_FB_LANDOR_MATERIAL) &&
					!ast_rir_nomat_env) { MCC_TRACE("br\n");
				for (k = 0; k < nc; k++) { MCC_TRACE("br\n");
					ast_replay_value(a, ast_child(a, n, k));
					save_regs(1);
					t = gvtst(i, t);
				}
				vpushi((int)(int64_t)ast_ival(a, n));
				gsym(t);
				break;
			}
			for (k = 0; k < nc; k++) { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, n, k));
				save_regs(1);
				if (k + 1 < nc)
					{ MCC_TRACE("br\n"); t = gvtst(i, t); }
			}
			gvtst_set(i, t);
			if ((ast_fbits(a, n) & AST_FB_LANDOR_INVERT) &&
					!ast_rir_noinv_env) { MCC_TRACE("br\n");
				int jt = vtop->jfalse;
				vtop->jfalse = vtop->jtrue;
				vtop->jtrue = jt;
				vtop->cmp_op ^= 1;
			}
			break;
		}
		ast_replay_value(a, ast_child(a, n, 0));
		ast_replay_value(a, ast_child(a, n, 1));
		if (ast_fbits(a, n) & AST_FB_BINARY_RHS_GV)
			{ MCC_TRACE("br\n"); gv(USING_TWO_WORDS(vtop->type.t)
															? MCC_RC_RET(vtop->type.t)
															: MCC_RC_TYPE(vtop->type.t)); }

		if (ast_fbits(a, n) & AST_FB_JIT_GUARD)
			{ MCC_TRACE("br\n"); ast_reemit_guard_op = bop; }
		gen_op((ast_fbits(a, n) & AST_FB_CMP_INVERT_LATE) ? bop ^ 1 : bop);
		ast_reemit_guard_op = 0;
		if (ast_fbits(a, n) & AST_FB_CMP_INVERT_LATE) { MCC_TRACE("br\n");
			if (vtop->r == VT_CMP) { MCC_TRACE("br\n");
				int j = vtop->jfalse;
				vtop->jfalse = vtop->jtrue;
				vtop->jtrue = j;
				vtop->cmp_op ^= 1;
			} else if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
				vtop->c.i = !vtop->c.i;
			} else { MCC_TRACE("br\n");
				mcc_error("ast-replay: late comparison inversion lost");
			}
		}
		break;
	}
	case AST_Convert: {
		ast_replay_value(a, ast_child(a, n, 0));
		CType ct;
		ct.t = ast_type_t(a, n);
		ct.bp = ast_type_bp(a, n);
		ct.bs = ast_type_bs(a, n);
		ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		MCC_TRACE_IF("CVT from t=%#x r=%#x -> t=%#x fb=%#x\n", vtop->type.t, vtop->r,
								 ct.t, (unsigned)ast_fbits(a, n));
		gen_cast(&ct);
		if (ast_fbits(a, n) & AST_FB_CONVERT_FCS)
			{ MCC_TRACE("br\n"); vtop->type.t = VT_BOOL; }
		if (ast_fbits(a, n) & AST_FB_CONVERT_GV)
			{ MCC_TRACE("br\n"); gv_cast_rvalue(); }
		break;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		ast_replay_value(a, ast_child(a, n, 0));
		if (uop == AST_OP_ADDR) { MCC_TRACE("br\n");
			gaddrof();
			vtop->type.t = ast_type_t(a, n);
			vtop->type.bp = ast_type_bp(a, n);
			vtop->type.bs = ast_type_bs(a, n);
			vtop->type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		} else if (uop == AST_OP_MEMBER || uop == AST_OP_MEMBER_ARROW) { MCC_TRACE("br\n");
			if (uop == AST_OP_MEMBER_ARROW)
				{ MCC_TRACE("br\n"); indir(); }
			gaddrof();
			vtop->type = char_pointer_type;
			CType mt;
			mt.t = ast_type_t(a, n);
			mt.bp = ast_type_bp(a, n);
			mt.bs = ast_type_bs(a, n);
			mt.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			int cofs = (int)ast_ival(a, n);
			if (ast_regdisp_env && cofs && !(mt.t & VT_ARRAY) &&
					(vtop->r & VT_VALMASK) < VT_CONST &&
					!(vtop->r & (VT_SYM | VT_LVAL | VT_MUSTBOUND | VT_BOUNDED))) { MCC_TRACE("br\n");
				vtop->c.i = cofs;
				vtop->r |= VT_REGDISP;
			} else { MCC_TRACE("br\n");
				vpushi(cofs);
				gen_op('+');
			}
			vtop->type = mt;
			if (!(mt.t & VT_ARRAY)) { MCC_TRACE("br\n");
				uint64_t mfb = ast_fbits(a, n);
				vtop->r |= VT_LVAL | (int)(mfb & ~(uint64_t)AST_FB_MEMBER_REVSO);
				if (mfb & AST_FB_MEMBER_REVSO)
					{ MCC_TRACE("br\n"); vtop->r |= VT_REVSO; }
			}
		} else if (uop == AST_OP_FNEG) { MCC_TRACE("br\n");
			gen_opif(TOK_NEG);
			vtop->type.t = ast_type_t(a, n);
			vtop->type.bp = ast_type_bp(a, n);
			vtop->type.bs = ast_type_bs(a, n);
			vtop->type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
#if defined(MCC_IR_HAVE_VA_START) && !defined(MCC_IR_VA_START_VOID)
		} else if (uop == AST_OP_VASTART) { MCC_TRACE("br\n");
			gen_va_start();
#endif
#ifdef MCC_IR_HAVE_BSWAP
		} else if (uop == AST_OP_BSWAP) { MCC_TRACE("br\n");
			gen_bswap((int)ast_ival(a, n));
#endif
#if defined(MCC_TARGET_X86_64)
		} else if (uop == AST_OP_ROTL) { MCC_TRACE("br\n");
			if (ast_nchild(a, n) == 2) { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, n, 1));
				gen_rotl_var((int)(ast_ival(a, n) >> 8));
			} else { MCC_TRACE("br\n");
				gen_rotl((int)(ast_ival(a, n) >> 8), (int)(ast_ival(a, n) & 0xff));
			}
		} else if (uop == AST_OP_ROTR) { MCC_TRACE("br\n");
			ast_replay_value(a, ast_child(a, n, 1));
			gen_rotr_var((int)(ast_ival(a, n) >> 8));
#endif
#ifdef MCC_IR_HAVE_X86_PRIMS
		} else if (uop == AST_OP_SIGNBIT) { MCC_TRACE("br\n");
			gen_signbit((int)ast_ival(a, n));
		} else if (uop == AST_OP_FFS) { MCC_TRACE("br\n");
			gen_ffs((int)ast_ival(a, n));
		} else if (uop == AST_OP_BITSCAN) { MCC_TRACE("br\n");
			gen_bitscan((int)ast_ival(a, n), (int)(ast_ival(a, n) >> 32));
#endif
#ifdef MCC_IR_HAVE_VA_ARG
		} else if (uop == AST_OP_VAARG) { MCC_TRACE("br\n");
			CType vat;
			vat.t = ast_type_t(a, n);
			vat.bp = ast_type_bp(a, n);
			vat.bs = ast_type_bs(a, n);
			vat.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			gen_va_arg(&vat);
			vtop->type = vat;
#endif
		} else if (uop == AST_OP_BITB) { MCC_TRACE("br\n");
			gen_bit_builtin((int)ast_ival(a, n), (int)(ast_ival(a, n) >> 32));
		} else if (uop == AST_OP_IMAG) { MCC_TRACE("br\n");
			gen_imaginary_complex((int)ast_ival(a, n));
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_I386)
		} else if (uop == AST_OP_FABS) { MCC_TRACE("br\n");
			CType ct;
			ct.t = ast_type_t(a, n);
			ct.bp = ast_type_bp(a, n);
			ct.bs = ast_type_bs(a, n);
			ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			gen_cast(&ct);
			gen_fabs();
			vtop->type = ct;
		} else if (uop == AST_OP_SQRT) { MCC_TRACE("br\n");
			CType ct;
			ct.t = ast_type_t(a, n);
			ct.bp = ast_type_bp(a, n);
			ct.bs = ast_type_bs(a, n);
			ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			gen_cast(&ct);
			gen_sqrt();
			vtop->type = ct;
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
		} else if (uop == AST_OP_FLOOR || uop == AST_OP_CEIL ||
							 uop == AST_OP_TRUNC) { MCC_TRACE("br\n");
			CType ct;
			ct.t = ast_type_t(a, n);
			ct.bp = ast_type_bp(a, n);
			ct.bs = ast_type_bs(a, n);
			ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			gen_cast(&ct);
			gen_round(uop == AST_OP_FLOOR ? 0 : uop == AST_OP_CEIL ? 1 : 2);
			vtop->type = ct;
		} else if (uop == AST_OP_RINT || uop == AST_OP_NEARBYINT) { MCC_TRACE("br\n");
			CType ct;
			ct.t = ast_type_t(a, n);
			ct.bp = ast_type_bp(a, n);
			ct.bs = ast_type_bs(a, n);
			ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			gen_cast(&ct);
			gen_round(uop == AST_OP_RINT ? 4 : 5);
			vtop->type = ct;
#endif
#if defined(MCC_TARGET_ARM64)
		} else if (uop == AST_OP_ROUND) { MCC_TRACE("br\n");
			CType ct;
			ct.t = ast_type_t(a, n);
			ct.bp = ast_type_bp(a, n);
			ct.bs = ast_type_bs(a, n);
			ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			gen_cast(&ct);
			gen_round(3);
			vtop->type = ct;
#endif
#endif
		} else { MCC_TRACE("br\n");
			inc((int)ast_ival(a, n), uop);
		}
		break;
	}
	case AST_Load:
		ast_replay_value(a, ast_child(a, n, 0));
		if ((ast_fbits(a, n) & AST_FB_LOAD_LVAL) || !(vtop->r & VT_LVAL) ||
				!ast_load_over_member(a, n))
			{ MCC_TRACE("br\n"); indir();
				if (ast_addr_over_revso_member(a, ast_child(a, n, 0)) &&
						(vtop->r & VT_LVAL) &&
						!(vtop->type.t & (VT_ARRAY | VT_VLA)))
					{ MCC_TRACE("br\n"); vtop->r |= VT_REVSO; } }
		break;
	case AST_If: {
		SValue sv;
		CType type;
		int tt, u, rc, r1, r2;
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) ||                 \
		defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_I386) ||                 \
		defined(MCC_TARGET_ARM)
		if (ast_select_env && ast_ival(a, n) == AST_SEL_MARK) { MCC_TRACE("br\n");
			CType stype;
			SValue svt;
			ast_replay_value(a, ast_child(a, n, 0));
			gv(MCC_RC_INT);
			ast_replay_value(a, ast_child(a, n, 1));
			svt = *vtop;
			ast_replay_value(a, ast_child(a, n, 2));
			combine_types(&stype, &svt, vtop, '?');
			gen_select(&stype);
			break;
		}
#endif
		if (ast_op(a, n) == 9) { MCC_TRACE("br\n");
			int islv;
			ast_replay_value(a, ast_child(a, n, 0));
			save_regs(1);
			if (is_complex_type(&vtop->type)) { MCC_TRACE("br\n");
				CType ct = vtop->type, cbase = ct.ref->next->type;
				SValue ctmp;
				cplx_materialize(&ct, &cbase, &ctmp);
				vpushv(&ctmp);
				vpushv(&ctmp);
			} else
				{ MCC_TRACE("br\n"); gv_dup(); }
			u = gvtst(0, 0);
			sv = *vtop;
			vtop--;
			ast_replay_value(a, ast_child(a, n, 1));
			combine_types(&type, &sv, vtop, '?');
			islv = VT_STRUCT == (type.t & VT_BTYPE);
			gen_cast(&type);
			if (islv) { MCC_TRACE("br\n");
				mk_pointer(&vtop->type);
				gaddrof();
			}
			rc = MCC_RC_TYPE(type.t);
			if (USING_TWO_WORDS(type.t))
				{ MCC_TRACE("br\n"); rc = MCC_RC_RET(type.t); }
			r2 = gv_unpinned(rc, islv ? VT_PTR : type.t);
			tt = gjmp(0);
			gsym(u);
			*vtop = sv;
			gen_cast(&type);
			if (islv) { MCC_TRACE("br\n");
				mk_pointer(&vtop->type);
				gaddrof();
			}
			r1 = gv(rc);
			move_reg(r2, r1, islv ? VT_PTR : type.t);
			vtop->r = r2;
			gsym(tt);
			if (islv) { MCC_TRACE("br\n");
				indir();
				vtop->r |= VT_NONLVAL;
			}
			break;
		}
		ast_replay_value(a, ast_child(a, n, 0));
		save_regs(1);
		tt = gvtst(1, 0);
		ast_replay_value(a, ast_child(a, n, 1));
		if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); mk_pointer(&vtop->type); }
		sv = *vtop;
		vtop--;
		u = gjmp(0);
		gsym(tt);
		ast_replay_value(a, ast_child(a, n, 2));
		if ((vtop->type.t & VT_BTYPE) == VT_FUNC)
			{ MCC_TRACE("br\n"); mk_pointer(&vtop->type); }
		combine_types(&type, &sv, vtop, '?');
		int islv = VT_STRUCT == (type.t & VT_BTYPE);
		gen_cast(&type);
		if (islv) { MCC_TRACE("br\n");
			mk_pointer(&vtop->type);
			gaddrof();
		}
		rc = MCC_RC_TYPE(type.t);
		if (USING_TWO_WORDS(type.t))
			{ MCC_TRACE("br\n"); rc = MCC_RC_RET(type.t); }
		r2 = gv_unpinned(rc, islv ? VT_PTR : type.t);
		tt = gjmp(0);
		gsym(u);
		*vtop = sv;
		gen_cast(&type);
		if (islv) { MCC_TRACE("br\n");
			mk_pointer(&vtop->type);
			gaddrof();
		}
		r1 = gv(rc);
		move_reg(r2, r1, islv ? VT_PTR : type.t);
		vtop->r = r2;
		gsym(tt);
		if (islv)
			{ MCC_TRACE("br\n"); indir(); }
		break;
	}
	case AST_Invoke: {
		uint32_t nc = ast_nchild(a, n);
		int live_arg = (ast_fbits(a, n) & AST_FB_CALL_STOREVAL_ARG) != 0;
		int rir_pre_sret = 0;
		if (!live_arg && ast_inline_graft(a, n)) { MCC_TRACE("br\n");
			if (ast_type_t(a, n) == VT_VOID)
				{ MCC_TRACE("br\n"); vpop(); }
			break;
		}
		if (rir_c2_active && (ast_type_t(a, n) & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			CType prt, prtmp;
			int prax, prsx;
			prt.t = ast_type_t(a, n);
			prt.bp = ast_type_bp(a, n);
			prt.bs = ast_type_bs(a, n);
			prt.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			if (gfunc_sret(&prt, 0, &prtmp, &prax, &prsx) <= 0) { MCC_TRACE("br\n");
				int psal, pssz = type_size(&prt, &psal);
#ifdef MCC_TARGET_ARM64
				if (pssz < 16)
					{ MCC_TRACE("br\n"); while (pssz & (pssz - 1))
						{ MCC_TRACE("br\n"); pssz = (pssz | (pssz - 1)) + 1; } }
#endif
				ast_alloc_loc(pssz, psal);
				rir_pre_sret = 1;
			}
		}
		for (uint32_t i = 0; i < nc; i++) { MCC_TRACE("br\n");
			ast_replay_value(a, ast_child(a, n, i));
			if (i == 0 && ast_indirect_call_env &&
					(vtop->type.t & VT_BTYPE) != VT_FUNC &&
					(vtop->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR &&
					vtop->type.ref &&
					(vtop->type.ref->type.t & VT_BTYPE) == VT_FUNC)
				{ MCC_TRACE("br\n"); vtop->type = *pointed_type(&vtop->type); }
			if (i == 0 && live_arg) { MCC_TRACE("br\n");
				int sv_need = (ast_fbits(a, n) & AST_FB_CALL_STOREVAL_STORE) ? 3 : 2;
				if (vtop - vstack + 1 < sv_need)
					{ MCC_TRACE("br\n"); mcc_error("ast-replay: storeval-arg stack underflow"); }
				if (ast_fbits(a, n) & AST_FB_CALL_STOREVAL_STORE)
					{ MCC_TRACE("br\n"); vrotb(3); }
				else
					{ MCC_TRACE("br\n"); vswap(); }
			}
		}
		vcheck_cmp();
		{
			SValue *fnv = vtop - ((int)nc - 1);
			if (fnv < vstack || !fnv->type.ref ||
					((fnv->type.t & VT_BTYPE) != VT_FUNC &&
					 (fnv->type.t & VT_BTYPE) != VT_PTR))
				{ MCC_TRACE("br\n"); mcc_error("ast-replay: call target lost its function type"); }
		}
		gfunc_call((int)nc - 1);
		if (ast_fbits(a, n) & AST_FB_CALL_NORETURN)
			{ MCC_TRACE("br\n"); CODE_OFF(); }
		if (ast_type_t(a, n) == VT_VOID)
			{ MCC_TRACE("br\n"); break; }
		if ((ast_type_t(a, n) & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			CType rt;
			SValue ret;
			int ret_nregs, regsize, ret_align, r, nn, size, align, addr, offset;
			rt.t = ast_type_t(a, n);
			rt.bp = ast_type_bp(a, n);
			rt.bs = ast_type_bs(a, n);
			rt.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			{
				CType rtmp;
				int rax, rsx, rnx;
				rnx = gfunc_sret(&rt, 0, &rtmp, &rax, &rsx);
				if (rnx <= 0) { MCC_TRACE("br\n");
					int sal, ssz = type_size(&rt, &sal);
#ifdef MCC_TARGET_ARM64
					if (ssz < 16)
						{ MCC_TRACE("br\n"); while (ssz & (ssz - 1))
							{ MCC_TRACE("br\n"); ssz = (ssz | (ssz - 1)) + 1; } }
#endif
					if (!rir_pre_sret)
						{ MCC_TRACE("br\n"); ast_alloc_loc(ssz, sal); }
					SValue sv;
					memset(&sv, 0, sizeof sv);
					sv.type.t = ast_type_t(a, n);
					sv.type.bp = ast_type_bp(a, n);
					sv.type.bs = ast_type_bs(a, n);
					sv.type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
					sv.r = (unsigned short)ast_op(a, n);
					sv.r2 = VT_CONST;
					sv.c.i = ast_ival(a, n);
					sv.sym = (Sym *)(uintptr_t)ast_sym(a, n);
					vpushv(&sv);
#if defined(MCC_TARGET_RISCV64) || (defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE))
					if (rnx < 0) { MCC_TRACE("br\n");
						unsigned short rsv = vtop->r;
						vtop->r &= (unsigned short)~VT_NONLVAL;
						arch_transfer_ret_regs(1);
						vtop->r = rsv;
					}
#endif
					vtop->r |= VT_NONLVAL;
					break;
				}
			}
			memset(&ret, 0, sizeof ret);
			ret_nregs = gfunc_sret(&rt, 0, &ret.type, &ret_align, &regsize);
			ret.c.i = 0;
			PUT_R_RET(&ret, ret.type.t);
			nn = ret_nregs;
			while (nn > 1) { MCC_TRACE("br\n");
				int rc = reg_classes[ret.r] & ~(MCC_RC_INT | MCC_RC_FLOAT);
				rc <<= --nn;
				for (r = 0; r < MCC_NB_REGS; ++r)
					{ MCC_TRACE("br\n"); if (reg_classes[r] & rc)
						{ MCC_TRACE("br\n"); break; } }
				vsetc(&ret.type, r, &ret.c);
			}
			vsetc(&ret.type, ret.r, &ret.c);
			vtop->r2 = ret.r2;
			size = type_size(&rt, &align);
			size = (size + regsize - 1) & -regsize;
			if (ret_align > align)
				{ MCC_TRACE("br\n"); align = ret_align; }
			loc = ast_alloc_loc(size, align);
			addr = loc;
			offset = 0;
			for (;;) { MCC_TRACE("br\n");
				vset(&ret.type, VT_LOCAL | VT_LVAL, addr + offset);
				vswap();
				vstore();
				vtop--;
				if (--ret_nregs == 0)
					{ MCC_TRACE("br\n"); break; }
				offset += regsize;
			}
			vset(&rt, VT_LOCAL | VT_LVAL, addr);
			vtop->r |= VT_NONLVAL;
			break;
		}
		SValue sv;
		memset(&sv, 0, sizeof sv);
		sv.type.t = ast_type_t(a, n);
		sv.type.bp = ast_type_bp(a, n);
		sv.type.bs = ast_type_bs(a, n);
		sv.type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		sv.r = (unsigned short)ast_op(a, n);
		sv.r2 = (unsigned short)ast_wide_r2(a, n);
		sv.c.i = ast_ival(a, n);
		sv.c.q.hi = ast_wide_hi(a, n);
		sv.sym = (Sym *)(uintptr_t)ast_sym(a, n);
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_ARM)
		if (sv.r2 == AST_R2_NONE && (sv.type.t & VT_BTYPE) == VT_LLONG) {
			MCC_TRACE("br\n");
			sv.r = REG_IRET;
			sv.r2 = REG_IRE2;
		}
#endif
		vpushv(&sv);
		break;
	}
	default:
		break;
	}
}

static struct ast_rp_label *ast_rp_label_get(int v) { MCC_TRACE("enter\n");
	for (int i = ast_rp_label_floor; i < ast_rp_nlabel; i++)
		{ MCC_TRACE("br\n"); if (ast_rp_labels[i].v == v)
			{ MCC_TRACE("br\n"); return &ast_rp_labels[i]; } }
	if (ast_rp_nlabel == ast_rp_caplabel) { MCC_TRACE("br\n");
		ast_rp_caplabel = ast_rp_caplabel ? ast_rp_caplabel * 2 : 8;
		ast_rp_labels =
				mcc_realloc(ast_rp_labels, ast_rp_caplabel * sizeof *ast_rp_labels);
	}
	struct ast_rp_label *l = &ast_rp_labels[ast_rp_nlabel++];
	l->v = v;
	l->jind = l->jnext = l->defined = 0;
	return l;
}

#ifdef MCC_IR_HAVE_X86_PRIMS
static int ast_has_atomic(AstArena *a, AstLocal n, int depth) { MCC_TRACE("enter\n");
	uint32_t i, nc;
	int op;
	if (n == AST_NONE || depth > 8)
		{ MCC_TRACE("br\n"); return 0; }
	op = ast_op(a, n);
	if (ast_kind(a, n) == AST_Binary &&
			(op == AST_OP_AXADD || op == AST_OP_AXCHG || op == AST_OP_ACMPXCHG))
		{ MCC_TRACE("br\n"); return 1; }
	nc = ast_nchild(a, n);
	for (i = 0; i < nc; i++) { MCC_TRACE("br\n");
		if (ast_has_atomic(a, ast_child(a, n, i), depth + 1))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}
#endif

static void ast_sattr_begin(const AstArena *a) { MCC_TRACE("enter\n");
	AstLocal n = a ? ast_count(a) : 0;
	if (n > ast_sattr_cap) { MCC_TRACE("br\n");
		ast_sattr = mcc_realloc(ast_sattr, (size_t)n * sizeof *ast_sattr);
		ast_sattr_cap = n;
	}
	if (n)
		{ MCC_TRACE("br\n"); memset(ast_sattr, 0, (size_t)n * sizeof *ast_sattr); }
	ast_sattr_arena = a;
}

static void ast_sattr_note(const AstArena *a, AstLocal s, int bytes) { MCC_TRACE("enter\n");
	if (a != ast_sattr_arena || s >= ast_sattr_cap)
		{ MCC_TRACE("br\n"); return; }
	ast_sattr[s] += bytes;
}

/* Hoisted out of ast_replay_bb, which is recursive: SValue
 * sv_stack[VSTACK_SIZE + 1] is 41,040 bytes -- measured, 80 * 513, not the
 * 32,832 recorded here until 2026-08-13, which was a 64-byte SValue two
 * growths ago -- of that function's frame,
 * and C allocates the whole frame at entry even though this arm is the only
 * user, so every level of the recursion paid for the inline-asm path. A
 * separate noinline callee pays it once, and only when ASMGEN actually
 * fires. It must not be a file-scope buffer: mcc_error longjmps straight
 * out of here, and a shared buffer would then be clobbered for whatever
 * outer frame catches it. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((noinline))
#endif
static void ast_replay_asmgen(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	ASMOperand ops[MAX_ASM_OPERANDS];
	uint8_t cr[MCC_NB_ASM_REGS];
	const unsigned char *p = ir_cap_raw + (int)(ast_ival(a, s) & 0xffffffff);
	int nb_operands, nb_outputs;
	SValue sv_stack[VSTACK_SIZE + 1];
	SValue *sv_top = vtop;
	int vs_off = (int)(ast_fbits(a, s) & 0xffffffff);
	int vs_n = (int)(ast_fbits(a, s) >> 32);
	memcpy(sv_stack, vstack, sizeof sv_stack);
	if (vs_n > 0 && vs_n <= VSTACK_SIZE) { MCC_TRACE("br\n");
		memcpy(vstack, ir_cap_vs + vs_off, (size_t)vs_n * sizeof(SValue));
		vtop = vstack + vs_n - 1;
	}
	int hdr[4], nall;
	memcpy(hdr, p, sizeof hdr);
	p += sizeof hdr;
	nb_operands = hdr[0];
	nb_outputs = hdr[1];
	nall = hdr[0] + hdr[2];
	if (nall > 0)
		{ MCC_TRACE("br\n"); memcpy(ops, p, (size_t)nall * sizeof *ops); }
	p += (size_t)nall * sizeof *ops;
	memcpy(cr, p, sizeof cr);
	asm_gen_code(ops, nb_operands, nb_outputs,
							 (int)(ast_sym(a, s) & 0xffffffff), cr,
							 (int)(ast_sym(a, s) >> 32));
	memcpy(vstack, sv_stack, sizeof sv_stack);
	vtop = sv_top;
	if ((int)(ast_sym(a, s) & 0xffffffff) && ast_rp_asmops > 0) {
		MCC_TRACE("br\n");
		while (ast_rp_asmops-- > 0)
			{ MCC_TRACE("br\n"); vpop(); }
		ast_rp_asmops = 0;
	}
}

static void ast_replay_bb(AstArena *a, AstLocal bb) { MCC_TRACE("enter\n");
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE;
			 s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		int ast_sattr_ind0 = ast_sattr_arena ? ind : 0;
		if (ast_kind(a, s) == AST_If && (ast_fbits(a, s) & AST_FB_NOCODE))
			{ MCC_TRACE("br\n"); continue; }
		switch (ast_kind(a, s)) { MCC_TRACE("br\n");
		case AST_Store: {
			if (ast_fbits(a, s) & AST_FB_STORE_CHAIN_SKIP)
				{ MCC_TRACE("br\n"); break; }
			if (ast_fbits(a, s) & AST_FB_STORE_CHAIN_MEMBER) { MCC_TRACE("br\n");
				AstLocal outer = ast_next_sib(a, s);
				ast_replay_value(a, ast_child(a, outer, 0));
				ast_replay_value(a, ast_child(a, s, 0));
				ast_replay_value(a, ast_child(a, s, 1));
				vstore();
				vstore();
				if (ast_fbits(a, outer) & AST_FB_STORE_VALUE_LIVE)
					{ MCC_TRACE("br\n"); ast_sv_live_depth = (int)(vtop - vstack + 1); break; }
				vpop();
				break;
			}
			if (ast_fbits(a, s) & AST_FB_STORE_CHAIN_REUSE) { MCC_TRACE("br\n");
#if (defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64))
				int cr_preg = (ast_promo_n && !ast_in_graft)
													? ast_promo_reg_of(a, ast_child(a, s, 0))
													: -1;
				if (cr_preg >= 0) { MCC_TRACE("br\n");
					CType crt;
					crt.t = ast_type_t(a, ast_child(a, s, 0)) & ~(VT_ARRAY | VT_VLA);
					crt.bp = ast_type_bp(a, ast_child(a, s, 0));
					crt.bs = ast_type_bs(a, ast_child(a, s, 0));
					crt.ref = (Sym *)(uintptr_t)ast_type_ref(a, ast_child(a, s, 0));
					ast_promo_write(cr_preg, &crt);
					if (ast_fbits(a, s) & AST_FB_STORE_VALUE_LIVE)
						{ MCC_TRACE("br\n"); ast_sv_live_depth = (int)(vtop - vstack + 1); break; }
					vpop();
					break;
				}
#endif
				ast_replay_value(a, ast_child(a, s, 0));
				vswap();
				vstore();
				if (ast_fbits(a, s) & AST_FB_STORE_VALUE_LIVE)
					{ MCC_TRACE("br\n"); ast_sv_live_depth = (int)(vtop - vstack + 1); break; }
				vpop();
				break;
			}
#if (defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64))
			int preg = (ast_promo_n && !ast_in_graft)
										 ? ast_promo_reg_of(a, ast_child(a, s, 0))
										 : -1;
			if (preg >= 0) { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, s, 1));
				CType tct;
				tct.t = ast_type_t(a, ast_child(a, s, 0)) & ~(VT_ARRAY | VT_VLA);
				tct.bp = ast_type_bp(a, ast_child(a, s, 0));
				tct.bs = ast_type_bs(a, ast_child(a, s, 0));
				tct.ref = (Sym *)(uintptr_t)ast_type_ref(a, ast_child(a, s, 0));
				ast_promo_write(preg, &tct);
				if (ast_storeval_call_env && (ast_fbits(a, s) & AST_FB_STORE_VALUE_LIVE))
					{ MCC_TRACE("br\n"); ast_sv_live_depth = (int)(vtop - vstack + 1); break; }
				vpop();
				break;
			}
#endif
			if (ast_op(a, s) == AST_OP_OPASSIGN) { MCC_TRACE("br\n");
				AstLocal c0 = ast_child(a, s, 0), c1 = ast_child(a, s, 1);
				if (c1 != AST_NONE && ast_kind(a, c1) == AST_Binary &&
						ast_nchild(a, c1) == 2 &&
						ast_struct_eq(a, c0, ast_child(a, c1, 0), 16) &&
						ast_expr_pure(a, c0, 16)) { MCC_TRACE("br\n");
					ast_replay_value(a, c0);
					vpushv(vtop);
					ast_replay_value(a, ast_child(a, c1, 1));
					gen_op(ast_op(a, c1));
					vstore();
					if (ast_storeval_call_env && (ast_fbits(a, s) & AST_FB_STORE_VALUE_LIVE))
						{ MCC_TRACE("br\n"); ast_sv_live_depth = (int)(vtop - vstack + 1); break; }
					vpop();
					break;
				}
			}
			if (ast_fbits(a, s) & AST_FB_STORE_ADDR_LATE) { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, s, 1));
				ast_replay_value(a, ast_child(a, s, 0));
				vswap();
			} else { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, s, 0));
				ast_replay_value(a, ast_child(a, s, 1));
			}
			if (ast_fbits(a, s) & AST_FB_STORE_CMP_GV)
				{ MCC_TRACE("br\n"); vcheck_cmp(); }
			vstore();
			if (ast_fbits(a, s) & AST_FB_STORE_VALUE_LIVE)
				{ MCC_TRACE("br\n"); ast_sv_live_depth = (int)(vtop - vstack + 1); break; }
			if ((ast_fbits(a, s) & AST_FB_STORE_BF_GV) && (vtop->r & VT_LVAL) &&
					!nocode_wanted)
				{ MCC_TRACE("br\n"); gv(MCC_RC_TYPE(vtop->type.t)); }
			vpop();
			break;
		}
		case AST_BasicBlock:
			ast_replay_bb(a, s);
			break;
		case AST_Load:
			if (ast_fbits(a, s) & AST_FB_STMT_DISCARD) { MCC_TRACE("br\n");
				ast_replay_value(a, s);
				vpop();
			}
			break;
		case AST_Binary:
#ifdef MCC_IR_VA_START_VOID
			if (ast_op(a, s) == AST_OP_VASTART) { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, s, 0));
				ast_replay_value(a, ast_child(a, s, 1));
				gen_va_start();
				break;
			}
#endif
			if (ast_fbits(a, s) & (AST_FB_LANDOR_MATERIAL | AST_FB_STMT_DISCARD)) { MCC_TRACE("br\n");
				ast_replay_value(a, s);
				vpop();
				break;
			}
			if (ast_op(a, s) == AST_OP_ACASRMW
#ifdef MCC_IR_HAVE_X86_PRIMS
					|| ast_has_atomic(a, s, 0)
#endif
			) { MCC_TRACE("br\n");
				ast_replay_value(a, s);
				vpop();
			}
			break;
		case AST_Convert:
			if (
#ifdef MCC_IR_HAVE_X86_PRIMS
					ast_has_atomic(a, s, 0) ||
#endif
					(ast_fbits(a, s) & AST_FB_STMT_DISCARD) ||
					(ast_nchild(a, s) == 1 &&
					 ast_kind(a, ast_child(a, s, 0)) == AST_Invoke)) { MCC_TRACE("br\n");
				ast_replay_value(a, s);
				vpop();
			}
			break;
		case AST_Invoke:
			ast_replay_value(a, s);
			if (ast_type_t(a, s) != VT_VOID)
				{ MCC_TRACE("br\n"); vpop(); }
			break;
		case AST_Unary:
#if (defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64))
			if ((ast_op(a, s) == TOK_INC || ast_op(a, s) == TOK_DEC) &&
					ast_promo_incdec_env && ast_promo_n && !ast_in_graft) { MCC_TRACE("br\n");
				AstLocal ic = ast_first_child(a, s);
				int preg = ast_promo_reg_of(a, ic);
				if (preg >= 0) { MCC_TRACE("br\n");
					CType ict;
					ict.t = ast_type_t(a, ic) & ~(VT_ARRAY | VT_VLA);
					ict.bp = ast_type_bp(a, ic);
					ict.bs = ast_type_bs(a, ic);
					ict.ref = (Sym *)(uintptr_t)ast_type_ref(a, ic);
					SValue isv;
					isv.type = ict;
					isv.r = (unsigned short)preg;
					isv.r2 = VT_CONST;
					isv.c.i = 0;
					isv.sym = NULL;
					vpushv(&isv);
					vpushi(ast_op(a, s) - TOK_MID);
					gen_op('+');
					ast_promo_write(preg, &ict);
					vpop();
					break;
				}
			}
#endif
			if (ast_op(a, s) == AST_OP_VLA) { MCC_TRACE("br\n");
				CType vt;
				vt.t = ast_type_t(a, s);
				vt.bp = ast_type_bp(a, s);
				vt.bs = ast_type_bs(a, s);
				vt.ref = (Sym *)(uintptr_t)ast_type_ref(a, s);
				int addr = (int)(int64_t)ast_ival(a, s);
				int locorig = (int)(int64_t)ast_sym(a, s);
				int al;
				if (ast_fbits(a, s))
					{ MCC_TRACE("br\n"); gen_vla_sp_save(locorig); }
				vpush_type_size(&vt, &al);
				if (ast_wide_hi(a, s))
					{ MCC_TRACE("br\n"); al = (int)(unsigned)ast_wide_hi(a, s); }
				gen_vla_alloc(&vt, al);
#ifdef MCC_IR_HAVE_VLA_RESULT
				{
					int vres = (int)(uint32_t)(ast_ival(a, s) >> 32);
					if (vres)
						{ MCC_TRACE("br\n"); gen_vla_result(vres); }
				}
#endif
				gen_vla_sp_save(addr);
				break;
			}
			if (ast_op(a, s) == AST_OP_VLA_RESTORE) { MCC_TRACE("br\n");
				gen_vla_sp_restore((int)(int64_t)ast_ival(a, s));
				break;
			}
			if (ast_op(a, s) == AST_OP_GGOTO) { MCC_TRACE("br\n");
				ast_replay_value(a, ast_child(a, s, 0));
				ggoto();
				break;
			}
			if (ast_op(a, s) == AST_OP_ASMOPS) { MCC_TRACE("br\n");
				uint32_t k, nc = ast_nchild(a, s);
				uint64_t gvmask = ast_ival(a, s);
				for (k = 0; k < nc; k++) { MCC_TRACE("br\n");
					ast_replay_value(a, ast_child(a, s, k));
					if (k < 64 && (gvmask >> k) & 1)
						{ MCC_TRACE("br\n"); gv(MCC_RC_INT); }
				}
				save_regs(0);
				ast_rp_asmops = (int)nc;
				break;
			}
			if (ast_op(a, s) == AST_OP_ASMGEN) { MCC_TRACE("br\n");
				ast_replay_asmgen(a, s);
				break;
			}
			if (ast_op(a, s) == AST_OP_ASM) { MCC_TRACE("br\n");
				int clen = (int)(ast_ival(a, s) >> 32);
				int coff = (int)(ast_ival(a, s) & 0xffffffff);
				int rlen = (int)(ast_fbits(a, s) >> 32);
				int roff = (int)(ast_fbits(a, s) & 0xffffffff);
				if (clen > 0) { MCC_TRACE("br\n");
					if (ind + clen > cur_text_section->data_allocated)
						{ MCC_TRACE("br\n"); section_realloc(cur_text_section, ind + clen); }
					memcpy(cur_text_section->data + ind, ir_cap_raw + coff, (size_t)clen);
					ind += clen;
				}
				if (rlen > 0 && cur_text_section->reloc) { MCC_TRACE("br\n");
					void *rp = section_ptr_add(cur_text_section->reloc, rlen);
					memcpy(rp, ir_cap_raw + roff, (size_t)rlen);
				}
				break;
			}
			ast_replay_value(a, s);
			vpop();
			break;
		case AST_Jump:
			if (ast_op(a, s) == 4) { MCC_TRACE("br\n");
				struct ast_rp_label *l = ast_rp_label_get((int)ast_ival(a, s));
				gsym(l->jnext);
				l->jnext = 0;
				l->jind = gind();
				l->defined = 1;
			} else if (ast_op(a, s) == 5) { MCC_TRACE("br\n");
				struct ast_rp_label *l = ast_rp_label_get((int)ast_ival(a, s));
				if (l->defined)
					{ MCC_TRACE("br\n"); gjmp_addr(l->jind); }
				else
					{ MCC_TRACE("br\n"); l->jnext = gjmp(l->jnext); }
			} else if (ast_op(a, s) == 2) { MCC_TRACE("br\n");
				if (ast_rp_switch) { MCC_TRACE("br\n");
					struct case_t *cr = mcc_malloc(sizeof(struct case_t));
					int uns = ast_rp_switch->sv.type.t & VT_UNSIGNED;
					cr->v1 = (int64_t)ast_ival(a, s);
					cr->v2 = (int64_t)ast_fbits(a, s);
					cr->v1hi = uns ? 0 : (uint64_t)(cr->v1 >> 63);
					cr->v2hi = uns ? 0 : (uint64_t)(cr->v2 >> 63);
					cr->ind = gind();
					cr->line = 0;
					dynarray_add(&ast_rp_switch->p, &ast_rp_switch->n, cr);
				}
			} else if (ast_op(a, s) == 3) { MCC_TRACE("br\n");
				if (ast_rp_switch)
					{ MCC_TRACE("br\n"); ast_rp_switch->def_sym = gind(); }
			} else if (ast_op(a, s) == 1) { MCC_TRACE("br\n");
				if (ast_rp_csym)
					{ MCC_TRACE("br\n"); *ast_rp_csym = gjmp(*ast_rp_csym); }
			} else { MCC_TRACE("br\n");
				if (ast_rp_bsym)
					{ MCC_TRACE("br\n"); *ast_rp_bsym = gjmp(*ast_rp_bsym); }
			}
			break;
		case AST_If: {
			if (ast_op(a, s) == 7) { MCC_TRACE("br\n");
				int avoid =
						(ast_type_t(a, ast_child(a, s, 1)) & VT_BTYPE) == VT_VOID ||
						(ast_type_t(a, ast_child(a, s, 2)) & VT_BTYPE) == VT_VOID;
				if (avoid) { MCC_TRACE("br\n");
					SValue *base0;
					ast_replay_value(a, ast_child(a, s, 0));
					save_regs(1);
					int tt = gvtst(1, 0);
					base0 = vtop;
					ast_replay_value(a, ast_child(a, s, 1));
					while (vtop > base0)
						{ MCC_TRACE("br\n"); vpop(); }
					int u = gjmp(0);
					gsym(tt);
					base0 = vtop;
					ast_replay_value(a, ast_child(a, s, 2));
					while (vtop > base0)
						{ MCC_TRACE("br\n"); vpop(); }
					int t2 = gjmp(0);
					gsym(u);
					gsym(t2);
					break;
				}
				ast_replay_value(a, s);
				vpop();
				break;
			}
			if (ast_op(a, s) == 6) { MCC_TRACE("br\n");
				struct switch_t *sw = mcc_mallocz(sizeof *sw);
				struct switch_t *prevsw = ast_rp_switch;
				int *sb = ast_rp_bsym;
				int a2 = 0, b2;
				ast_replay_value(a, ast_child(a, s, 0));
				sw->sv = *vtop;
				vtop--;
				b2 = gjmp(0);
				ast_rp_bsym = &a2;
				ast_rp_switch = sw;
				ast_replay_bb(a, ast_child(a, s, 1));
				ast_rp_switch = prevsw;
				ast_rp_bsym = sb;
				a2 = gjmp(a2);
				gsym(b2);
				sw->prev = cur_switch;
				cur_switch = sw;
				case_sort(sw);
				vpushv(&sw->sv);
				gv(MCC_RC_INT);
				int d = gcase(sw->p, sw->n, 0);
				vpop();
				if (sw->def_sym)
					{ MCC_TRACE("br\n"); gsym_addr(d, sw->def_sym); }
				else
					{ MCC_TRACE("br\n"); gsym(d); }
				gsym(a2);
				cur_switch = sw->prev;
				dynarray_reset(&sw->p, &sw->n);
				mcc_free(sw);
				break;
			}
			if (ast_op(a, s) == 2) { MCC_TRACE("br\n");
				int dd = gind();
				if (ast_nchild(a, s) >= 3)
					{ MCC_TRACE("br\n"); ast_replay_bb(a, ast_child(a, s, 2)); }
				ast_replay_value(a, ast_child(a, s, 0));
				int aa = gvtst(1, 0);
				int bb = 0;
				int *sb = ast_rp_bsym, *sc = ast_rp_csym;
				ast_rp_bsym = &aa;
				ast_rp_csym = &bb;
				ast_replay_bb(a, ast_child(a, s, 1));
				ast_rp_bsym = sb;
				ast_rp_csym = sc;
				gjmp_addr(dd);
				gsym_addr(bb, dd);
				gsym(aa);
				break;
			}
			if (ast_op(a, s) == 4) { MCC_TRACE("br\n");
				int dd = gind();
				int aa = 0, bb = 0;
				int *sb = ast_rp_bsym, *sc = ast_rp_csym;
				ast_rp_bsym = &aa;
				ast_rp_csym = &bb;
				ast_replay_bb(a, ast_child(a, s, 0));
				ast_rp_bsym = sb;
				ast_rp_csym = sc;
				gsym(bb);
				if (ast_nchild(a, s) >= 3)
					{ MCC_TRACE("br\n"); ast_replay_bb(a, ast_child(a, s, 2)); }
				ast_replay_value(a, ast_child(a, s, 1));
				int cc = gvtst(0, 0);
				gsym_addr(cc, dd);
				gsym(aa);
				break;
			}
			if (ast_op(a, s) == 8) { MCC_TRACE("br\n");
				int cc = gind();
				int dd = cc;
				AstLocal incrbb = ast_child(a, s, 0);
				if (incrbb != AST_NONE &&
						(ast_first_child(a, incrbb) != AST_NONE ||
						 (ast_fbits(a, s) & AST_FB_FOR_INCR_LIVE))) { MCC_TRACE("br\n");
					int ee = gjmp(0);
					dd = gind();
					ast_replay_bb(a, incrbb);
					gjmp_addr(cc);
					gsym(ee);
				}
				int aa = 0, bb = 0;
				int *sb = ast_rp_bsym, *sc = ast_rp_csym;
				ast_rp_bsym = &aa;
				ast_rp_csym = &bb;
				ast_replay_bb(a, ast_child(a, s, 1));
				ast_rp_bsym = sb;
				ast_rp_csym = sc;
				gjmp_addr(dd);
				gsym_addr(bb, dd);
				gsym(aa);
				break;
			}
			if (ast_op(a, s) == 3) { MCC_TRACE("br\n");
				int cc = gind();
				if (ast_nchild(a, s) >= 4)
					{ MCC_TRACE("br\n"); ast_replay_bb(a, ast_child(a, s, 3)); }
				ast_replay_value(a, ast_child(a, s, 0));
				int aa = gvtst(1, 0);
				int dd = cc;
				AstLocal incrbb = ast_child(a, s, 1);
				if (incrbb != AST_NONE &&
						(ast_first_child(a, incrbb) != AST_NONE ||
						 (ast_fbits(a, s) & AST_FB_FOR_INCR_LIVE))) { MCC_TRACE("br\n");
					int ee = gjmp(0);
					dd = gind();
					ast_replay_bb(a, incrbb);
					gjmp_addr(cc);
					gsym(ee);
				}
				int bb = 0;
				int *sb = ast_rp_bsym, *sc = ast_rp_csym;
				ast_rp_bsym = &aa;
				ast_rp_csym = &bb;
				ast_replay_bb(a, ast_child(a, s, 2));
				ast_rp_bsym = sb;
				ast_rp_csym = sc;
				gjmp_addr(dd);
				gsym_addr(bb, dd);
				gsym(aa);
				break;
			}
			ast_replay_value(a, ast_child(a, s, 0));
			if (ast_in_graft &&
					(vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
				int truthy = vtop->c.i != 0;
				vtop--;
				AstLocal taken = truthy ? ast_child(a, s, 1) : ast_child(a, s, 2);
				if (taken != AST_NONE)
					{ MCC_TRACE("br\n"); ast_replay_bb(a, taken); }
				break;
			}
			int aa = gvtst(1, 0);
			ast_replay_bb(a, ast_child(a, s, 1));
			AstLocal elsebb = ast_child(a, s, 2);
			if (elsebb != AST_NONE) { MCC_TRACE("br\n");
				int dd = gjmp(0);
				gsym(aa);
				ast_replay_bb(a, elsebb);
				gsym(dd);
			} else { MCC_TRACE("br\n");
				gsym(aa);
			}
			break;
		}
		case AST_Return: {
			AstLocal v = ast_first_child(a, s);
			if (ast_in_graft) { MCC_TRACE("br\n");
				if (v != AST_NONE) { MCC_TRACE("br\n");
					ast_replay_value(a, v);
					gen_cast(&ast_graft_rt);
					SValue rs;
					memset(&rs, 0, sizeof rs);
					rs.type = ast_graft_rt;
					rs.r = VT_LOCAL | VT_LVAL;
					rs.r2 = VT_CONST;
					rs.c.i = ast_inline_ret_slot;
					vpushv(&rs);
					vswap();
					vstore();
					vpop();
				}
				if (ast_op(a, s) == 1)
					{ MCC_TRACE("br\n"); ast_inline_ret_sym = gjmp(ast_inline_ret_sym); }
				break;
			}
			int rloc = (int)(int64_t)ast_ival(a, s);
			if (v != AST_NONE) { MCC_TRACE("br\n");
				ast_replay_value(a, v);
				gen_assign_cast(&func_vt);
			}
			if (rloc)
				{ MCC_TRACE("br\n"); gen_vla_sp_restore(rloc); }
			if (v != AST_NONE)
				{ MCC_TRACE("br\n"); gfunc_return(&func_vt); }
			if (ast_op(a, s) == 1)
				{ MCC_TRACE("br\n"); rsym = gjmp(rsym); }
			break;
		}
		default:
			break;
		}
		if (ast_sattr_arena)
			{ MCC_TRACE("br\n"); ast_sattr_note(a, s, ind - ast_sattr_ind0); }
	}
}

static int ast_treechk_env_cached = -1;

static const char *ast_relsym_name(int idx) { MCC_TRACE("enter\n");
	ElfW(Sym) * sy;
	if (!symtab_section || idx <= 0)
		{ MCC_TRACE("br\n"); return "?"; }
	if ((unsigned long)idx * sizeof *sy >= symtab_section->data_offset)
		{ MCC_TRACE("br\n"); return "?"; }
	sy = &((ElfW(Sym) *)symtab_section->data)[idx];
	return (const char *)symtab_section->link->data + sy->st_name;
}

static int ast_treechk_on(void) { MCC_TRACE("enter\n");
	if (ast_treechk_env_cached < 0)
		{ MCC_TRACE("br\n"); ast_treechk_env_cached = mcc_env_on("MCC_AST_TREECHK") ? 1 : 0; }
	return ast_treechk_env_cached;
}

static int ast_treechk(AstArena *a, const char *fname, const char *phase) { MCC_TRACE("enter\n");
	AstLocal n, c;
	AstLocal *seen;
	int bad = 0;
	if (!a || !a->count)
		{ MCC_TRACE("br\n"); return 0; }
	seen = (AstLocal *)mcc_malloc((size_t)a->count * sizeof *seen);
	if (!seen)
		{ MCC_TRACE("br\n"); return 0; }
	for (n = 0; n < a->count; n++)
		{ MCC_TRACE("br\n"); seen[n] = AST_NONE; }
	for (n = 0; n < a->count; n++) { MCC_TRACE("br\n");
		for (c = a->first_child[n]; c != AST_NONE && c < a->count;
				 c = a->next_sib[c]) { MCC_TRACE("br\n");
			if (seen[c] != AST_NONE) { MCC_TRACE("br\n");
				fprintf(stderr,
								"[treechk] %s (%s): node %u(k%u) is in TWO child chains: %u and %u\n",
								fname ? fname : "?", phase, (unsigned)c, (unsigned)a->kind[c],
								(unsigned)seen[c], (unsigned)n);
				bad++;
				break;
			}
			seen[c] = n;
		}
	}
	for (n = 0; n < a->count; n++) { MCC_TRACE("br\n");
		if (seen[n] == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		if (a->parent[n] != seen[n]) { MCC_TRACE("br\n");
			fprintf(stderr,
							"[treechk] %s (%s): node %u(k%u) sits in %u's chain but parent[] says %u\n",
							fname ? fname : "?", phase, (unsigned)n, (unsigned)a->kind[n],
							(unsigned)seen[n], (unsigned)a->parent[n]);
			bad++;
		}
	}
	mcc_free(seen);
	return bad;
}

static int ast_storeval_lval_leaf(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int k, r;
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	k = ast_kind(a, n);
	if (k != AST_Literal && k != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	r = (int)ast_op(a, n);
	if (!(r & VT_LVAL))
		{ MCC_TRACE("br\n"); return 0; }
	r &= VT_VALMASK;
	return r == VT_LOCAL || r == VT_CONST;
}

static int ast_storeval_push_leaf(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_nchild(a, n) != 0)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_kind(a, n) == AST_Ref || ast_kind(a, n) == AST_Literal;
}

static int ast_storeval_const_leaf(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	while (n != AST_NONE && ast_kind(a, n) == AST_Convert && ast_nchild(a, n) == 1)
		{ MCC_TRACE("br\n"); n = ast_child(a, n, 0); }
	return n != AST_NONE && ast_kind(a, n) == AST_Literal &&
				 (ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
}

static void ast_finalize_storevals(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal n;
	for (n = 0; n < a->count; n++) { MCC_TRACE("br\n");
		AstLocal st, par;
		if (ast_kind(a, n) != AST_StoreVal)
			{ MCC_TRACE("br\n"); continue; }
		st = (AstLocal)ast_ival(a, n);
		if (st == AST_NONE || st >= a->count || ast_kind(a, st) != AST_Store)
			{ MCC_TRACE("br\n"); continue; }
		par = ast_parent(a, n);
		if (par == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		{
			AstLocal cur = n, up, call_up = AST_NONE;
			int leftmost = 1, constl = 0, call_store = 0, docond = 0, rot = 0;
			for (;;) {
				MCC_TRACE("br\n");
				up = ast_parent(a, cur);
				if (up == AST_NONE || ast_kind(a, up) == AST_BasicBlock)
					{ MCC_TRACE("br\n"); break; }
				if (ast_kind(a, up) == AST_Invoke) { MCC_TRACE("br\n");
					AstLocal pst = ast_parent(a, up);
					if (ast_storeval_calllast_env && call_up == AST_NONE && !constl &&
							!docond && !rot && ast_nchild(a, up) >= 3 &&
							ast_child(a, up, ast_nchild(a, up) - 1) == cur &&
							pst != AST_NONE && ast_kind(a, pst) == AST_BasicBlock) { MCC_TRACE("br\n");
						rot = 1;
						cur = up;
						continue;
					}
					int pst_store = ast_storeval_callstore_env && pst != AST_NONE &&
													ast_kind(a, pst) == AST_Store &&
													ast_nchild(a, pst) == 2 && ast_child(a, pst, 1) == up &&
													ast_kind(a, ast_parent(a, pst)) == AST_BasicBlock &&
													ast_storeval_lval_leaf(a, ast_child(a, pst, 0));
					if (!ast_storeval_call_env || call_up != AST_NONE ||
							ast_nchild(a, up) < 2 || ast_child(a, up, 1) != cur ||
							(ast_kind(a, pst) != AST_BasicBlock && !pst_store &&
							 !ast_storeval_callup_env))
						{ MCC_TRACE("br\n"); leftmost = 0; break; }
					call_up = up;
					if (pst_store) { MCC_TRACE("br\n");
						call_store = 1;
						cur = pst;
					} else { MCC_TRACE("br\n");
						cur = up;
					}
					continue;
				}
				if (ast_first_child(a, up) != cur) { MCC_TRACE("br\n");
					if (ast_storeval_constl_env && !constl && call_up == AST_NONE &&
							ast_kind(a, up) == AST_Binary && ast_nchild(a, up) == 2 &&
							ast_child(a, up, 1) == cur &&
							ast_storeval_const_leaf(a, ast_child(a, up, 0))) { MCC_TRACE("br\n");
						constl = 1;
						cur = up;
						continue;
					}
					if (ast_loopcond_store_env && ast_while_comma_env && call_up == AST_NONE && !constl &&
							ast_kind(a, up) == AST_If && ast_op(a, up) == 4 &&
							ast_nchild(a, up) >= 2 && ast_child(a, up, 1) == cur) { MCC_TRACE("br\n");
						docond = 1;
						cur = up;
						continue;
					}
					if (ast_storeval_rot_env && call_up == AST_NONE && !constl &&
							!docond && ast_nchild(a, up) == 2 &&
							ast_child(a, up, 1) == cur &&
							(ast_kind(a, up) == AST_Binary || ast_kind(a, up) == AST_Store) &&
							ast_storeval_push_leaf(a, ast_child(a, up, 0))) { MCC_TRACE("br\n");
						rot = 1;
						cur = up;
						continue;
					}
					leftmost = 0;
					break;
				}
				cur = up;
			}
			if (!leftmost || up == AST_NONE || ast_kind(a, up) != AST_BasicBlock)
				{ MCC_TRACE("br\n"); continue; }
			if (ast_next_sib(a, st) != cur || up != ast_parent(a, st) || docond) { MCC_TRACE("br\n");
				int pfx_ok = 0;
				AstLocal pbb = ast_parent(a, st);
				if (ast_loopcond_store_env && ast_while_comma_env && call_up == AST_NONE && !constl &&
						pbb != AST_NONE && ast_kind(a, pbb) == AST_BasicBlock &&
						cur != AST_NONE && ast_kind(a, cur) == AST_If &&
						ast_parent(a, pbb) == cur &&
						(ast_op(a, cur) == 2 || ast_op(a, cur) == 3 ||
						 (ast_op(a, cur) == 4 && docond)) &&
						ast_next_sib(a, st) == AST_NONE) { MCC_TRACE("br\n");
					uint32_t nc = ast_nchild(a, cur);
					if (nc >= 3 && ast_child(a, cur, nc - 1) == pbb)
						{ MCC_TRACE("br\n"); pfx_ok = 1; }
				}
				if (!pfx_ok)
					{ MCC_TRACE("br\n"); continue; }
			}
			{
				AstLocal it = n;
				int reload_ok = 1;
				while (it != AST_NONE && it != up) { MCC_TRACE("br\n");
					AstLocal pu = ast_parent(a, it);
					if (pu != AST_NONE && ast_kind(a, pu) == AST_Store &&
							ast_nchild(a, pu) == 2 && ast_child(a, pu, 1) == it &&
							!ast_storeval_reload_ok(a, ast_child(a, pu, 0), 16))
						{ MCC_TRACE("br\n"); reload_ok = 0; break; }
					it = pu;
				}
				if (!reload_ok)
					{ MCC_TRACE("br\n"); continue; }
			}
			if (call_up != AST_NONE)
				{ MCC_TRACE("br\n"); ast_set_fbits(a, call_up,
						ast_fbits(a, call_up) | AST_FB_CALL_STOREVAL_ARG |
						(call_store ? AST_FB_CALL_STOREVAL_STORE : 0u)); }
			if (rot)
				{ MCC_TRACE("br\n"); ast_set_fbits(a, st,
						ast_fbits(a, st) | AST_FB_STORE_LIVE_ROT); }
			if (constl)
				{ MCC_TRACE("br\n"); ast_set_fbits(a, st,
						ast_fbits(a, st) | AST_FB_STOREVAL_CONST_LEFT); }
		}
		ast_set_fbits(a, st, ast_fbits(a, st) | AST_FB_STORE_VALUE_LIVE);
	}
}

static void ast_finalize_chainstores(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal n;
	if (!ast_chainstore_live_env && !ast_chainstore_member_env)
		{ MCC_TRACE("br\n"); return; }
	for (n = 0; n < a->count; n++) { MCC_TRACE("br\n");
		AstLocal nx;
		if (ast_kind(a, n) != AST_Store || ast_nchild(a, n) != 2)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_fbits(a, n) & AST_FB_STORE_CHAIN_SKIP)
			{ MCC_TRACE("br\n"); continue; }
		nx = ast_next_sib(a, n);
		if (nx == AST_NONE || ast_kind(a, nx) != AST_Store ||
				!(ast_fbits(a, nx) & 1u) || ast_nchild(a, nx) != 2)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_op(a, nx) == AST_OP_OPASSIGN)
			{ MCC_TRACE("br\n"); continue; }
		{
			AstLocal ov = ast_child(a, nx, 1);
			int ovreg = ov != AST_NONE &&
									(ast_kind(a, ov) == AST_Literal || ast_kind(a, ov) == AST_Ref) &&
									ast_nchild(a, ov) == 0 &&
									((int)ast_op(a, ov) & VT_VALMASK) < VT_CONST &&
									!((int)ast_op(a, ov) & (VT_LVAL | VT_SYM));
			if (!ovreg && !ast_struct_eq(a, ast_child(a, n, 1), ov, 16))
				{ MCC_TRACE("br\n"); continue; }
		}
		if (ast_chainstore_live_env &&
				!(ast_fbits(a, nx) & (AST_FB_STORE_CHAIN_MEMBER |
															AST_FB_STORE_CHAIN_SKIP)) &&
				ast_storeval_lval_leaf(a, ast_child(a, nx, 0))) { MCC_TRACE("br\n");
			ast_set_fbits(a, n, ast_fbits(a, n) | AST_FB_STORE_VALUE_LIVE |
														 AST_FB_STORE_CHAIN_LIVE);
			ast_set_fbits(a, nx, ast_fbits(a, nx) | AST_FB_STORE_CHAIN_REUSE);
			continue;
		}
		if (!ast_chainstore_member_env)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_fbits(a, n) & AST_FB_STORE_CHAIN_REUSE)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_fbits(a, nx) & AST_FB_STORE_VALUE_LIVE)
			{ MCC_TRACE("br\n"); continue; }
		{
			AstLocal mk, consumed = AST_NONE;
			for (mk = 0; mk < a->count; mk++)
				{ MCC_TRACE("br\n"); if (ast_kind(a, mk) == AST_StoreVal &&
						(AstLocal)ast_ival(a, mk) == nx &&
						ast_parent(a, mk) != AST_NONE) { MCC_TRACE("br\n");
					consumed = mk;
					break;
				} }
			if (consumed != AST_NONE)
				{ MCC_TRACE("br\n"); continue; }
		}
		{
			AstLocal after = ast_next_sib(a, nx);
			if (after != AST_NONE && ast_kind(a, after) == AST_Store &&
					ast_nchild(a, after) == 2) { MCC_TRACE("br\n");
				AstLocal av = ast_child(a, after, 1);
				if (av != AST_NONE &&
						(ast_kind(a, av) == AST_Literal || ast_kind(a, av) == AST_Ref) &&
						ast_nchild(a, av) == 0 &&
						((int)ast_op(a, av) & VT_VALMASK) < VT_CONST &&
						!((int)ast_op(a, av) & (VT_LVAL | VT_SYM)))
					{ MCC_TRACE("br\n"); continue; }
			}
		}
		if (!ast_expr_pure(a, ast_child(a, nx, 0), 16) ||
				!ast_expr_pure(a, ast_child(a, n, 0), 16))
			{ MCC_TRACE("br\n"); continue; }
#if (defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64))
		if (ast_promo_n &&
				(ast_promo_reg_of(a, ast_child(a, n, 0)) >= 0 ||
				 ast_promo_reg_of(a, ast_child(a, nx, 0)) >= 0))
			{ MCC_TRACE("br\n"); continue; }
#endif
		ast_set_fbits(a, n, ast_fbits(a, n) | AST_FB_STORE_CHAIN_MEMBER);
		ast_set_fbits(a, nx, ast_fbits(a, nx) | AST_FB_STORE_CHAIN_SKIP);
	}
}

static AstLocal ast_prev_sib(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal p = ast_parent(a, n), c, prev = AST_NONE;
	if (p == AST_NONE)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	for (c = ast_first_child(a, p); c != AST_NONE && c != n; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); prev = c; }
	return prev;
}

static int ast_chain_partner_ok(AstArena *a, AstLocal n, uint64_t want) { MCC_TRACE("enter\n");
	return n != AST_NONE && ast_kind(a, n) == AST_Store &&
				 ast_nchild(a, n) == 2 && (ast_fbits(a, n) & want) != 0;
}

static void ast_revoke_chainstores(AstArena *a) { MCC_TRACE("enter\n");
	int changed;
	do { MCC_TRACE("br\n");
		AstLocal n;
		changed = 0;
		for (n = 0; n < a->count; n++) { MCC_TRACE("br\n");
			uint64_t fb, drop = 0;
			if (ast_kind(a, n) != AST_Store || ast_nchild(a, n) != 2)
				{ MCC_TRACE("br\n"); continue; }
			fb = ast_fbits(a, n);
			if (!(fb & (AST_FB_STORE_CHAIN_LIVE | AST_FB_STORE_CHAIN_REUSE |
									AST_FB_STORE_CHAIN_MEMBER | AST_FB_STORE_CHAIN_SKIP)))
				{ MCC_TRACE("br\n"); continue; }
			if ((fb & AST_FB_STORE_CHAIN_LIVE) &&
					!ast_chain_partner_ok(a, ast_next_sib(a, n), AST_FB_STORE_CHAIN_REUSE))
				{ MCC_TRACE("br\n"); drop |= AST_FB_STORE_CHAIN_LIVE | AST_FB_STORE_VALUE_LIVE; }
			if ((fb & AST_FB_STORE_CHAIN_REUSE) &&
					!ast_chain_partner_ok(a, ast_prev_sib(a, n), AST_FB_STORE_CHAIN_LIVE))
				{ MCC_TRACE("br\n"); drop |= AST_FB_STORE_CHAIN_REUSE; }
			if ((fb & AST_FB_STORE_CHAIN_MEMBER) &&
					!ast_chain_partner_ok(a, ast_next_sib(a, n), AST_FB_STORE_CHAIN_SKIP))
				{ MCC_TRACE("br\n"); drop |= AST_FB_STORE_CHAIN_MEMBER; }
			if ((fb & AST_FB_STORE_CHAIN_SKIP) &&
					!ast_chain_partner_ok(a, ast_prev_sib(a, n), AST_FB_STORE_CHAIN_MEMBER))
				{ MCC_TRACE("br\n"); drop |= AST_FB_STORE_CHAIN_SKIP; }
			if (drop) { MCC_TRACE("br\n");
				ast_set_fbits(a, n, fb & ~drop);
				changed = 1;
			}
		}
	} while (changed);
}

static void ast_replay_body(AstArena *a) { MCC_TRACE("enter\n");
	if (ast_fpert_on && !ast_in_graft) { MCC_TRACE("br\n");
		AstLocal nn = ast_count(a);
		int lo = 0;
		ast_fpert_n = 0;
		ast_fpert_floor =
				(rir_body_loc_sv <= 0 && rir_body_loc_sv >= loc) ? rir_body_loc_sv : loc;
		ast_fpert_live = 1;
		for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
			int r;
			if (ast_kind(a, n) != AST_Ref && ast_kind(a, n) != AST_Literal)
				{ MCC_TRACE("br\n"); continue; }
			r = ast_op(a, n);
			if ((r & VT_SYM) ||
					((r & VT_VALMASK) != VT_LOCAL && (r & VT_VALMASK) != VT_LLOCAL))
				{ MCC_TRACE("br\n"); continue; }
			{
				int o = ast_fpert_map((int)(int64_t)ast_ival(a, n));
				if (o < lo)
					{ MCC_TRACE("br\n"); lo = o; }
			}
		}
		loc = ast_fpert_floor + ast_fpert_base;
		if (lo < loc)
			{ MCC_TRACE("br\n"); loc = lo; }
		loc &= -16;
		if (ast_fpert_dbg) { MCC_TRACE("br\n");
			int nm = 0;
			for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
				int r;
				if (ast_kind(a, n) != AST_Ref && ast_kind(a, n) != AST_Literal)
					{ MCC_TRACE("br\n"); continue; }
				r = ast_op(a, n);
				if ((r & VT_SYM) ||
						((r & VT_VALMASK) != VT_LOCAL && (r & VT_VALMASK) != VT_LLOCAL))
					{ MCC_TRACE("br\n"); continue; }
				if ((int)(int64_t)ast_ival(a, n) < ast_fpert_floor)
					{ MCC_TRACE("br\n"); nm++; }
			}
			fprintf(stderr, "[fpert] %s floor=%d bodysv=%d loc=%d lo=%d remapped=%d\n",
							funcname ? funcname : "?", ast_fpert_floor, rir_body_loc_sv, loc, lo,
							nm);
		}
		if (loc < ast_loc_low)
			{ MCC_TRACE("br\n"); ast_loc_low = loc; }
		ast_graft_base = loc;
	}
	ast_sv_live_depth = 0;
	ast_revoke_chainstores(a);
	ast_finalize_storevals(a);
	ast_finalize_chainstores(a);
	ast_replay_bb(a, ast_root(a));
}

static uint64_t ast_fold_eval(int op, int tt, uint64_t l1, uint64_t l2,
															int *ok) { MCC_TRACE("enter\n");
	int shm = ((tt & VT_BTYPE) == VT_LLONG) ? 63 : 31;
	switch (op) { MCC_TRACE("br\n");
	case '+':
		return l1 + l2;
	case '-':
		return l1 - l2;
	case '&':
		return l1 & l2;
	case '^':
		return l1 ^ l2;
	case '|':
		return l1 | l2;
	case '*':
		return l1 * l2;
	case '/':
		if (l2 == 0) { MCC_TRACE("br\n");
			*ok = 0;
			return 0;
		}
		return (tt & VT_UNSIGNED) ? l1 / l2 : gen_opic_sdiv(l1, l2);
	case '%':
		if (l2 == 0) { MCC_TRACE("br\n");
			*ok = 0;
			return 0;
		}
		return (tt & VT_UNSIGNED) ? l1 % l2 : l1 - l2 * gen_opic_sdiv(l1, l2);
	case TOK_SHL:
		return l1 << (l2 & shm);
	case TOK_SHR:
		return l1 >> (l2 & shm);
	case TOK_SAR:
		return (l1 >> 63) ? ~(~l1 >> (l2 & shm)) : l1 >> (l2 & shm);
	default:
		*ok = 0;
		return 0;
	}
}

static int ast_fold_op_ok(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case TOK_SHL:
	case TOK_SHR:
	case TOK_SAR:
		return 1;
	default:
		return 0;
	}
}

static void ast_fold_rec(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_fold_rec(a, c); }
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return; }
	int op = ast_op(a, n);
	if (!ast_fold_op_ok(op))
		{ MCC_TRACE("br\n"); return; }
	AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
	if (ast_kind(a, x) != AST_Literal || ast_kind(a, y) != AST_Literal)
		{ MCC_TRACE("br\n"); return; }
	int tt = ast_type_t(a, x);
	{
		int ty2 = ast_type_t(a, y);
		int bx = tt & VT_BTYPE, by = ty2 & VT_BTYPE;
		if (op != TOK_SHL && op != TOK_SHR && op != TOK_SAR &&
				!ast_bad_type(tt) && !ast_bad_type(ty2) && !is_float(tt) &&
				!is_float(ty2)) { MCC_TRACE("br\n");
			int wx = bx == VT_LLONG ? 8 : 4, wy = by == VT_LLONG ? 8 : 4;
			if (wy > wx)
				tt = ty2;
			else if (wy == wx && (ty2 & VT_UNSIGNED))
				tt |= VT_UNSIGNED;
		}
	}
	if (ast_bad_type(tt) || ast_bad_type(ast_type_t(a, y)))
		{ MCC_TRACE("br\n"); return; }
	if (is_float(tt) || is_float(ast_type_t(a, y)))
		{ MCC_TRACE("br\n"); return; }
	uint64_t l1 = value64(ast_ival(a, x), tt);
	uint64_t l2 = value64(ast_ival(a, y), ast_type_t(a, y));
	int ok = 1;
	uint64_t r = ast_fold_eval(op, tt, l1, l2, &ok);
	if (!ok)
		{ MCC_TRACE("br\n"); return; }
	ast_set_kind(a, n, AST_Literal);
	ast_clear_children(a, n);
	ast_set_op(a, n, ast_op(a, x) | (ast_op(a, y) & VT_NONCONST));
	ast_set_type(a, n, tt, ast_type_ref(a, x));
	ast_set_ival(a, n, value64(r, tt));
	ast_set_sym(a, n, 0);
	ast_tmpl_folds++;
}

/* T-lin-10510 (win-x64): recognize the byte-swap idiom -- N OR'd terms each of
 * the form (base SHIFT 8*k) & (0xff << 8*d), over one non-volatile scalar Ref
 * `base', that together reverse all N bytes of base -- and fold it to a single
 * AST_OP_BSWAP so gen_bswap emits a native bswap/rev (gcc/clang parity). Value-
 * exact; default-OFF (-fbswap-idiom). Slice 1: 16/32/64-bit, base a plain Ref.
 * Reassociation-agnostic: the OR tree is flattened and the byte permutation is
 * checked as a set, so term order does not matter. */
static int ast_bswap_flatten(AstArena *a, AstLocal n, AstLocal *out, int max) { MCC_TRACE("enter\n");
	if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == '|' &&
			ast_nchild(a, n) == 2) { MCC_TRACE("br\n");
		int c0 = ast_bswap_flatten(a, ast_child(a, n, 0), out, max);
		if (c0 < 0)
			{ MCC_TRACE("br\n"); return -1; }
		int c1 = ast_bswap_flatten(a, ast_child(a, n, 1), out + c0, max - c0);
		if (c1 < 0)
			{ MCC_TRACE("br\n"); return -1; }
		return c0 + c1;
	}
	if (max < 1)
		{ MCC_TRACE("br\n"); return -1; }
	out[0] = n;
	return 1;
}

static int ast_bswap_same_base(AstArena *a, AstLocal x, AstLocal y) { MCC_TRACE("enter\n");
	return ast_kind(a, x) == AST_Ref && ast_kind(a, y) == AST_Ref &&
				 ast_op(a, x) == ast_op(a, y) && ast_sym(a, x) == ast_sym(a, y) &&
				 ast_type_t(a, x) == ast_type_t(a, y) &&
				 ast_type_ref(a, x) == ast_type_ref(a, y) &&
				 ast_ival(a, x) == ast_ival(a, y);
}

static int ast_bswap_try(AstArena *a, AstLocal top) { MCC_TRACE("enter\n");
	AstLocal terms[8], base = AST_NONE;
	int nt, i, nbytes = 0, btype = 0, srcseen = 0;
	if (ast_kind(a, top) != AST_Binary || ast_op(a, top) != '|')
		{ MCC_TRACE("br\n"); return 0; }
	nt = ast_bswap_flatten(a, top, terms, 8);
	if (nt < 2)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < nt; i++) { MCC_TRACE("br\n");
		AstLocal t = terms[i], sh, bnode, mlit, slit, tmp;
		int so, k, d, dd, srcb;
		uint64_t mask, shamt, wmask;
		if (ast_kind(a, t) != AST_Binary || ast_op(a, t) != '&' ||
				ast_nchild(a, t) != 2)
			{ MCC_TRACE("br\n"); return 0; }
		sh = ast_child(a, t, 0);
		mlit = ast_child(a, t, 1);
		if (ast_kind(a, mlit) != AST_Literal) { MCC_TRACE("br\n");
			tmp = sh;
			sh = mlit;
			mlit = tmp;
		}
		if (ast_kind(a, mlit) != AST_Literal)
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_kind(a, sh) != AST_Binary || ast_nchild(a, sh) != 2)
			{ MCC_TRACE("br\n"); return 0; }
		so = ast_op(a, sh);
		if (so != TOK_SHL && so != TOK_SHR && so != TOK_SAR)
			{ MCC_TRACE("br\n"); return 0; }
		bnode = ast_child(a, sh, 0);
		slit = ast_child(a, sh, 1);
		if (ast_kind(a, slit) != AST_Literal)
			{ MCC_TRACE("br\n"); return 0; }
		if (base == AST_NONE) { MCC_TRACE("br\n");
			if (ast_kind(a, bnode) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			btype = ast_type_t(a, bnode);
			if (btype & VT_VOLATILE)
				{ MCC_TRACE("br\n"); return 0; }
			switch (btype & VT_BTYPE) { MCC_TRACE("br\n");
			case VT_SHORT:
				nbytes = 2;
				break;
			case VT_INT:
				nbytes = 4;
				break;
			case VT_LLONG:
				nbytes = 8;
				break;
			default:
				return 0;
			}
			if (nt != nbytes)
				{ MCC_TRACE("br\n"); return 0; }
			base = bnode;
		} else if (!ast_bswap_same_base(a, bnode, base)) { MCC_TRACE("br\n");
			return 0;
		}
		shamt = ast_ival(a, slit);
		if ((shamt & 7) || (shamt >> 3) >= (uint64_t)nbytes)
			{ MCC_TRACE("br\n"); return 0; }
		k = (int)(shamt >> 3);
		wmask = (nbytes == 8) ? ~(uint64_t)0
													: (((uint64_t)1 << (8 * nbytes)) - 1);
		mask = ast_ival(a, mlit) & wmask;
		d = -1;
		for (dd = 0; dd < nbytes; dd++)
			if (mask == ((uint64_t)0xff << (8 * dd)))
				{ MCC_TRACE("br\n"); d = dd; break; }
		if (d < 0)
			{ MCC_TRACE("br\n"); return 0; }
		srcb = (so == TOK_SHL) ? d - k : d + k;
		if (srcb < 0 || srcb >= nbytes)
			{ MCC_TRACE("br\n"); return 0; }
		if (d != nbytes - 1 - srcb)
			{ MCC_TRACE("br\n"); return 0; }
		if (srcseen & (1 << srcb))
			{ MCC_TRACE("br\n"); return 0; }
		srcseen |= 1 << srcb;
	}
	if (srcseen != ((1 << nbytes) - 1))
		{ MCC_TRACE("br\n"); return 0; }
	{
		AstLocal nb = ast_node(a, AST_Ref);
		ast_copy_type(a, nb, a, base);
		ast_set_op(a, nb, ast_op(a, base));
		ast_set_sym(a, nb, ast_sym(a, base));
		ast_set_ival(a, nb, ast_ival(a, base));
		ast_set_fbits(a, nb, ast_fbits(a, base));
		ast_clear_children(a, top);
		ast_set_kind(a, top, AST_Unary);
		ast_set_op(a, top, AST_OP_BSWAP);
		ast_set_ival(a, top, (uint64_t)nbytes);
		ast_set_type(a, top, btype, ast_type_ref(a, base));
		ast_set_sym(a, top, 0);
		ast_add_child(a, top, nb);
	}
	return 1;
}

#if defined(MCC_TARGET_X86_64)
/* T-lin-10510 (win-x64): recognize the constant-count rotate idiom
 * `(x << C) | (x >> (W-C))` (either term order) over one non-volatile UNSIGNED
 * scalar Ref of width W bits, and fold it to AST_OP_ROTL (native `rol`). gcc/clang
 * both emit `roll`. Value-exact; default-OFF (-frotate-idiom). Slice 1: constant
 * count, x86_64 only (rol reg,imm); variable-count + arm64/i386 are follow-ups.
 * Requires the right shift be LOGICAL (TOK_SHR) + base unsigned, so the fold is a
 * true rotate (a signed SAR would sign-extend, not rotate). */
static int ast_rotate_try(AstArena *a, AstLocal top) { MCC_TRACE("enter\n");
	AstLocal terms[8], base = AST_NONE, shlterm = AST_NONE, shrterm = AST_NONE;
	int nt, i, wbits = 0, sbytes = 0, btype = 0;
	uint64_t shl_c = 0, shr_c = 0;
	if (ast_kind(a, top) != AST_Binary || ast_op(a, top) != '|')
		{ MCC_TRACE("br\n"); return 0; }
	nt = ast_bswap_flatten(a, top, terms, 8);
	if (nt != 2)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < 2; i++) { MCC_TRACE("br\n");
		AstLocal t = terms[i], bnode, cnode;
		int so;
		if (ast_kind(a, t) != AST_Binary || ast_nchild(a, t) != 2)
			{ MCC_TRACE("br\n"); return 0; }
		so = ast_op(a, t);
		if (so != TOK_SHL && so != TOK_SHR && so != TOK_SAR)
			{ MCC_TRACE("br\n"); return 0; }
		bnode = ast_child(a, t, 0);
		cnode = ast_child(a, t, 1);
		if (ast_kind(a, cnode) != AST_Literal)
			{ MCC_TRACE("br\n"); return 0; }
		if (base == AST_NONE) { MCC_TRACE("br\n");
			if (ast_kind(a, bnode) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			btype = ast_type_t(a, bnode);
			if ((btype & VT_VOLATILE) || !(btype & VT_UNSIGNED))
				{ MCC_TRACE("br\n"); return 0; }
			switch (btype & VT_BTYPE) { MCC_TRACE("br\n");
			case VT_SHORT:
				wbits = 16;
				sbytes = 2;
				break;
			case VT_INT:
				wbits = 32;
				sbytes = 4;
				break;
			case VT_LLONG:
				wbits = 64;
				sbytes = 8;
				break;
			default:
				return 0;
			}
			base = bnode;
		} else if (!ast_bswap_same_base(a, bnode, base)) { MCC_TRACE("br\n");
			return 0;
		}
		if (so == TOK_SHL) { MCC_TRACE("br\n");
			if (shlterm != AST_NONE)
				{ MCC_TRACE("br\n"); return 0; }
			shlterm = t;
			shl_c = ast_ival(a, cnode);
		} else { MCC_TRACE("br\n");
			if (shrterm != AST_NONE)
				{ MCC_TRACE("br\n"); return 0; }
			shrterm = t;
			shr_c = ast_ival(a, cnode);
		}
	}
	if (shlterm == AST_NONE || shrterm == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (shl_c == 0 || shl_c >= (uint64_t)wbits || shr_c == 0 ||
			shr_c >= (uint64_t)wbits)
		{ MCC_TRACE("br\n"); return 0; }
	if (shl_c + shr_c != (uint64_t)wbits)
		{ MCC_TRACE("br\n"); return 0; }
	/* rotate-LEFT by shl_c: rewrite top OR to AST_Unary/AST_OP_ROTL over base */
	{
		AstLocal nb = ast_node(a, AST_Ref);
		ast_copy_type(a, nb, a, base);
		ast_set_op(a, nb, ast_op(a, base));
		ast_set_sym(a, nb, ast_sym(a, base));
		ast_set_ival(a, nb, ast_ival(a, base));
		ast_set_fbits(a, nb, ast_fbits(a, base));
		ast_clear_children(a, top);
		ast_set_kind(a, top, AST_Unary);
		ast_set_op(a, top, AST_OP_ROTL);
		ast_set_ival(a, top, ((uint64_t)sbytes << 8) | (shl_c & 0xff));
		ast_set_type(a, top, btype, ast_type_ref(a, base));
		ast_set_sym(a, top, 0);
		ast_add_child(a, top, nb);
	}
	return 1;
}

#endif
#if defined(MCC_TARGET_X86_64)
/* T-lin-10510 (win-x64): variable-count rotate `(x<<n)|(x>>(W-n))` where n is a
 * side-effect-free (Ref) variable and the right count is `W-n` -> rotate-LEFT by
 * n. Emitted as a 2-child AST_OP_ROTL (child0=x, child1=n) -> gen_rotl_var (rol
 * reg,cl). Unsigned base (logical shr). x86_64 only. Slice: the (x<<n)|(x>>(W-n))
 * shape; the mirror (x>>n)|(x<<(W-n)) is a follow-up. */
static int ast_rotate_var_try(AstArena *a, AstLocal top) { MCC_TRACE("enter\n");
	AstLocal terms[8], base = AST_NONE, ncnt = AST_NONE;
	AstLocal shlterm = AST_NONE, shrterm = AST_NONE;
	int nt, i, wbits = 0, sbytes = 0, btype = 0;
	if (ast_kind(a, top) != AST_Binary || ast_op(a, top) != '|')
		{ MCC_TRACE("br\n"); return 0; }
	nt = ast_bswap_flatten(a, top, terms, 8);
	if (nt != 2)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < 2; i++) { MCC_TRACE("br\n");
		AstLocal t = terms[i], bnode, cnode;
		int so;
		if (ast_kind(a, t) != AST_Binary || ast_nchild(a, t) != 2)
			{ MCC_TRACE("br\n"); return 0; }
		so = ast_op(a, t);
		bnode = ast_child(a, t, 0);
		cnode = ast_child(a, t, 1);
		if (base == AST_NONE) { MCC_TRACE("br\n");
			if (ast_kind(a, bnode) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			btype = ast_type_t(a, bnode);
			if ((btype & VT_VOLATILE) || !(btype & VT_UNSIGNED))
				{ MCC_TRACE("br\n"); return 0; }
			switch (btype & VT_BTYPE) { MCC_TRACE("br\n");
			case VT_SHORT: wbits = 16; sbytes = 2; break;
			case VT_INT: wbits = 32; sbytes = 4; break;
			case VT_LLONG: wbits = 64; sbytes = 8; break;
			default: return 0;
			}
			base = bnode;
		} else if (!ast_bswap_same_base(a, bnode, base)) { MCC_TRACE("br\n");
			return 0;
		}
		if (so == TOK_SHL) { MCC_TRACE("br\n");
			/* left term: count is a plain Ref `n' */
			if (shlterm != AST_NONE || ast_kind(a, cnode) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			shlterm = t;
			ncnt = cnode;
		} else if (so == TOK_SHR || so == TOK_SAR) { MCC_TRACE("br\n");
			/* right term: count is `W - n' = Binary('-', Literal(W), n) */
			AstLocal wlit, nn;
			if (shrterm != AST_NONE || ast_kind(a, cnode) != AST_Binary ||
					ast_op(a, cnode) != '-' || ast_nchild(a, cnode) != 2)
				{ MCC_TRACE("br\n"); return 0; }
			wlit = ast_child(a, cnode, 0);
			nn = ast_child(a, cnode, 1);
			if (ast_kind(a, wlit) != AST_Literal || ast_kind(a, nn) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			if ((int)ast_ival(a, wlit) != wbits)
				{ MCC_TRACE("br\n"); return 0; }
			shrterm = t;
			/* nn must be the SAME Ref as the shl count -- record, cross-check below */
			if (ncnt != AST_NONE && !ast_bswap_same_base(a, nn, ncnt))
				{ MCC_TRACE("br\n"); return 0; }
			if (ncnt == AST_NONE)
				{ MCC_TRACE("br\n"); ncnt = nn; }
		} else {
			MCC_TRACE("br\n");
			return 0;
		}
	}
	if (shlterm == AST_NONE || shrterm == AST_NONE || ncnt == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	/* recheck: the shl count and the (W-n) `n' are the same variable */
	{
		AstLocal shl_n = ast_child(a, shlterm, 1);
		AstLocal sub = ast_child(a, shrterm, 1);
		AstLocal sub_n = ast_child(a, sub, 1);
		if (!ast_bswap_same_base(a, shl_n, sub_n))
			{ MCC_TRACE("br\n"); return 0; }
	}
	/* rewrite: AST_Unary/AST_OP_ROTL with 2 children (fresh copies of x and n) */
	{
		AstLocal shl_n = ast_child(a, shlterm, 1);
		AstLocal nb = ast_node(a, AST_Ref);
		AstLocal nc = ast_node(a, AST_Ref);
		ast_copy_type(a, nb, a, base);
		ast_set_op(a, nb, ast_op(a, base));
		ast_set_sym(a, nb, ast_sym(a, base));
		ast_set_ival(a, nb, ast_ival(a, base));
		ast_set_fbits(a, nb, ast_fbits(a, base));
		ast_copy_type(a, nc, a, shl_n);
		ast_set_op(a, nc, ast_op(a, shl_n));
		ast_set_sym(a, nc, ast_sym(a, shl_n));
		ast_set_ival(a, nc, ast_ival(a, shl_n));
		ast_set_fbits(a, nc, ast_fbits(a, shl_n));
		ast_clear_children(a, top);
		ast_set_kind(a, top, AST_Unary);
		ast_set_op(a, top, AST_OP_ROTL);
		ast_set_ival(a, top, (uint64_t)sbytes << 8);
		ast_set_type(a, top, btype, ast_type_ref(a, base));
		ast_set_sym(a, top, 0);
		ast_add_child(a, top, nb);
		ast_add_child(a, top, nc);
	}
	return 1;
}

#endif
#if defined(MCC_TARGET_X86_64)
/* T-lin-10510 (win-x64): variable rotate-RIGHT mirror `(x>>n)|(x<<(W-n))` (n a
 * side-effect-free Ref, right-side count = W-n) -> rotate-right by n. Emitted as a
 * 2-child AST_OP_ROTR (child0=x, child1=n) -> gen_rotr_var (`ror reg,cl`). Same
 * guards as the rotate-left var slice (unsigned base ⇒ logical shr). x86_64. */
static int ast_rotate_varr_try(AstArena *a, AstLocal top) { MCC_TRACE("enter\n");
	AstLocal terms[8], base = AST_NONE, ncnt = AST_NONE;
	AstLocal shlterm = AST_NONE, shrterm = AST_NONE;
	int nt, i, wbits = 0, sbytes = 0, btype = 0;
	if (ast_kind(a, top) != AST_Binary || ast_op(a, top) != '|')
		{ MCC_TRACE("br\n"); return 0; }
	nt = ast_bswap_flatten(a, top, terms, 8);
	if (nt != 2)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < 2; i++) { MCC_TRACE("br\n");
		AstLocal t = terms[i], bnode, cnode;
		int so;
		if (ast_kind(a, t) != AST_Binary || ast_nchild(a, t) != 2)
			{ MCC_TRACE("br\n"); return 0; }
		so = ast_op(a, t);
		bnode = ast_child(a, t, 0);
		cnode = ast_child(a, t, 1);
		if (base == AST_NONE) { MCC_TRACE("br\n");
			if (ast_kind(a, bnode) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			btype = ast_type_t(a, bnode);
			if ((btype & VT_VOLATILE) || !(btype & VT_UNSIGNED))
				{ MCC_TRACE("br\n"); return 0; }
			switch (btype & VT_BTYPE) { MCC_TRACE("br\n");
			case VT_SHORT: wbits = 16; sbytes = 2; break;
			case VT_INT: wbits = 32; sbytes = 4; break;
			case VT_LLONG: wbits = 64; sbytes = 8; break;
			default: return 0;
			}
			base = bnode;
		} else if (!ast_bswap_same_base(a, bnode, base)) { MCC_TRACE("br\n");
			return 0;
		}
		if (so == TOK_SHR || so == TOK_SAR) { MCC_TRACE("br\n");
			/* right term: count is a plain Ref `n' -- the rotate-right amount */
			if (shrterm != AST_NONE || ast_kind(a, cnode) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			shrterm = t;
			ncnt = cnode;
		} else if (so == TOK_SHL) { MCC_TRACE("br\n");
			/* left term: count is `W - n' */
			AstLocal wlit, nn;
			if (shlterm != AST_NONE || ast_kind(a, cnode) != AST_Binary ||
					ast_op(a, cnode) != '-' || ast_nchild(a, cnode) != 2)
				{ MCC_TRACE("br\n"); return 0; }
			wlit = ast_child(a, cnode, 0);
			nn = ast_child(a, cnode, 1);
			if (ast_kind(a, wlit) != AST_Literal || ast_kind(a, nn) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			if ((int)ast_ival(a, wlit) != wbits)
				{ MCC_TRACE("br\n"); return 0; }
			shlterm = t;
		} else {
			MCC_TRACE("br\n");
			return 0;
		}
	}
	if (shlterm == AST_NONE || shrterm == AST_NONE || ncnt == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	{
		AstLocal shr_n = ast_child(a, shrterm, 1);
		AstLocal sub = ast_child(a, shlterm, 1);
		AstLocal sub_n = ast_child(a, sub, 1);
		if (!ast_bswap_same_base(a, shr_n, sub_n))
			{ MCC_TRACE("br\n"); return 0; }
	}
	{
		AstLocal shr_n = ast_child(a, shrterm, 1);
		AstLocal nb = ast_node(a, AST_Ref);
		AstLocal nc = ast_node(a, AST_Ref);
		ast_copy_type(a, nb, a, base);
		ast_set_op(a, nb, ast_op(a, base));
		ast_set_sym(a, nb, ast_sym(a, base));
		ast_set_ival(a, nb, ast_ival(a, base));
		ast_set_fbits(a, nb, ast_fbits(a, base));
		ast_copy_type(a, nc, a, shr_n);
		ast_set_op(a, nc, ast_op(a, shr_n));
		ast_set_sym(a, nc, ast_sym(a, shr_n));
		ast_set_ival(a, nc, ast_ival(a, shr_n));
		ast_set_fbits(a, nc, ast_fbits(a, shr_n));
		ast_clear_children(a, top);
		ast_set_kind(a, top, AST_Unary);
		ast_set_op(a, top, AST_OP_ROTR);
		ast_set_ival(a, top, (uint64_t)sbytes << 8);
		ast_set_type(a, top, btype, ast_type_ref(a, base));
		ast_set_sym(a, top, 0);
		ast_add_child(a, top, nb);
		ast_add_child(a, top, nc);
	}
	return 1;
}

#endif
static int ast_bswap_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n;
	int total = 0;
	if (!ast_bswap_idiom_env && !ast_rotate_idiom_env)
		{ MCC_TRACE("br\n"); return 0; }
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (a->kind[n] != AST_Binary || a->op[n] != '|' || a->nchild[n] != 2)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_bswap_idiom_env)
			{ MCC_TRACE("br\n"); total += ast_bswap_try(a, n); }
#if defined(MCC_TARGET_X86_64)
		if (ast_rotate_idiom_env && a->kind[n] == AST_Binary && a->op[n] == '|') { MCC_TRACE("br\n");
			int rf = ast_rotate_try(a, n);
			if (!rf && a->kind[n] == AST_Binary && a->op[n] == '|')
				{ MCC_TRACE("br\n"); rf = ast_rotate_var_try(a, n); }
			if (!rf && a->kind[n] == AST_Binary && a->op[n] == '|')
				{ MCC_TRACE("br\n"); rf = ast_rotate_varr_try(a, n); }
			total += rf;
		}
#endif
	}
	return total;
}

static void ast_run_templates(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n;
	ast_tmpl_folds = 0;
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstLocal x, y;
		if (a->kind[n] != AST_Binary || a->nchild[n] != 2 ||
				!ast_fold_op_ok(a->op[n]))
			{ MCC_TRACE("br\n"); continue; }
		x = a->first_child[n];
		if (x == AST_NONE || a->kind[x] != AST_Literal)
			{ MCC_TRACE("br\n"); continue; }
		y = a->next_sib[x];
		if (y != AST_NONE && a->kind[y] == AST_Literal)
			{ MCC_TRACE("br\n"); ast_fold_rec(a, ast_root(a)); return; }
	}
}

static const struct {
	const char *name;
	unsigned char id, nargs, flt;
} ast_bfold_tab[] = {
		{"sqrt", 0, 1, 0},     {"sqrtf", 0, 1, 1},
		{"fabs", 1, 1, 0},     {"fabsf", 1, 1, 1},
		{"floor", 2, 1, 0},    {"floorf", 2, 1, 1},
		{"ceil", 3, 1, 0},     {"ceilf", 3, 1, 1},
		{"trunc", 4, 1, 0},    {"truncf", 4, 1, 1},
		{"copysign", 5, 2, 0}, {"copysignf", 5, 2, 1},
		{"fmin", 6, 2, 0},     {"fminf", 6, 2, 1},
		{"fmax", 7, 2, 0},     {"fmaxf", 7, 2, 1},
		{"round", 8, 1, 0},    {"roundf", 8, 1, 1},
		{"rint", 9, 1, 0},     {"rintf", 9, 1, 1},
		{"nearbyint", 10, 1, 0}, {"nearbyintf", 10, 1, 1},
		{"fma", 11, 3, 0},     {"fmaf", 11, 3, 1},
};

static uint64_t ast_bfold_mul128(uint64_t a, uint64_t b, uint64_t *lo) { MCC_TRACE("enter\n");
	uint64_t a0 = a & 0xffffffffu, a1 = a >> 32;
	uint64_t b0 = b & 0xffffffffu, b1 = b >> 32;
	uint64_t p00 = a0 * b0;
	uint64_t p01 = a0 * b1;
	uint64_t p10 = a1 * b0;
	uint64_t p11 = a1 * b1;
	uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
	*lo = (mid << 32) | (p00 & 0xffffffffu);
	return p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

static int ast_bfold_sq_gt(uint64_t s, uint64_t hi, uint64_t lo) { MCC_TRACE("enter\n");
	uint64_t sqlo, sqhi = ast_bfold_mul128(s, s, &sqlo);
	return sqhi > hi || (sqhi == hi && sqlo > lo);
}

static double ast_bfold_sqrt(double x) { MCC_TRACE("enter\n");
	uint64_t ix;
	memcpy(&ix, &x, sizeof ix);
	if (ix == 0 || ix == 0x8000000000000000ull || ix == 0x7ff0000000000000ull)
		{ MCC_TRACE("br\n"); return x; }
	int e = (int)(ix >> 52) & 0x7ff;
	uint64_t m = ix & 0x000fffffffffffffull;
	int E;
	if (e == 0) { MCC_TRACE("br\n");
		E = -1074;
		while (m < (1ull << 52)) { MCC_TRACE("br\n");
			m <<= 1;
			E--;
		}
	} else { MCC_TRACE("br\n");
		E = e - 1075;
		m |= 1ull << 52;
	}
	if (E & 1) { MCC_TRACE("br\n");
		m <<= 1;
		E--;
	}
	double dm = (double)m, r;
	uint64_t g;
	memcpy(&g, &dm, sizeof g);
	g = (g >> 1) + 0x1ff8000000000000ull;
	memcpy(&r, &g, sizeof r);
	for (int i = 0; i < 5; i++)
		{ MCC_TRACE("br\n"); r = 0.5 * (r + dm / r); }
	uint64_t S = (uint64_t)(r * 67108864.0);
	uint64_t hi = m >> 10, lo = m << 54;
	if (S < (1ull << 52))
		{ MCC_TRACE("br\n"); S = 1ull << 52; }
	if (S > (1ull << 53) - 1)
		{ MCC_TRACE("br\n"); S = (1ull << 53) - 1; }
	while (!ast_bfold_sq_gt(2 * S + 1, hi, lo))
		{ MCC_TRACE("br\n"); S++; }
	while (ast_bfold_sq_gt(2 * S - 1, hi, lo))
		{ MCC_TRACE("br\n"); S--; }
	uint64_t rbits = ((uint64_t)(E / 2 - 26 + 52 + 1023) << 52) + (S - (1ull << 52));
	memcpy(&r, &rbits, sizeof r);
	return r;
}

static double ast_bfold_trunc(double x) { MCC_TRACE("enter\n");
	if (x >= 9007199254740992.0 || x <= -9007199254740992.0)
		{ MCC_TRACE("br\n"); return x; }
	uint64_t ix, it;
	double t = (double)(int64_t)x;
	memcpy(&ix, &x, sizeof ix);
	memcpy(&it, &t, sizeof it);
	it |= ix & 0x8000000000000000ull;
	memcpy(&t, &it, sizeof it);
	return t;
}

static double ast_bfold_floor(double x) { MCC_TRACE("enter\n");
	double t = ast_bfold_trunc(x);
	return t > x ? t - 1.0 : t;
}

static double ast_bfold_ceil(double x) { MCC_TRACE("enter\n");
	double t = ast_bfold_trunc(x);
	return t < x ? t + 1.0 : t;
}

static double ast_bfold_round(double x) { MCC_TRACE("enter\n");
	double t = ast_bfold_trunc(x);
	double d = x - t;
	if (d >= 0.5)
		{ MCC_TRACE("br\n"); return t + 1.0; }
	if (d <= -0.5)
		{ MCC_TRACE("br\n"); return t - 1.0; }
	return t;
}

static int ast_bfold_eval_f(int id, uint32_t b0, uint32_t b1, uint64_t *out) { MCC_TRACE("enter\n");
	float x0, x1, r;
	uint32_t rb;
	memcpy(&x0, &b0, sizeof x0);
	memcpy(&x1, &b1, sizeof x1);
	if (x0 != x0 || x1 != x1)
		{ MCC_TRACE("br\n"); return 0; }
	switch (id) { MCC_TRACE("br\n");
	case 0:
		if (x0 < 0)
			{ MCC_TRACE("br\n"); return 0; }
		r = (float)ast_bfold_sqrt(x0);
		break;
	case 1:
		*out = b0 & 0x7fffffffu;
		return 1;
	case 2:
		r = (float)ast_bfold_floor(x0);
		break;
	case 3:
		r = (float)ast_bfold_ceil(x0);
		break;
	case 4:
		r = (float)ast_bfold_trunc(x0);
		break;
	case 5:
		*out = (b0 & 0x7fffffffu) | (b1 & 0x80000000u);
		return 1;
	case 8:
		r = (float)ast_bfold_round(x0);
		break;
	case 9:
	case 10:
		return 0;
	default:
		if (x0 == 0 && x1 == 0 && ((b0 ^ b1) >> 31))
			{ MCC_TRACE("br\n"); return 0; }
		r = id == 6 ? (x0 < x1 ? x0 : x1) : (x0 > x1 ? x0 : x1);
		break;
	}
	memcpy(&rb, &r, sizeof rb);
	*out = rb;
	return 1;
}

static int ast_bfold_eval_d(int id, uint64_t b0, uint64_t b1, uint64_t *out) { MCC_TRACE("enter\n");
	double x0, x1, r;
	memcpy(&x0, &b0, sizeof x0);
	memcpy(&x1, &b1, sizeof x1);
	if (x0 != x0 || x1 != x1)
		{ MCC_TRACE("br\n"); return 0; }
	switch (id) { MCC_TRACE("br\n");
	case 0:
		if (x0 < 0)
			{ MCC_TRACE("br\n"); return 0; }
		r = ast_bfold_sqrt(x0);
		break;
	case 1:
		*out = b0 & 0x7fffffffffffffffull;
		return 1;
	case 2:
		r = ast_bfold_floor(x0);
		break;
	case 3:
		r = ast_bfold_ceil(x0);
		break;
	case 4:
		r = ast_bfold_trunc(x0);
		break;
	case 5:
		*out = (b0 & 0x7fffffffffffffffull) | (b1 & 0x8000000000000000ull);
		return 1;
	case 8:
		r = ast_bfold_round(x0);
		break;
	case 9:
	case 10:
		return 0;
	default:
		if (x0 == 0 && x1 == 0 && ((b0 ^ b1) >> 63))
			{ MCC_TRACE("br\n"); return 0; }
		r = id == 6 ? (x0 < x1 ? x0 : x1) : (x0 > x1 ? x0 : x1);
		break;
	}
	memcpy(out, &r, sizeof r);
	return 1;
}

static AstLocal ast_bfold_arg(AstArena *a, AstLocal arg, int bt) { MCC_TRACE("enter\n");
	while (ast_kind(a, arg) == AST_Convert && ast_nchild(a, arg) == 1 &&
				 (ast_type_t(a, arg) & VT_BTYPE) == bt)
		{ MCC_TRACE("br\n"); arg = ast_first_child(a, arg); }
	if (ast_kind(a, arg) != AST_Literal ||
			(ast_op(a, arg) & (VT_VALMASK | VT_LVAL | VT_SYM | VT_NONCONST)) != VT_CONST ||
			(ast_type_t(a, arg) & VT_BTYPE) != bt)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	return arg;
}

static void ast_bfold_emit(AstArena *a, AstLocal n, int bt, uint64_t res) { MCC_TRACE("enter\n");
	ast_set_kind(a, n, AST_Literal);
	ast_clear_children(a, n);
	ast_set_op(a, n, VT_CONST);
	ast_set_type(a, n, bt, 0);
	ast_set_ival(a, n, res);
	ast_set_sym(a, n, 0);
}

static int ast_bfold_minmax_inf(AstArena *a, AstLocal n, int bid, int bt,
																uint64_t *out) { MCC_TRACE("enter\n");
	uint64_t dom = bt == VT_FLOAT ? (bid == 6 ? 0xff800000ull : 0x7f800000ull)
															 : (bid == 6 ? 0xfff0000000000000ull
																					 : 0x7ff0000000000000ull);
	uint64_t mask = bt == VT_FLOAT ? 0xffffffffull : ~0ull;
	for (int i = 0; i < 2; i++) { MCC_TRACE("br\n");
		AstLocal lit = ast_bfold_arg(a, ast_child(a, n, i + 1), bt);
		if (lit != AST_NONE && (ast_ival(a, lit) & mask) == dom) { MCC_TRACE("br\n");
			*out = dom;
			return 1;
		}
	}
	return 0;
}

static int ast_struct_eq(AstArena *a, AstLocal x, AstLocal y, int depth) { MCC_TRACE("enter\n");
	if (x == y)
		{ MCC_TRACE("br\n"); return 1; }
	if (x == AST_NONE || y == AST_NONE || depth <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, x) != ast_kind(a, y) || ast_op(a, x) != ast_op(a, y) ||
			ast_type_t(a, x) != ast_type_t(a, y) ||
			ast_type_ref(a, x) != ast_type_ref(a, y) ||
			ast_ival(a, x) != ast_ival(a, y) || ast_sym(a, x) != ast_sym(a, y))
		{ MCC_TRACE("br\n"); return 0; }
	uint32_t nx = ast_nchild(a, x);
	if (nx != ast_nchild(a, y))
		{ MCC_TRACE("br\n"); return 0; }
	for (uint32_t i = 0; i < nx; i++)
		if (!ast_struct_eq(a, ast_child(a, x, i), ast_child(a, y, i), depth - 1))
			{ MCC_TRACE("br\n"); return 0; }
	return 1;
}

static int ast_expr_pure(AstArena *a, AstLocal n, int depth) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 1; }
	if (depth <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Invoke || (ast_type_t(a, n) & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	uint32_t nc = ast_nchild(a, n);
	for (uint32_t i = 0; i < nc; i++)
		if (!ast_expr_pure(a, ast_child(a, n, i), depth - 1))
			{ MCC_TRACE("br\n"); return 0; }
	return 1;
}

static int ast_local_nonneg(AstArena *a, AstLocal ref, int depth);

static int ast_expr_nonneg(AstArena *a, AstLocal n, int depth) { MCC_TRACE("enter\n");
	if (n == AST_NONE || depth <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	while (ast_kind(a, n) == AST_Convert && ast_nchild(a, n) == 1)
		{ MCC_TRACE("br\n"); n = ast_first_child(a, n); }
	int k = ast_kind(a, n);
	if (k == AST_Ref && ast_math_inline_prepass_env)
		{ MCC_TRACE("br\n"); return ast_local_nonneg(a, n, depth); }
	if (k == AST_Literal) { MCC_TRACE("br\n");
		int bt = ast_type_t(a, n) & VT_BTYPE;
		uint64_t v = ast_ival(a, n);
		if (bt == VT_DOUBLE)
			{ MCC_TRACE("br\n"); return !(v >> 63) &&
				!((v & 0x7ff0000000000000ull) == 0x7ff0000000000000ull &&
					(v & 0x000fffffffffffffull)); }
		if (bt == VT_FLOAT)
			{ MCC_TRACE("br\n"); uint32_t f = (uint32_t)v; return !(f >> 31) &&
				!((f & 0x7f800000u) == 0x7f800000u && (f & 0x007fffffu)); }
		return 0;
	}
	if (k == AST_Unary && ast_op(a, n) == AST_OP_FABS)
		{ MCC_TRACE("br\n"); return 1; }
	if (k == AST_Binary) { MCC_TRACE("br\n");
		int op = ast_op(a, n);
		AstLocal l = ast_child(a, n, 0), r = ast_child(a, n, 1);
		if (op == '*' && ast_struct_eq(a, l, r, 12) && ast_expr_pure(a, l, 16))
			{ MCC_TRACE("br\n"); return 1; }
		if (op == '+' || op == '*' || op == '/')
			{ MCC_TRACE("br\n"); return ast_expr_nonneg(a, l, depth - 1) &&
				ast_expr_nonneg(a, r, depth - 1); }
		return 0;
	}
	return 0;
}

static int ast_local_nonneg(AstArena *a, AstLocal ref, int depth) { MCC_TRACE("enter\n");
	if (depth <= 0 || ast_kind(a, ref) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	int rop = ast_op(a, ref);
	if ((rop & VT_VALMASK) != VT_LOCAL || (rop & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	int64_t off = (int64_t)ast_ival(a, ref);
	AstLocal nn = ast_count(a), def = AST_NONE;
	int ndefs = 0;
	for (AstLocal m = 0; m < nn; m++) { MCC_TRACE("br\n");
		int mk = ast_kind(a, m);
		if (mk == AST_Unary && ast_op(a, m) == AST_OP_ADDR) { MCC_TRACE("br\n");
			AstLocal c = ast_first_child(a, m);
			if (c != AST_NONE && ast_kind(a, c) == AST_Ref &&
					(ast_op(a, c) & VT_VALMASK) == VT_LOCAL && !(ast_op(a, c) & VT_SYM) &&
					(int64_t)ast_ival(a, c) == off)
				{ MCC_TRACE("br\n"); return 0; }
		}
		if (mk == AST_Store) { MCC_TRACE("br\n");
			AstLocal tgt = ast_child(a, m, 0);
			if (tgt != AST_NONE && ast_kind(a, tgt) == AST_Ref &&
					(ast_op(a, tgt) & VT_VALMASK) == VT_LOCAL && !(ast_op(a, tgt) & VT_SYM) &&
					(int64_t)ast_ival(a, tgt) == off) { MCC_TRACE("br\n");
				ndefs++;
				def = m;
			}
		}
	}
	if (ndefs != 1 || def == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_expr_nonneg(a, ast_child(a, def, 1), depth - 1);
}

static AstLocal ast_local_def_value(AstArena *a, AstLocal ref) { MCC_TRACE("enter\n");
	if (ast_kind(a, ref) != AST_Ref)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	int rop = ast_op(a, ref);
	if ((rop & VT_VALMASK) != VT_LOCAL || (rop & VT_SYM))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	int64_t off = (int64_t)ast_ival(a, ref);
	AstLocal nn = ast_count(a), def = AST_NONE;
	int ndefs = 0;
	for (AstLocal m = 0; m < nn; m++) { MCC_TRACE("br\n");
		int mk = ast_kind(a, m);
		if (mk == AST_Unary && ast_op(a, m) == AST_OP_ADDR) { MCC_TRACE("br\n");
			AstLocal c = ast_first_child(a, m);
			if (c != AST_NONE && ast_kind(a, c) == AST_Ref &&
					(ast_op(a, c) & VT_VALMASK) == VT_LOCAL && !(ast_op(a, c) & VT_SYM) &&
					(int64_t)ast_ival(a, c) == off)
				{ MCC_TRACE("br\n"); return AST_NONE; }
		}
		if (mk == AST_Store) { MCC_TRACE("br\n");
			AstLocal tgt = ast_child(a, m, 0);
			if (tgt != AST_NONE && ast_kind(a, tgt) == AST_Ref &&
					(ast_op(a, tgt) & VT_VALMASK) == VT_LOCAL && !(ast_op(a, tgt) & VT_SYM) &&
					(int64_t)ast_ival(a, tgt) == off) { MCC_TRACE("br\n");
				ndefs++;
				def = m;
			}
		}
	}
	if (ndefs != 1 || def == AST_NONE)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	return ast_child(a, def, 1);
}

typedef struct {
	int64_t lo, hi;
} AstIval;

static int ast_pred_ivals(int op, int64_t c, int64_t tlo, int64_t thi,
													AstIval *out) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case TOK_LT:
		if (c <= tlo)
			{ MCC_TRACE("br\n"); return 0; }
		out[0].lo = tlo, out[0].hi = (c > thi ? thi : c - 1);
		return 1;
	case TOK_LE:
		if (c < tlo)
			{ MCC_TRACE("br\n"); return 0; }
		out[0].lo = tlo, out[0].hi = (c >= thi ? thi : c);
		return 1;
	case TOK_GT:
		if (c >= thi)
			{ MCC_TRACE("br\n"); return 0; }
		out[0].lo = (c < tlo ? tlo : c + 1), out[0].hi = thi;
		return 1;
	case TOK_GE:
		if (c > thi)
			{ MCC_TRACE("br\n"); return 0; }
		out[0].lo = (c <= tlo ? tlo : c), out[0].hi = thi;
		return 1;
	case TOK_EQ:
		if (c < tlo || c > thi)
			{ MCC_TRACE("br\n"); return 0; }
		out[0].lo = out[0].hi = c;
		return 1;
	case TOK_NE: {
		int k = 0;
		if (c < tlo || c > thi) { MCC_TRACE("br\n");
			out[0].lo = tlo, out[0].hi = thi;
			return 1;
		}
		if (c > tlo)
			{ MCC_TRACE("br\n"); out[k].lo = tlo, out[k].hi = c - 1, k++; }
		if (c < thi)
			{ MCC_TRACE("br\n"); out[k].lo = c + 1, out[k].hi = thi, k++; }
		return k;
	}
	}
	return -1;
}

static int ast_ival_cmp(const void *x, const void *y) { MCC_TRACE("enter\n");
	const AstIval *p = x, *q = y;
	return p->lo < q->lo ? -1 : p->lo > q->lo ? 1 : 0;
}

static int ast_ivals_cover(AstIval *v, int n, int64_t tlo, int64_t thi) { MCC_TRACE("enter\n");
	int i;
	int64_t reach;
	if (n <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	qsort(v, (size_t)n, sizeof *v, ast_ival_cmp);
	if (v[0].lo > tlo)
		{ MCC_TRACE("br\n"); return 0; }
	reach = v[0].hi;
	for (i = 1; i < n; i++) { MCC_TRACE("br\n");
		if (reach >= thi)
			{ MCC_TRACE("br\n"); return 1; }
		if (v[i].lo > reach + 1)
			{ MCC_TRACE("br\n"); return 0; }
		if (v[i].hi > reach)
			{ MCC_TRACE("br\n"); reach = v[i].hi; }
	}
	return reach >= thi;
}

static int ast_rel_negate(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case TOK_LT: return TOK_GE;
	case TOK_GE: return TOK_LT;
	case TOK_LE: return TOK_GT;
	case TOK_GT: return TOK_LE;
	case TOK_EQ: return TOK_NE;
	case TOK_NE: return TOK_EQ;
	}
	return -1;
}

static int ast_rel_swap(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case TOK_LT: return TOK_GT;
	case TOK_GT: return TOK_LT;
	case TOK_LE: return TOK_GE;
	case TOK_GE: return TOK_LE;
	case TOK_ULT: return TOK_UGT;
	case TOK_UGT: return TOK_ULT;
	case TOK_ULE: return TOK_UGE;
	case TOK_UGE: return TOK_ULE;
	}
	return -1;
}

static int ast_bt_bits(int t) { MCC_TRACE("enter\n");
	switch (t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_BYTE: return 8;
	case VT_SHORT: return 16;
	case VT_INT: return 32;
	case VT_LLONG: return 64;
	}
	return 0;
}

static int ast_eff_bits(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int b = ast_bt_bits(ast_type_t(a, n));
	if (b)
		{ MCC_TRACE("br\n"); return b; }
	if (ast_kind(a, n) == AST_Binary && ast_nchild(a, n) == 2 &&
			!ast_type_t(a, n))
		{ MCC_TRACE("br\n"); return ast_bt_bits(ast_type_t(a, ast_child(a, n, 0))); }
	return 0;
}

static int ast_rel_operand(AstArena *a, AstLocal n, AstLocal *x, int64_t *c,
													 int *op) { MCC_TRACE("enter\n");
	AstLocal l, r;
	int t, bt, bits;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_fbits(a, n))
		{ MCC_TRACE("br\n"); return 0; }
	*op = ast_op(a, n);
	switch (*op) { MCC_TRACE("br\n");
	case TOK_LT: case TOK_LE: case TOK_GT: case TOK_GE:
	case TOK_EQ: case TOK_NE:
		break;
	default:
		return 0;
	}
	l = ast_child(a, n, 0), r = ast_child(a, n, 1);
	if (ast_kind(a, r) != AST_Literal)
		{ MCC_TRACE("br\n"); return 0; }

	if (*op == TOK_LT && ast_kind(a, l) == AST_Convert &&
			ast_nchild(a, l) == 1 && !ast_fbits(a, l)) { MCC_TRACE("br\n");
		AstLocal inner = ast_first_child(a, l);
		int it = ast_type_t(a, inner), ibt = it & VT_BTYPE;
		int ibits = ibt == VT_BYTE ? 8 : ibt == VT_SHORT ? 16
								: ibt == VT_INT ? 32 : ibt == VT_LLONG ? 64 : 0;
		int obits = (ast_type_t(a, l) & VT_BTYPE) == VT_LLONG ? 64 : 32;
		if (ibits && (it & VT_UNSIGNED) && ibits < obits &&
				(ast_type_t(a, l) & VT_UNSIGNED)) { MCC_TRACE("br\n");
			uint64_t umax = ibits >= 64 ? ~(uint64_t)0
																	: (((uint64_t)1 << ibits) - 1);
			if ((uint64_t)ast_ival(a, r) == umax) { MCC_TRACE("br\n");
				*op = TOK_NE;
				l = inner;
			}
		}
	}

	if (*op == TOK_EQ || *op == TOK_NE) { MCC_TRACE("br\n");
		uint64_t cc = ast_ival(a, r);
		int guard = 0;
		while (guard++ < 8) { MCC_TRACE("br\n");
			if (ast_kind(a, l) == AST_Convert && ast_nchild(a, l) == 1 &&
					!ast_fbits(a, l)) { MCC_TRACE("br\n");
				AstLocal inner = ast_first_child(a, l);
				int ib = ast_eff_bits(a, inner), ob = ast_eff_bits(a, l);
				if (!ib || ib != ob)
					{ MCC_TRACE("br\n"); break; }
				l = inner;
				continue;
			}
			if (ast_kind(a, l) == AST_Binary && ast_nchild(a, l) == 2 &&
					!ast_fbits(a, l) &&
					(ast_op(a, l) == '+' || ast_op(a, l) == '-')) { MCC_TRACE("br\n");
				AstLocal bl = ast_child(a, l, 0), brr = ast_child(a, l, 1);
				int lb = ast_type_t(a, bl) & VT_BTYPE;
				if (ast_kind(a, brr) != AST_Literal ||
						(ast_type_t(a, bl) & (VT_UNSIGNED | VT_BITFIELD)) ||
						(lb != VT_INT && lb != VT_LLONG))
					{ MCC_TRACE("br\n"); break; }
				if (ast_op(a, l) == '-')
					{ MCC_TRACE("br\n"); cc = cc + ast_ival(a, brr); }
				else
					{ MCC_TRACE("br\n"); cc = cc - ast_ival(a, brr); }
				l = bl;
				continue;
			}
			break;
		}
		if (l != ast_child(a, n, 0)) { MCC_TRACE("br\n");
			int nb = ast_type_t(a, l) & VT_BTYPE;
			int nbits = nb == VT_BYTE ? 8 : nb == VT_SHORT ? 16
									: nb == VT_INT ? 32 : nb == VT_LLONG ? 64 : 0;
			if (!nbits || (ast_type_t(a, l) & (VT_UNSIGNED | VT_BITFIELD)))
				{ MCC_TRACE("br\n"); return 0; }
			*c = (int64_t)cc;
			if (nbits < 64)
				{ MCC_TRACE("br\n"); *c = (*c << (64 - nbits)) >> (64 - nbits); }
			*x = l;
			return nbits;
		}
	}

	t = ast_type_t(a, l);
	bt = t & VT_BTYPE;
	if (t & (VT_UNSIGNED | VT_BITFIELD))
		{ MCC_TRACE("br\n"); return 0; }
	if (bt == VT_BYTE)
		bits = 8;
	else if (bt == VT_SHORT)
		bits = 16;
	else if (bt == VT_INT)
		bits = 32;
	else if (bt == VT_LLONG)
		bits = 64;
	else
		{ MCC_TRACE("br\n"); return 0; }
	if ((ast_type_t(a, r) & VT_BTYPE) != bt)
		{ MCC_TRACE("br\n"); return 0; }
	*c = (int64_t)ast_ival(a, r);
	if (bits < 64)
		{ MCC_TRACE("br\n"); *c = (*c << (64 - bits)) >> (64 - bits); }
	*x = l;
	return bits;
}

static int ast_logic_tautology(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal l, r, xl, xr;
	int64_t cl, cr, tlo, thi;
	int opl, opr, bl, br, nl, nr, lor, val;
	uint64_t fb;
	AstIval v[4];
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	lor = ast_op(a, n) == TOK_LOR;
	if (!lor && ast_op(a, n) != TOK_LAND)
		{ MCC_TRACE("br\n"); return 0; }
	fb = ast_fbits(a, n);
	if (fb & ~(uint64_t)(AST_FB_LANDOR_INVERT | AST_FB_LANDOR_MATERIAL))
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0), r = ast_child(a, n, 1);
	bl = ast_rel_operand(a, l, &xl, &cl, &opl);
	br = ast_rel_operand(a, r, &xr, &cr, &opr);
	if (!bl || bl != br)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_struct_eq(a, xl, xr, 12) || !ast_expr_pure(a, xl, 16))
		{ MCC_TRACE("br\n"); return 0; }
	tlo = bl == 64 ? INT64_MIN : -((int64_t)1 << (bl - 1));
	thi = bl == 64 ? INT64_MAX : ((int64_t)1 << (bl - 1)) - 1;
	if (!lor) { MCC_TRACE("br\n");
		opl = ast_rel_negate(opl), opr = ast_rel_negate(opr);
		if (opl < 0 || opr < 0)
			{ MCC_TRACE("br\n"); return 0; }
	}
	nl = ast_pred_ivals(opl, cl, tlo, thi, v);
	if (nl < 0)
		{ MCC_TRACE("br\n"); return 0; }
	nr = ast_pred_ivals(opr, cr, tlo, thi, v + nl);
	if (nr < 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ivals_cover(v, nl + nr, tlo, thi))
		{ MCC_TRACE("br\n"); return 0; }
	val = lor ? 1 : 0;
	if (fb & AST_FB_LANDOR_INVERT)
		{ MCC_TRACE("br\n"); val = !val; }
	ast_bfold_emit(a, n, VT_INT, (uint64_t)val);
	ast_set_fbits(a, n, 0);
	return 1;
}

static int ast_cmp_nonneg_lt_zero(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal l, r;
	uint64_t v;
	int bt;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != TOK_LT ||
			ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	bt = ast_type_t(a, l) & VT_BTYPE;
	if (bt != VT_FLOAT && bt != VT_DOUBLE)
		{ MCC_TRACE("br\n"); return 0; }
	if ((ast_type_t(a, r) & VT_BTYPE) != bt)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, r) != AST_Literal && ast_math_inline_prepass_env) { MCC_TRACE("br\n");
		AstLocal rv = ast_local_def_value(a, r);
		if (rv != AST_NONE && (ast_type_t(a, rv) & VT_BTYPE) == bt)
			{ MCC_TRACE("br\n"); r = rv; }
	}
	if (ast_kind(a, r) != AST_Literal)
		{ MCC_TRACE("br\n"); return 0; }
	v = ast_ival(a, r);
	v &= bt == VT_FLOAT ? 0x7fffffffull : 0x7fffffffffffffffull;
	if (v != 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_expr_nonneg(a, l, 24) || !ast_expr_pure(a, l, 24))
		{ MCC_TRACE("br\n"); return 0; }
	ast_bfold_emit(a, n, VT_INT, 0);
	return 1;
}

static int ast_bfold_run(AstArena *a) { MCC_TRACE("enter\n");
	int folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		folds += ast_cmp_nonneg_lt_zero(a, n);
		folds += ast_logic_tautology(a, n);
	}
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int rbt;
		if (ast_kind(a, n) != AST_Invoke)
			{ MCC_TRACE("br\n"); continue; }
		rbt = ast_type_t(a, n) & VT_BTYPE;
		if (rbt != VT_FLOAT && rbt != VT_DOUBLE)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal cref = ast_first_child(a, n);
		if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		Sym *cs = (Sym *)(uintptr_t)ast_sym(a, cref);
		if (!cs || (cs->type.t & VT_BTYPE) != VT_FUNC || (cs->type.t & VT_STATIC))
			{ MCC_TRACE("br\n"); continue; }
		ElfSym *es = elfsym(cs);
		if (es && es->st_shndx != SHN_UNDEF)
			{ MCC_TRACE("br\n"); continue; }
		const char *nm = get_tok_str(cs->v, NULL);
		int nfn = (int)(sizeof ast_bfold_tab / sizeof *ast_bfold_tab);
		int bi;
		for (bi = 0; bi < nfn; bi++)
			{ MCC_TRACE("br\n"); if (!strcmp(nm, ast_bfold_tab[bi].name))
				{ MCC_TRACE("br\n"); break; } }
		if (bi == nfn)
			{ MCC_TRACE("br\n"); continue; }
		int bid = ast_bfold_tab[bi].id;
		int bgate = bid == 0							 ? ast_bfold_sqrt_env
								: bid == 1 || bid == 5 ? ast_bfold_sign_env
								: bid == 6 || bid == 7 ? ast_bfold_minmax_env
																			 : ast_bfold_round_env;
		if (!bgate)
			{ MCC_TRACE("br\n"); continue; }
		int bt = ast_bfold_tab[bi].flt ? VT_FLOAT : VT_DOUBLE;
		int nargs = ast_bfold_tab[bi].nargs;
		if ((ast_type_t(a, n) & VT_BTYPE) != bt ||
				(int)ast_nchild(a, n) != nargs + 1)
			{ MCC_TRACE("br\n"); continue; }
		if (bid == 11) { MCC_TRACE("br\n");
#if defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64)
			if (nargs == 3 && ast_fma_env) { MCC_TRACE("br\n");
				AstLocal x = ast_child(a, n, 1), y = ast_child(a, n, 2),
								 z = ast_child(a, n, 3);
				ast_clear_children(a, n);
				ast_set_kind(a, n, AST_Binary);
				ast_set_op(a, n, AST_OP_FMA);
				ast_set_type(a, n, bt, 0);
				ast_set_ival(a, n, 0);
				ast_set_sym(a, n, 0);
				ast_add_child(a, n, x);
				ast_add_child(a, n, y);
				ast_add_child(a, n, z);
				MCC_TRACE("math-inline fma flt=%d\n", (int)ast_bfold_tab[bi].flt);
				folds++;
			}
#endif
			continue;
		}
		uint64_t ab[2] = {0, 0};
		int i;
		for (i = 0; i < nargs; i++) { MCC_TRACE("br\n");
			AstLocal lit = ast_bfold_arg(a, ast_child(a, n, i + 1), bt);
			if (lit == AST_NONE)
				{ MCC_TRACE("br\n"); break; }
			ab[i] = ast_ival(a, lit);
		}
		if (i < nargs) { MCC_TRACE("br\n");
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_I386)
			if (bid == 1 && nargs == 1 && ast_math_inline_env &&
					ast_fabs_inline_env) { MCC_TRACE("br\n");
				AstLocal arg = ast_child(a, n, 1);
				ast_clear_children(a, n);
				ast_set_kind(a, n, AST_Unary);
				ast_set_op(a, n, AST_OP_FABS);
				ast_set_type(a, n, bt, 0);
				ast_set_ival(a, n, 0);
				ast_set_sym(a, n, 0);
				ast_add_child(a, n, arg);
				MCC_TRACE("math-inline fabs flt=%d\n", (int)ast_bfold_tab[bi].flt);
				folds++;
				continue;
			}
			if (bid == 0 && nargs == 1 && ast_math_inline_env &&
					(ast_no_math_errno ||
					 ast_expr_nonneg(a, ast_child(a, n, 1), 24))) { MCC_TRACE("br\n");
				AstLocal arg = ast_child(a, n, 1);
				ast_clear_children(a, n);
				ast_set_kind(a, n, AST_Unary);
				ast_set_op(a, n, AST_OP_SQRT);
				ast_set_type(a, n, bt, 0);
				ast_set_ival(a, n, 0);
				ast_set_sym(a, n, 0);
				ast_add_child(a, n, arg);
				MCC_TRACE("math-inline sqrt(nonneg) flt=%d\n", (int)ast_bfold_tab[bi].flt);
				folds++;
				continue;
			}
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
			if ((bid == 2 || bid == 3 || bid == 4) && nargs == 1 &&
					ast_round_inline_env) { MCC_TRACE("br\n");
				AstLocal arg = ast_child(a, n, 1);
				ast_clear_children(a, n);
				ast_set_kind(a, n, AST_Unary);
				ast_set_op(a, n, bid == 2 ? AST_OP_FLOOR
										: bid == 3 ? AST_OP_CEIL
															 : AST_OP_TRUNC);
				ast_set_type(a, n, bt, 0);
				ast_set_ival(a, n, 0);
				ast_set_sym(a, n, 0);
				ast_add_child(a, n, arg);
				MCC_TRACE("math-inline round bid=%d flt=%d\n", bid, (int)ast_bfold_tab[bi].flt);
				folds++;
				continue;
			}
			if ((bid == 9 || bid == 10) && nargs == 1 &&
					ast_round_inline_env) { MCC_TRACE("br\n");
				AstLocal arg = ast_child(a, n, 1);
				ast_clear_children(a, n);
				ast_set_kind(a, n, AST_Unary);
				ast_set_op(a, n, bid == 9 ? AST_OP_RINT : AST_OP_NEARBYINT);
				ast_set_type(a, n, bt, 0);
				ast_set_ival(a, n, 0);
				ast_set_sym(a, n, 0);
				ast_add_child(a, n, arg);
				MCC_TRACE("math-inline rint/nearbyint bid=%d flt=%d\n", bid, (int)ast_bfold_tab[bi].flt);
				folds++;
				continue;
			}
#endif
#if defined(MCC_TARGET_ARM64)
			if (bid == 8 && nargs == 1 && ast_round_inline_env) { MCC_TRACE("br\n");
				AstLocal arg = ast_child(a, n, 1);
				ast_clear_children(a, n);
				ast_set_kind(a, n, AST_Unary);
				ast_set_op(a, n, AST_OP_ROUND);
				ast_set_type(a, n, bt, 0);
				ast_set_ival(a, n, 0);
				ast_set_sym(a, n, 0);
				ast_add_child(a, n, arg);
				MCC_TRACE("math-inline round(FRINTA) flt=%d\n", (int)ast_bfold_tab[bi].flt);
				folds++;
				continue;
			}
#endif
#if defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64)
			if (bid == 5 && nargs == 2 && ast_copysign_env &&
					ast_bfold_sign_env) { MCC_TRACE("br\n");
				AstLocal x = ast_child(a, n, 1), y = ast_child(a, n, 2);
				ast_clear_children(a, n);
				ast_set_kind(a, n, AST_Binary);
				ast_set_op(a, n, AST_OP_COPYSIGN);
				ast_set_type(a, n, bt, 0);
				ast_set_ival(a, n, 0);
				ast_set_sym(a, n, 0);
				ast_add_child(a, n, x);
				ast_add_child(a, n, y);
				MCC_TRACE("math-inline copysign flt=%d\n", (int)ast_bfold_tab[bi].flt);
				folds++;
				continue;
			}
#endif
#if defined(MCC_TARGET_ARM64)
			if ((bid == 6 || bid == 7) && nargs == 2 && ast_minmax_inline_env &&
					ast_bfold_minmax_env) { MCC_TRACE("br\n");
				AstLocal x = ast_child(a, n, 1), y = ast_child(a, n, 2);
				ast_clear_children(a, n);
				ast_set_kind(a, n, AST_Binary);
				ast_set_op(a, n, bid == 6 ? AST_OP_FMIN : AST_OP_FMAX);
				ast_set_type(a, n, bt, 0);
				ast_set_ival(a, n, 0);
				ast_set_sym(a, n, 0);
				ast_add_child(a, n, x);
				ast_add_child(a, n, y);
				MCC_TRACE("math-inline fmin/fmax bid=%d flt=%d\n", bid, (int)ast_bfold_tab[bi].flt);
				folds++;
				continue;
			}
#endif
#endif
			uint64_t pres;
			if ((bid == 6 || bid == 7) &&
					ast_bfold_minmax_inf(a, n, bid, bt, &pres)) { MCC_TRACE("br\n");
				MCC_TRACE("bfold %s partial-minmax id=%d flt=%d res=0x%llx\n", nm, bid,
									(int)ast_bfold_tab[bi].flt, (unsigned long long)pres);
				ast_bfold_emit(a, n, bt, pres);
				folds++;
			}
			continue;
		}
		uint64_t res;
		int ok = ast_bfold_tab[bi].flt
								 ? ast_bfold_eval_f(ast_bfold_tab[bi].id, (uint32_t)ab[0],
																		(uint32_t)ab[1], &res)
								 : ast_bfold_eval_d(ast_bfold_tab[bi].id, ab[0], ab[1], &res);
		if (!ok)
			{ MCC_TRACE("br\n"); continue; }
		MCC_TRACE("bfold %s id=%d nargs=%d flt=%d res=0x%llx\n", nm,
							(int)ast_bfold_tab[bi].id, nargs, (int)ast_bfold_tab[bi].flt,
							(unsigned long long)res);
		ast_bfold_emit(a, n, bt, res);
		folds++;
	}
	return folds;
}

#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_I386)
static int ast_math_inline_run(AstArena *a) { MCC_TRACE("enter\n");
	if (!ast_math_inline_env)
		{ MCC_TRACE("br\n"); return 0; }
	int folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Invoke)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal cref = ast_first_child(a, n);
		if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		Sym *cs = (Sym *)(uintptr_t)ast_sym(a, cref);
		if (!cs || (cs->type.t & VT_BTYPE) != VT_FUNC || (cs->type.t & VT_STATIC))
			{ MCC_TRACE("br\n"); continue; }
		ElfSym *es = elfsym(cs);
		if (es && es->st_shndx != SHN_UNDEF)
			{ MCC_TRACE("br\n"); continue; }
		const char *nm = get_tok_str(cs->v, NULL);
		int nfn = (int)(sizeof ast_bfold_tab / sizeof *ast_bfold_tab), bi;
		for (bi = 0; bi < nfn; bi++)
			{ MCC_TRACE("br\n"); if (!strcmp(nm, ast_bfold_tab[bi].name))
				{ MCC_TRACE("br\n"); break; } }
		if (bi == nfn)
			{ MCC_TRACE("br\n"); continue; }
		int bid = ast_bfold_tab[bi].id;
#if defined(MCC_TARGET_ARM64)
		int is_round = (bid == 2 || bid == 3 || bid == 4 || bid == 8 ||
										bid == 9 || bid == 10) && ast_round_inline_env;
#elif defined(MCC_TARGET_X86_64)
		int is_round = (bid == 2 || bid == 3 || bid == 4 ||
										bid == 9 || bid == 10) && ast_round_inline_env;
#else
		int is_round = 0;
#endif
		if (bid != 0 && bid != 1 && !is_round)
			{ MCC_TRACE("br\n"); continue; }
		int bgate = bid == 0 ? ast_bfold_sqrt_env
							: bid == 1 ? (ast_bfold_sign_env && ast_fabs_inline_env)
												 : ast_bfold_round_env;
		if (!bgate)
			{ MCC_TRACE("br\n"); continue; }
		int bt = ast_bfold_tab[bi].flt ? VT_FLOAT : VT_DOUBLE;
		if ((ast_type_t(a, n) & VT_BTYPE) != bt || (int)ast_nchild(a, n) != 2)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_bfold_arg(a, ast_child(a, n, 1), bt) != AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		if (bid == 0 && !ast_no_math_errno &&
				!ast_expr_nonneg(a, ast_child(a, n, 1), 24))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal arg = ast_child(a, n, 1);
		ast_clear_children(a, n);
		ast_set_kind(a, n, AST_Unary);
		ast_set_op(a, n, bid == 0 ? AST_OP_SQRT
						: bid == 1 ? AST_OP_FABS
						: bid == 2 ? AST_OP_FLOOR
						: bid == 3 ? AST_OP_CEIL
						: bid == 4 ? AST_OP_TRUNC
						: bid == 8 ? AST_OP_ROUND
						: bid == 9 ? AST_OP_RINT
											 : AST_OP_NEARBYINT);
		ast_set_type(a, n, bt, 0);
		ast_set_ival(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_add_child(a, n, arg);
		MCC_TRACE("math-inline-prepass %s\n", nm);
		folds++;
	}
	return folds;
}
#else
static int ast_math_inline_run(AstArena *a) { MCC_TRACE("enter\n"); (void)a; return 0; }
#endif

static int ast_ident_intt(int tt) { MCC_TRACE("enter\n");
	if (tt & VT_BITFIELD)
		{ MCC_TRACE("br\n"); return 0; }
	switch (tt & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_BYTE:
	case VT_SHORT:
	case VT_INT:
	case VT_LLONG:
	case VT_BOOL:
		return 1;
	}
	return 0;
}

static int ast_float_rank(int bt) { MCC_TRACE("enter\n");
	switch (bt) { MCC_TRACE("br\n");
	case VT_FLOAT16:
	case VT_BF16:
		return 1;
	case VT_FLOAT:
		return 2;
	case VT_DOUBLE:
		return 3;
	case VT_LDOUBLE:
		return 4;
	case VT_QFLOAT:
	case VT_FLOAT128:
		return 5;
	}
	return 0;
}

static int ast_ident_common(int t1, int t2) { MCC_TRACE("enter\n");
	int fb1 = ast_float_rank(t1 & VT_BTYPE), fb2 = ast_float_rank(t2 & VT_BTYPE);
	if (fb1 || fb2)
		{ MCC_TRACE("br\n"); return (fb1 >= fb2 ? t1 : t2) & VT_BTYPE; }
	if ((t1 & VT_BTYPE) == VT_INT128 || (t2 & VT_BTYPE) == VT_INT128) { MCC_TRACE("br\n");
		if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT128 | VT_UNSIGNED) ||
				(t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT128 | VT_UNSIGNED))
			{ MCC_TRACE("br\n"); return VT_INT128 | VT_UNSIGNED; }
		return VT_INT128;
	}
	if ((t1 & VT_BTYPE) == VT_LLONG || (t2 & VT_BTYPE) == VT_LLONG) { MCC_TRACE("br\n");
		if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED) ||
				(t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED))
			{ MCC_TRACE("br\n"); return VT_LLONG | VT_UNSIGNED; }
		return VT_LLONG;
	}
	if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED) ||
			(t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED))
		{ MCC_TRACE("br\n"); return VT_INT | VT_UNSIGNED; }
	return VT_INT;
}

static int ast_ident_keep(int lt, int xt) { MCC_TRACE("enter\n");
	return ast_ident_common(lt, xt) == ast_ident_common(xt, xt);
}

static int ast_ident_etype(AstArena *a, AstLocal n, int *tt, uint64_t *ref) { MCC_TRACE("enter\n");
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_Literal:
	case AST_Ref:
	case AST_Convert:
	case AST_Invoke:
		*tt = ast_type_t(a, n);
		*ref = ast_type_ref(a, n);
		return 1;
	case AST_Unary:
		switch (ast_op(a, n)) { MCC_TRACE("br\n");
		case AST_OP_ADDR:
		case AST_OP_MEMBER:
		case AST_OP_MEMBER_ARROW:
			*tt = ast_type_t(a, n);
			*ref = ast_type_ref(a, n);
			return 1;
		}
		return 0;
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		int ct;
		uint64_t cref;
		if (c == AST_NONE || !ast_ident_etype(a, c, &ct, &cref))
			{ MCC_TRACE("br\n"); return 0; }
		if ((ct & VT_BTYPE) != VT_PTR || !cref)
			{ MCC_TRACE("br\n"); return 0; }
		Sym *ps = (Sym *)(uintptr_t)cref;
		*tt = ps->type.t;
		*ref = (uint64_t)(uintptr_t)ps->type.ref;
		return 1;
	}
	case AST_Binary: {
		int op = ast_op(a, n);
		switch (op) { MCC_TRACE("br\n");
		case TOK_LT:
		case TOK_GT:
		case TOK_LE:
		case TOK_GE:
		case TOK_EQ:
		case TOK_NE:
		case TOK_ULT:
		case TOK_UGE:
		case TOK_ULE:
		case TOK_UGT:
		case TOK_LAND:
		case TOK_LOR:
			*tt = VT_INT;
			*ref = 0;
			return 1;
		}
		if (ast_nchild(a, n) != 2)
			{ MCC_TRACE("br\n"); return 0; }
		int t1, t2;
		uint64_t r1, r2;
		if (!ast_ident_etype(a, ast_child(a, n, 0), &t1, &r1))
			{ MCC_TRACE("br\n"); return 0; }
		if (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR) { MCC_TRACE("br\n");
			if (!ast_ident_intt(t1) && (t1 & VT_BTYPE) != VT_INT128)
				{ MCC_TRACE("br\n"); return 0; }
			*tt = ast_ident_common(t1, t1);
			*ref = 0;
			return 1;
		}
		if (!ast_ident_etype(a, ast_child(a, n, 1), &t2, &r2))
			{ MCC_TRACE("br\n"); return 0; }
		int p1 = (t1 & VT_BTYPE) == VT_PTR, p2 = (t2 & VT_BTYPE) == VT_PTR;
		if (p1 || p2) { MCC_TRACE("br\n");
			if (op == '-' && p1 && p2) { MCC_TRACE("br\n");
				*tt = VT_PTRDIFF_T;
				*ref = 0;
				return 1;
			}
			if ((op != '+' && op != '-') || (p2 && op == '-'))
				{ MCC_TRACE("br\n"); return 0; }
			*tt = (p1 ? t1 : t2) & ~(VT_ARRAY | VT_VLA);
			*ref = p1 ? r1 : r2;
			return 1;
		}
		switch (op) { MCC_TRACE("br\n");
		case '+':
		case '-':
		case '*':
		case '/':
		case '%':
		case '&':
		case '|':
		case '^':
			break;
		default:
			return 0;
		}
		if ((op == '+' || op == '-' || op == '*' || op == '/') &&
				(is_float(t1) || is_float(t2))) { MCC_TRACE("br\n");
			if ((!is_float(t1) && !ast_ident_intt(t1)) ||
					(!is_float(t2) && !ast_ident_intt(t2)))
				{ MCC_TRACE("br\n"); return 0; }
			*tt = ast_ident_common(t1, t2);
			*ref = 0;
			return 1;
		}
		if ((!ast_ident_intt(t1) && (t1 & VT_BTYPE) != VT_INT128) ||
				(!ast_ident_intt(t2) && (t2 & VT_BTYPE) != VT_INT128))
			{ MCC_TRACE("br\n"); return 0; }
		*tt = ast_ident_common(t1, t2);
		*ref = 0;
		return 1;
	}
	}
	return 0;
}

static int ast_ident_pure_compute(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (ast_type_t(a, n) & VT_VOLATILE)
		{ MCC_TRACE("br\n"); return 0; }
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_Literal:
	case AST_Ref:
	case AST_Convert:
	case AST_If:
		break;
	case AST_Load: {
		AstLocal c = ast_first_child(a, n);
		int ct;
		uint64_t cref;
		if (c == AST_NONE || !ast_ident_etype(a, c, &ct, &cref))
			{ MCC_TRACE("br\n"); return 0; }
		if ((ct & VT_BTYPE) != VT_PTR || !cref)
			{ MCC_TRACE("br\n"); return 0; }
		if (((Sym *)(uintptr_t)cref)->type.t & VT_VOLATILE)
			{ MCC_TRACE("br\n"); return 0; }
		break;
	}
	case AST_Unary:
		switch (ast_op(a, n)) { MCC_TRACE("br\n");
		case AST_OP_ADDR:
		case AST_OP_MEMBER:
		case AST_OP_MEMBER_ARROW:
			break;
		default:
			return 0;
		}
		break;
	case AST_Binary:
		switch (ast_op(a, n)) { MCC_TRACE("br\n");
		case '+':
		case '-':
		case '*':
		case '&':
		case '|':
		case '^':
		case TOK_SHL:
		case TOK_SHR:
		case TOK_SAR:
		case TOK_LT:
		case TOK_GT:
		case TOK_LE:
		case TOK_GE:
		case TOK_EQ:
		case TOK_NE:
		case TOK_ULT:
		case TOK_UGE:
		case TOK_ULE:
		case TOK_UGT:
		case TOK_LAND:
		case TOK_LOR:
			break;
		default:
			return 0;
		}
		break;
	default:
		return 0;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_ident_pure(a, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

#if MCC_EMBED_JIT
static int ast_purity_op_effectfree(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case AST_OP_ADDR:
	case AST_OP_MEMBER:
	case AST_OP_MEMBER_ARROW:
	case AST_OP_IMAG:
	case AST_OP_MULHU:
	case AST_OP_MULHS:
	case AST_OP_FABS:
	case AST_OP_SQRT:
	case AST_OP_FLOOR:
	case AST_OP_CEIL:
	case AST_OP_TRUNC:
	case AST_OP_COPYSIGN:
	case AST_OP_ROUND:
	case AST_OP_FMIN:
	case AST_OP_FMAX:
	case AST_OP_RINT:
	case AST_OP_NEARBYINT:
	case AST_OP_FMA:
	case AST_OP_FNEG:
	case AST_OP_BSWAP:
	case AST_OP_SIGNBIT:
	case AST_OP_FFS:
	case AST_OP_BITSCAN:
	case AST_OP_CPLXBUILD:
		return 1;
	default:
		break;
	}
	if ((op & 0xffff0000) == 0x40000)
		{ MCC_TRACE("br\n"); return 0; }
	switch (op) { MCC_TRACE("br\n");
	case '+':
	case '-':
	case '*':
	case '/':
	case '%':
	case '&':
	case '|':
	case '^':
	case '~':
	case '!':
	case TOK_SHL:
	case TOK_SAR:
	case TOK_SHR:
	case TOK_NEG:
	case TOK_UDIV:
	case TOK_UMOD:
	case TOK_PDIV:
	case TOK_UMULL:
	case TOK_ADDC1:
	case TOK_ADDC2:
	case TOK_SUBC1:
	case TOK_SUBC2:
	case TOK_LAND:
	case TOK_LOR:
	case TOK_ULT:
	case TOK_UGE:
	case TOK_EQ:
	case TOK_NE:
	case TOK_ULE:
	case TOK_UGT:
	case TOK_LT:
	case TOK_GE:
	case TOK_LE:
	case TOK_GT:
		return 1;
	default:
		break;
	}
	return 0;
}

static int ast_purity_local_slot(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int r;
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	r = ast_op(a, n);
	return (r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM);
}

static int ast_purity_storeval_marker(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal st = (AstLocal)ast_ival(a, n);
	return st != AST_NONE && st < ast_count(a) && ast_kind(a, st) == AST_Store;
}

static int ast_purity_node(const AstArena *a, AstLocal n, int *has_load) { MCC_TRACE("enter\n");
	if (ast_type_t(a, n) & VT_VOLATILE)
		{ MCC_TRACE("br\n"); return 0; }
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_BasicBlock:
	case AST_If:
	case AST_Jump:
	case AST_Return:
	case AST_Literal:
	case AST_Convert:
		return 1;
	case AST_StoreVal:
		return ast_purity_storeval_marker(a, n);
	case AST_Load:
		*has_load = 1;
		return 1;
	case AST_Ref: { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & VT_LVAL) && (r & VT_VALMASK) != VT_LOCAL)
			{ MCC_TRACE("br\n"); *has_load = 1; }
		return 1;
	}
	case AST_Unary:
	case AST_Binary:
		return ast_purity_op_effectfree(ast_op(a, n));
	default:
		break;
	}
	return 0;
}

int ast_fn_purity(const AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n;
	int has_load = 0;
	if (ast_arena_has_hole(a))
		{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (!ast_purity_node(a, n, &has_load))
			{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
	}
	return has_load ? AST_PURITY_TIER1 : AST_PURITY_TIER0;
}

void ast_fn_slice_profile(const AstArena *a, AstSliceProfile *out) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n;
	out->impure_ops = 0;
	out->loads = 0;
	out->pure_compute = 0;
	out->nodes = (int)nn;
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_type_t(a, n) & VT_VOLATILE) { MCC_TRACE("br\n");
			out->impure_ops++;
			continue;
		}
		switch (ast_kind(a, n)) { MCC_TRACE("br\n");
		case AST_Store:
		case AST_Invoke:
			out->impure_ops++;
			break;
		case AST_Load:
			out->loads++;
			break;
		default:
			out->pure_compute++;
			break;
		}
	}
}

int ast_fn_purity_noescape(const AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n;
	int has_load = 0;
	if (ast_arena_has_hole(a))
		{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_type_t(a, n) & VT_VOLATILE)
			{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
		switch (ast_kind(a, n)) { MCC_TRACE("br\n");
		case AST_Store: { MCC_TRACE("br\n");
			if (!ast_purity_local_slot(a, ast_child(a, n, 0)))
				{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
			break;
		}
		case AST_Unary: { MCC_TRACE("br\n");
			int op = ast_op(a, n);
			if ((op == TOK_INC || op == TOK_DEC) &&
					ast_purity_local_slot(a, ast_first_child(a, n)))
				{ MCC_TRACE("br\n"); break; }
			if (!ast_purity_node(a, n, &has_load))
				{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
			break;
		}
		case AST_Ref: { MCC_TRACE("br\n");
			int r = ast_op(a, n);
			if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_LVAL) && !(r & VT_SYM))
				{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
			if (!ast_purity_node(a, n, &has_load))
				{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
			break;
		}
		default:
			if (!ast_purity_node(a, n, &has_load))
				{ MCC_TRACE("br\n"); return AST_PURITY_IMPURE; }
			break;
		}
	}
	return has_load ? AST_PURITY_TIER1 : AST_PURITY_TIER0;
}
#endif

static int ast_ident_same_scan(const AstArena *a, AstLocal x, AstLocal y) { MCC_TRACE("enter\n");
	if (a->kind[x] != a->kind[y] || a->op[x] != a->op[y] ||
			a->type_t[x] != a->type_t[y] || a->type_ref[x] != a->type_ref[y] ||
			a->type_bp[x] != a->type_bp[y] || a->type_bs[x] != a->type_bs[y] ||
			a->ival[x] != a->ival[y] || a->fbits[x] != a->fbits[y] ||
			a->sym[x] != a->sym[y] || a->nchild[x] != a->nchild[y])
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal cx = a->first_child[x], cy = a->first_child[y];
	for (; cx != AST_NONE; cx = a->next_sib[cx], cy = a->next_sib[cy])
		{ MCC_TRACE("br\n"); if (!ast_ident_same_scan(a, cx, cy))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static int ast_ident_same(const AstArena *a, AstLocal x, AstLocal y) { MCC_TRACE("enter\n");
	ast_hash_sync(a);
	if (x < (AstLocal)ast_hash_cap && y < (AstLocal)ast_hash_cap &&
			ast_hash_of(a, x) != ast_hash_of(a, y)) { MCC_TRACE("br\n");
#if MCC_DEV
		if (ast_ident_same_scan(a, x, y)) { MCC_TRACE("br\n");
			fprintf(stderr,
							"mcc: AST side-car divergence: ident_same(%u,%u) hash rejected "
							"but scan matched\n",
							(unsigned)x, (unsigned)y);
			abort();
		}
#endif
		return 0;
	}
	return ast_ident_same_scan(a, x, y);
}

static int ast_ident_cval(AstArena *a, AstLocal n, int *tt, uint64_t *v) { MCC_TRACE("enter\n");
	if (ast_kind(a, n) != AST_Literal)
		{ MCC_TRACE("br\n"); return 0; }
	if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	int t = ast_type_t(a, n);
	if (!ast_ident_intt(t))
		{ MCC_TRACE("br\n"); return 0; }
	*tt = t;
	*v = value64(ast_ival(a, n), t);
	return 1;
}

static int ast_ident_m1(uint64_t v, int ct) { MCC_TRACE("enter\n");
	if ((ct & VT_BTYPE) == VT_LLONG)
		{ MCC_TRACE("br\n"); return v == ~(uint64_t)0; }
	return (uint32_t)v == 0xffffffffu;
}

static AstLocal ast_ident_notof(AstArena *a, AstLocal n, int ct) { MCC_TRACE("enter\n");
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != '^' ||
			ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	AstLocal p = ast_child(a, n, 0), q = ast_child(a, n, 1);
	int tp, tq, lt;
	uint64_t rp, rq, lv;
	if (!ast_ident_etype(a, p, &tp, &rp) || !ast_ident_etype(a, q, &tq, &rq))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!ast_ident_intt(tp) || !ast_ident_intt(tq))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	int xct = ast_ident_common(tp, tq);
	if ((xct & (VT_BTYPE | VT_UNSIGNED)) != (ct & (VT_BTYPE | VT_UNSIGNED)))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (ast_ident_cval(a, q, &lt, &lv) && ast_ident_m1(lv, xct))
		{ MCC_TRACE("br\n"); return p; }
	if (ast_ident_cval(a, p, &lt, &lv) && ast_ident_m1(lv, xct))
		{ MCC_TRACE("br\n"); return q; }
	return AST_NONE;
}

static int ast_ident_iscompl(AstArena *a, AstLocal x, AstLocal y, int ct) { MCC_TRACE("enter\n");
	AstLocal w = ast_ident_notof(a, y, ct);
	if (w != AST_NONE && ast_ident_same(a, x, w) && ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 1; }
	w = ast_ident_notof(a, x, ct);
	if (w != AST_NONE && ast_ident_same(a, y, w) && ast_ident_pure(a, y))
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

static AstLocal ast_ident_lnotof(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != TOK_EQ ||
			ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	AstLocal p = ast_child(a, n, 0), q = ast_child(a, n, 1);
	int lt;
	uint64_t lv;
	if (ast_ident_cval(a, q, &lt, &lv) && lv == 0)
		{ MCC_TRACE("br\n"); return p; }
	if (ast_ident_cval(a, p, &lt, &lv) && lv == 0)
		{ MCC_TRACE("br\n"); return q; }
	return AST_NONE;
}

static int ast_ident_islnot(AstArena *a, AstLocal x, AstLocal y) { MCC_TRACE("enter\n");
	AstLocal w = ast_ident_lnotof(a, y);
	if (w != AST_NONE && ast_ident_same(a, x, w) && ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 1; }
	w = ast_ident_lnotof(a, x);
	if (w != AST_NONE && ast_ident_same(a, y, w) && ast_ident_pure(a, y))
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

static int ast_ident_cmpk(int op, int cct, uint64_t l1, uint64_t l2, int *ok) { MCC_TRACE("enter\n");
	int uns = (cct & VT_UNSIGNED) != 0;
	switch (op) { MCC_TRACE("br\n");
	case TOK_EQ:
		return l1 == l2;
	case TOK_NE:
		return l1 != l2;
	case TOK_ULT:
		*ok = uns;
		return l1 < l2;
	case TOK_UGE:
		*ok = uns;
		return l1 >= l2;
	case TOK_ULE:
		*ok = uns;
		return l1 <= l2;
	case TOK_UGT:
		*ok = uns;
		return l1 > l2;
	case TOK_LT:
		*ok = !uns;
		return gen_opic_lt(l1, l2);
	case TOK_GE:
		*ok = !uns;
		return !gen_opic_lt(l1, l2);
	case TOK_LE:
		*ok = !uns;
		return !gen_opic_lt(l2, l1);
	case TOK_GT:
		*ok = !uns;
		return gen_opic_lt(l2, l1);
	}
	*ok = 0;
	return 0;
}

static int ast_ident_leaf(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	uint16_t k = ast_kind(a, n);
	return k == AST_Literal || k == AST_Ref;
}

static void ast_ident_adopt(AstArena *a, AstLocal n, AstLocal x) { MCC_TRACE("enter\n");
	a->epoch++;
	a->kind[n] = a->kind[x];
	a->op[n] = a->op[x];
	a->type_t[n] = a->type_t[x];
	a->type_bp[n] = a->type_bp[x];
	a->type_bs[n] = a->type_bs[x];
	a->type_ref[n] = a->type_ref[x];
	a->ival[n] = a->ival[x];
	a->fbits[n] = a->fbits[x];
	a->sym[n] = a->sym[x];
	a->first_child[n] = a->first_child[x];
	a->last_child[n] = a->last_child[x];
	a->nchild[n] = a->nchild[x];
	for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c])
		{ MCC_TRACE("br\n"); a->parent[c] = n; }
	a->first_child[x] = AST_NONE;
	a->last_child[x] = AST_NONE;
	a->nchild[x] = 0;
}

static void ast_ident_setlit(AstArena *a, AstLocal n, int tt, uint64_t v) { MCC_TRACE("enter\n");
	ast_set_kind(a, n, AST_Literal);
	ast_clear_children(a, n);
	ast_set_op(a, n, VT_CONST | VT_NONCONST);
	ast_set_type(a, n, tt, 0);
	ast_set_ival(a, n, value64(v, tt));
	ast_set_sym(a, n, 0);
	ast_set_fbits(a, n, 0);
}

static int ast_ii_width(int tt) { MCC_TRACE("enter\n");
	switch (tt & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_BOOL:
	case VT_BYTE:
		return 1;
	case VT_SHORT:
		return 2;
	case VT_INT:
		return 4;
	case VT_LLONG:
		return 8;
	}
	return 0;
}

static int ast_ident_convert(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal c, x;
	int tt, ct, xt;
	if (ast_kind(a, n) != AST_Convert || ast_nchild(a, n) != 1)
		{ MCC_TRACE("br\n"); return 0; }
	tt = ast_type_t(a, n);
	if (!ast_ident_intt(tt) || (tt & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	c = ast_first_child(a, n);
	ct = ast_type_t(a, c);
	if (!ast_ident_intt(ct) || (ct & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	if ((tt & (VT_BTYPE | VT_UNSIGNED)) == (ct & (VT_BTYPE | VT_UNSIGNED))) { MCC_TRACE("br\n");
		ast_ident_adopt(a, n, c);
		return 2;
	}
	if (ast_kind(a, c) == AST_Convert && ast_nchild(a, c) == 1) { MCC_TRACE("br\n");
		x = ast_first_child(a, c);
		xt = ast_type_t(a, x);
		if (ast_ident_intt(xt) && !(xt & VT_VOLATILE) &&
				(tt & (VT_BTYPE | VT_UNSIGNED)) == (xt & (VT_BTYPE | VT_UNSIGNED)) &&
				(ct & VT_UNSIGNED) == (xt & VT_UNSIGNED) &&
				ast_ii_width(ct) >= ast_ii_width(tt)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, x);
			return 2;
		}
	}
	return 0;
}

static int ast_addr_nonnull(AstArena *a, AstLocal n, int depth) { MCC_TRACE("enter\n");
	int k, r, lt;
	uint64_t lv;
	AstLocal x, y;
	if (n == AST_NONE || depth > 8)
		{ MCC_TRACE("br\n"); return 0; }
	k = ast_kind(a, n);
	if (k == AST_Convert && ast_nchild(a, n) == 1)
		{ MCC_TRACE("br\n"); return ast_addr_nonnull(a, ast_first_child(a, n), depth + 1); }
	if (k == AST_Unary && ast_op(a, n) == AST_OP_ADDR && ast_nchild(a, n) == 1) { MCC_TRACE("br\n");
		AstLocal c = ast_first_child(a, n);
		if (ast_kind(a, c) != AST_Ref || ast_nchild(a, c))
			{ MCC_TRACE("br\n"); return 0; }
		r = ast_op(a, c);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM) || !(r & VT_LVAL))
			{ MCC_TRACE("br\n"); return 0; }
		return 1;
	}
	if (k == AST_Binary && ast_nchild(a, n) == 2 &&
			(ast_op(a, n) == '+' || ast_op(a, n) == '-')) { MCC_TRACE("br\n");
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		if (ast_ident_cval(a, y, &lt, &lv)) { MCC_TRACE("br\n"); }
		else if (ast_op(a, n) == '+' && ast_ident_cval(a, x, &lt, &lv))
			{ MCC_TRACE("br\n"); x = y; }
		else
			{ MCC_TRACE("br\n"); return 0; }
		if ((int64_t)value64(lv, lt) > ((int64_t)1 << 30) ||
				(int64_t)value64(lv, lt) < -((int64_t)1 << 30))
			{ MCC_TRACE("br\n"); return 0; }
		return ast_addr_nonnull(a, x, depth + 1);
	}
	if (k == AST_Ref && !ast_nchild(a, n)) { MCC_TRACE("br\n");
		Sym *s;
		r = ast_op(a, n);
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) != (VT_CONST | VT_SYM) ||
				!ast_sym(a, n))
			{ MCC_TRACE("br\n"); return 0; }
		s = (Sym *)(uintptr_t)ast_sym(a, n);
		if (s->a.weak)
			{ MCC_TRACE("br\n"); return 0; }
		if ((int64_t)ast_ival(a, n) < 0)
			{ MCC_TRACE("br\n"); return 0; }
		return 1;
	}
	return 0;
}

static int ast_addr_iszero(AstArena *a, AstLocal n, int depth) { MCC_TRACE("enter\n");
	if (n == AST_NONE || depth > 8)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Convert && ast_nchild(a, n) == 1)
		{ MCC_TRACE("br\n"); return ast_addr_iszero(a, ast_first_child(a, n), depth + 1); }
	if (ast_kind(a, n) != AST_Literal || ast_nchild(a, n))
		{ MCC_TRACE("br\n"); return 0; }
	if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	if (is_float(ast_type_t(a, n)))
		{ MCC_TRACE("br\n"); return 0; }
	return ast_ival(a, n) == 0;
}

static int ast_ident_addk(AstArena *a, AstLocal s, AstLocal e) { MCC_TRACE("enter\n");
	AstLocal p, q;
	int sop, lt, w;
	uint64_t lv;
	if (ast_kind(a, s) != AST_Binary || ast_nchild(a, s) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	sop = ast_op(a, s);
	if (sop != '+' && sop != '-')
		{ MCC_TRACE("br\n"); return 0; }
	p = ast_child(a, s, 0);
	q = ast_child(a, s, 1);
	if (ast_ident_cval(a, q, &lt, &lv)) { MCC_TRACE("br\n"); }
	else if (sop == '+' && ast_ident_cval(a, p, &lt, &lv))
		{ MCC_TRACE("br\n"); p = q; }
	else
		{ MCC_TRACE("br\n"); return 0; }
	{
		int st, et;
		uint64_t sref, eref;
		if (!ast_ident_etype(a, s, &st, &sref) || !ast_ident_etype(a, e, &et, &eref))
			{ MCC_TRACE("br\n"); return 0; }
		if (!ast_ident_intt(st) || !ast_ident_intt(et))
			{ MCC_TRACE("br\n"); return 0; }
		w = ast_ii_width(st);
		if (w <= 0 || w > 8 || w != ast_ii_width(et))
			{ MCC_TRACE("br\n"); return 0; }
	}
	if (w < 8)
		{ MCC_TRACE("br\n"); lv &= ((uint64_t)1 << (w * 8)) - 1; }
	if (!lv)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_ident_same(a, p, e) && ast_ident_pure(a, p);
}

static int ast_ident_exact_t(int t1, int t2) { MCC_TRACE("enter\n");
	return ast_ident_intt(t1) && ast_ident_intt(t2) &&
				 (t1 & (VT_BTYPE | VT_UNSIGNED)) == (t2 & (VT_BTYPE | VT_UNSIGNED));
}

static AstLocal ast_ident_muldivk(AstArena *a, AstLocal n, AstLocal x, AstLocal y) { MCC_TRACE("enter\n");
	int nt, xt, ot, lt, mt;
	uint64_t nref, xref, oref, lv, mv;
	AstLocal p, q, other;
	if (!ast_ident_cval(a, y, &lt, &lv) || lv == 0 || lv == 1)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (ast_kind(a, x) != AST_Binary || ast_op(a, x) != '*' || ast_nchild(a, x) != 2)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_etype(a, x, &xt, &xref))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if ((nt & VT_UNSIGNED) || !ast_ident_exact_t(nt, xt) || !ast_ident_exact_t(nt, lt))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (ast_ii_width(nt) < 4)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	p = ast_child(a, x, 0);
	q = ast_child(a, x, 1);
	if (ast_ident_cval(a, q, &mt, &mv) && mv == lv && ast_ident_exact_t(nt, mt))
		{ MCC_TRACE("br\n"); other = p; }
	else if (ast_ident_cval(a, p, &mt, &mv) && mv == lv && ast_ident_exact_t(nt, mt))
		{ MCC_TRACE("br\n"); other = q; }
	else
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!ast_ident_etype(a, other, &ot, &oref) || !ast_ident_exact_t(nt, ot))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	return other;
}

static int ast_ident_node(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (ast_ident_conv_env) { MCC_TRACE("br\n");
		int cr = ast_ident_convert(a, n);
		if (cr)
			{ MCC_TRACE("br\n"); return cr; }
	}
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	int op = ast_op(a, n);
	AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
	int tx, ty;
	uint64_t rx, ry;
	int lt;
	uint64_t lv;
	if (ast_ident_rel_env && (op == TOK_EQ || op == TOK_NE) &&
			((ast_addr_nonnull(a, x, 0) && ast_addr_iszero(a, y, 0)) ||
			 (ast_addr_nonnull(a, y, 0) && ast_addr_iszero(a, x, 0)))) { MCC_TRACE("br\n");
		ast_ident_setlit(a, n, VT_INT, op == TOK_NE ? 1u : 0u);
		return 2;
	}
	if (!ast_ident_etype(a, x, &tx, &rx) || !ast_ident_etype(a, y, &ty, &ry))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_intt(tx) || !ast_ident_intt(ty))
		{ MCC_TRACE("br\n"); return 0; }
	int ct = ast_ident_common(tx, ty);
	switch (op) { MCC_TRACE("br\n");
	case TOK_SHL:
	case TOK_SHR:
	case TOK_SAR:
		if (ast_ident_shift_env && ast_ident_cval(a, y, &lt, &lv) && lv == 0) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, x);
			return 1;
		}
		return 0;
	case '+':
		if (!ast_ident_arith_env)
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 0 && ast_ident_keep(lt, tx)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, x);
			return 1;
		}
		if (ast_ident_cval(a, x, &lt, &lv) && lv == 0 && ast_ident_keep(lt, ty)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, y);
			return 1;
		}
		return 0;
	case '-':
		if (!ast_ident_arith_env)
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 0 && ast_ident_keep(lt, tx)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, x);
			return 1;
		}
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, ct, 0);
			return 2;
		}
		return 0;
	case '/':
		if (!ast_ident_arith_env)
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 1 && ast_ident_keep(lt, tx)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, x);
			return 1;
		}
		if (ast_strict_overflow_env) { MCC_TRACE("br\n");
			AstLocal q = ast_ident_muldivk(a, n, x, y);
			if (q != AST_NONE) { MCC_TRACE("br\n");
				ast_ident_adopt(a, n, q);
				return 2;
			}
		}
		return 0;
	case '*':
		if (!ast_ident_arith_env)
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_ident_cval(a, y, &lt, &lv)) { MCC_TRACE("br\n");
			if (lv == 1 && ast_ident_keep(lt, tx)) { MCC_TRACE("br\n");
				ast_ident_adopt(a, n, x);
				return 1;
			}
			if (lv == 0 && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
				int sig = ast_ident_leaf(a, x) ? 1 : 2;
				ast_ident_setlit(a, n, ct, 0);
				return sig;
			}
		}
		if (ast_ident_cval(a, x, &lt, &lv)) { MCC_TRACE("br\n");
			if (lv == 1 && ast_ident_keep(lt, ty)) { MCC_TRACE("br\n");
				ast_ident_adopt(a, n, y);
				return 1;
			}
			if (lv == 0 && ast_ident_pure(a, y)) { MCC_TRACE("br\n");
				int sig = ast_ident_leaf(a, y) ? 1 : 2;
				ast_ident_setlit(a, n, ct, 0);
				return sig;
			}
		}
		return 0;
	case '&':
		if (!ast_ident_bit_env)
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, x);
			return 2;
		}
		if (ast_ident_iscompl(a, x, y, ct)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, ct, 0);
			return 2;
		}
		if (ast_ident_cval(a, y, &lt, &lv)) { MCC_TRACE("br\n");
			if (ast_ident_m1(lv, ct) && ast_ident_keep(lt, tx)) { MCC_TRACE("br\n");
				ast_ident_adopt(a, n, x);
				return 1;
			}
			if (lv == 0 && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
				int sig = ast_ident_leaf(a, x) ? 1 : 2;
				ast_ident_setlit(a, n, ct, 0);
				return sig;
			}
		}
		if (ast_ident_cval(a, x, &lt, &lv)) { MCC_TRACE("br\n");
			if (ast_ident_m1(lv, ct) && ast_ident_keep(lt, ty)) { MCC_TRACE("br\n");
				ast_ident_adopt(a, n, y);
				return 1;
			}
			if (lv == 0 && ast_ident_pure(a, y)) { MCC_TRACE("br\n");
				int sig = ast_ident_leaf(a, y) ? 1 : 2;
				ast_ident_setlit(a, n, ct, 0);
				return sig;
			}
		}
		return 0;
	case '|':
		if (!ast_ident_bit_env)
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, x);
			return 2;
		}
		if (ast_ident_iscompl(a, x, y, ct)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, ct, ~(uint64_t)0);
			return 2;
		}
		if (ast_ident_cval(a, y, &lt, &lv)) { MCC_TRACE("br\n");
			if (lv == 0 && ast_ident_keep(lt, tx)) { MCC_TRACE("br\n");
				ast_ident_adopt(a, n, x);
				return 1;
			}
			if (ast_ident_m1(lv, ct) && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
				int sig = ast_ident_leaf(a, x) ? 1 : 2;
				ast_ident_setlit(a, n, ct, ~(uint64_t)0);
				return sig;
			}
		}
		if (ast_ident_cval(a, x, &lt, &lv)) { MCC_TRACE("br\n");
			if (lv == 0 && ast_ident_keep(lt, ty)) { MCC_TRACE("br\n");
				ast_ident_adopt(a, n, y);
				return 1;
			}
			if (ast_ident_m1(lv, ct) && ast_ident_pure(a, y)) { MCC_TRACE("br\n");
				int sig = ast_ident_leaf(a, y) ? 1 : 2;
				ast_ident_setlit(a, n, ct, ~(uint64_t)0);
				return sig;
			}
		}
		return 0;
	case '^':
		if (!ast_ident_bit_env)
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, ct, 0);
			return 2;
		}
		if (ast_ident_iscompl(a, x, y, ct)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, ct, ~(uint64_t)0);
			return 2;
		}
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 0 && ast_ident_keep(lt, tx)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, x);
			return 1;
		}
		if (ast_ident_cval(a, x, &lt, &lv) && lv == 0 && ast_ident_keep(lt, ty)) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, y);
			return 1;
		}
		return 0;
	case TOK_EQ:
	case TOK_NE:
	case TOK_LT:
	case TOK_GT:
	case TOK_LE:
	case TOK_GE:
	case TOK_ULT:
	case TOK_UGT:
	case TOK_ULE:
	case TOK_UGE:
		if (ast_ident_rel_env) { MCC_TRACE("br\n");
			int rt, cok = 1;
			uint64_t rv;
			if (ast_ident_cval(a, x, &lt, &lv) && ast_ident_cval(a, y, &rt, &rv)) { MCC_TRACE("br\n");
				int cct = ast_ident_common(lt, rt);
				int res = ast_ident_cmpk(op, cct, value64(lv, cct), value64(rv, cct),
																 &cok);
				if (cok) { MCC_TRACE("br\n");
					ast_ident_setlit(a, n, VT_INT, (uint64_t)res);
					return 2;
				}
			}
		}
		if (ast_ident_rel_env && ast_ident_same(a, x, y) && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
			int one = (op == TOK_EQ || op == TOK_LE || op == TOK_GE || op == TOK_ULE ||
								 op == TOK_UGE);
			ast_ident_setlit(a, n, VT_INT, one ? 1u : 0u);
			return 2;
		}
		if (ast_ident_urange_env && op == TOK_GE && (tx & VT_UNSIGNED) &&
				ast_ident_cval(a, y, &lt, &lv) && lv == 0 && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, VT_INT, 1u);
			return 2;
		}
		if (ast_ident_urange_env && op == TOK_LT && (tx & VT_UNSIGNED) &&
				ast_ident_cval(a, y, &lt, &lv) && lv == 0 && ast_ident_pure(a, x)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, VT_INT, 0u);
			return 2;
		}
		if (ast_ident_urange_env && op == TOK_LE && (ty & VT_UNSIGNED) &&
				ast_ident_cval(a, x, &lt, &lv) && lv == 0 && ast_ident_pure(a, y)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, VT_INT, 1u);
			return 2;
		}
		if (ast_ident_urange_env && op == TOK_GT && (ty & VT_UNSIGNED) &&
				ast_ident_cval(a, x, &lt, &lv) && lv == 0 && ast_ident_pure(a, y)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, VT_INT, 0u);
			return 2;
		}
		if (ast_ident_rel_env && (op == TOK_EQ || op == TOK_NE) &&
				(ast_ident_addk(a, x, y) || ast_ident_addk(a, y, x))) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, VT_INT, op == TOK_NE ? 1u : 0u);
			return 2;
		}
		return 0;
	case TOK_LAND:
	case TOK_LOR: { MCC_TRACE("br\n");
		if (!ast_ident_rel_env)
			{ MCC_TRACE("br\n"); return 0; }
		int rt;
		int inv = (ast_fbits(a, n) & AST_FB_LANDOR_INVERT) ? 1 : 0;
		uint64_t rv;
		if (ast_ident_cval(a, x, &lt, &lv) && ast_ident_cval(a, y, &rt, &rv)) { MCC_TRACE("br\n");
			int res = (op == TOK_LAND) ? (lv != 0 && rv != 0) : (lv != 0 || rv != 0);
			ast_ident_setlit(a, n, VT_INT, (uint64_t)(res ^ inv));
			return 2;
		}
		if (ast_ident_islnot(a, x, y)) { MCC_TRACE("br\n");
			ast_ident_setlit(a, n, VT_INT, (uint64_t)((op == TOK_LAND ? 0 : 1) ^ inv));
			return 2;
		}
		return 0;
	}
	}
	return 0;
}

static int ast_ident_folds;

static int ast_ident_rec(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int sig = 0;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); sig += ast_ident_rec(a, c); }
	int r = ast_ident_node(a, n);
	if (r) { MCC_TRACE("br\n");
		ast_ident_folds++;
		if (r == 2)
			{ MCC_TRACE("br\n"); sig++; }
	}
	return sig;
}

static int ast_ident_run(AstArena *a) { MCC_TRACE("enter\n");
	int sig = 0, folds;
	ast_ident_folds = 0;
	do { MCC_TRACE("br\n");
		folds = ast_ident_folds;
		sig += ast_ident_rec(a, ast_root(a));
	} while (ast_ident_folds != folds);
	return sig;
}

static int ast_narrow_bs(int t) { MCC_TRACE("enter\n");
	return t & (VT_BTYPE | VT_UNSIGNED);
}

static int ast_narrow_class(AstArena *a, AstLocal op, int tt) { MCC_TRACE("enter\n");
	int ot;
	uint64_t oref;
	if (!ast_ident_etype(a, op, &ot, &oref) || !ast_ident_intt(ot) ||
			(ot & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return -1; }
	if (ast_narrow_bs(ot) == ast_narrow_bs(tt))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, op) == AST_Convert && ast_nchild(a, op) == 1) { MCC_TRACE("br\n");
		AstLocal inner = ast_first_child(a, op);
		int it;
		uint64_t iref;
		if (ast_ident_etype(a, inner, &it, &iref) && ast_ident_intt(it) &&
				!(it & VT_VOLATILE) && ast_narrow_bs(it) == ast_narrow_bs(tt) &&
				(ot & VT_UNSIGNED) == (it & VT_UNSIGNED) &&
				ast_ii_width(ot) >= ast_ii_width(tt))
			{ MCC_TRACE("br\n"); return 1; }
	}
	{
		int lt;
		uint64_t lv;
		if (ast_ident_cval(a, op, &lt, &lv))
			{ MCC_TRACE("br\n"); return 2; }
	}
	return 3;
}

static AstLocal ast_narrow_make(AstArena *a, AstLocal op, int tt, int cls) { MCC_TRACE("enter\n");
	switch (cls) { MCC_TRACE("br\n");
	case 1: {
		AstLocal inner = ast_first_child(a, op);
		ast_clear_children(a, op);
		ast_set_kind(a, op, AST_Poison);
		return inner;
	}
	case 2: {
		int lt;
		uint64_t lv;
		ast_ident_cval(a, op, &lt, &lv);
		ast_set_type(a, op, tt, 0);
		ast_set_ival(a, op, value64(lv, tt));
		return op;
	}
	case 3: {
		AstLocal cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, tt, 0);
		ast_add_child(a, cvt, op);
		return cvt;
	}
	}
	return op;
}

static int ast_narrow_binop(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case '+':
	case '-':
	case '*':
	case '&':
	case '|':
	case '^':
		return 1;
	}
	return 0;
}

static int ast_narrow_binop_ranged(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case '/':
	case '%':
	case TOK_SHL:
	case TOK_SAR:
	case TOK_SHR:
		return 1;
	}
	return 0;
}

static int ast_narrow_operand_off(AstArena *a, AstLocal op, int *off) { MCC_TRACE("enter\n");
	int r;
	if (ast_kind(a, op) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	r = ast_op(a, op);
	if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	*off = (int)(int64_t)ast_ival(a, op);
	return 1;
}

static int ast_ii_cval_fits(uint64_t lv, int tt) { MCC_TRACE("enter\n");
	int w = ast_ii_width(tt);
	uint64_t m, tr;
	if ((tt & VT_BTYPE) == VT_BOOL)
		{ MCC_TRACE("br\n"); return lv == 0 || lv == 1; }
	if (w <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (w >= 8)
		{ MCC_TRACE("br\n"); return 1; }
	m = ((uint64_t)1 << (w * 8)) - 1;
	tr = lv & m;
	if (!(tt & VT_UNSIGNED) && (tr & ((uint64_t)1 << (w * 8 - 1))))
		{ MCC_TRACE("br\n"); tr |= ~m; }
	return tr == lv;
}

static int ast_narrow_fits(AstArena *a, AstLocal op, int cls, int tt) { MCC_TRACE("enter\n");
	int lt, w;
	uint64_t lv, umax;
	int64_t smax;
	AstVLat ctx;
	if (cls == 0 || cls == 1)
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_ident_cval(a, op, &lt, &lv))
		{ MCC_TRACE("br\n"); return ast_ii_cval_fits(lv, tt); }
	if (!ast_vlat_context_at(a, op, &ctx))
		{ MCC_TRACE("br\n"); return 0; }
	w = ast_ii_width(tt);
	if (w <= 0 || w >= 8)
		{ MCC_TRACE("br\n"); return 0; }
	umax = ((uint64_t)1 << (w * 8)) - 1;
	if (tt & VT_UNSIGNED)
		{ MCC_TRACE("br\n"); return ctx.lo >= 0 && (uint64_t)ctx.hi <= umax; }
	smax = (int64_t)(umax >> 1);
	return ctx.lo >= -smax - 1 && ctx.hi <= smax;
}

static int ast_narrow_ranged_ok(AstArena *a, int op, int tt, int wt, AstLocal op0,
																AstLocal op1, int c0, int c1) { MCC_TRACE("enter\n");
	int lt;
	uint64_t lv;
	switch (op) { MCC_TRACE("br\n");
	case '/':
	case '%':
		if (!(tt & VT_UNSIGNED) || !(wt & VT_UNSIGNED))
			{ MCC_TRACE("br\n"); return 0; }
		return ast_narrow_fits(a, op0, c0, tt) && ast_narrow_fits(a, op1, c1, tt);
	case TOK_SHL:
		(void)c0;
		(void)c1;
		if (!ast_ident_cval(a, op1, &lt, &lv))
			{ MCC_TRACE("br\n"); return 0; }
		return (int64_t)lv >= 0 && (int64_t)lv <= 31;
	case TOK_SAR:
		if (!ast_ident_cval(a, op1, &lt, &lv))
			{ MCC_TRACE("br\n"); return 0; }
		if ((int64_t)lv < 0 || (int64_t)lv > 31)
			{ MCC_TRACE("br\n"); return 0; }
		return ast_narrow_fits(a, op0, c0, tt);
	case TOK_SHR:
		if (!(tt & VT_UNSIGNED))
			{ MCC_TRACE("br\n"); return 0; }
		if (!ast_ident_cval(a, op1, &lt, &lv))
			{ MCC_TRACE("br\n"); return 0; }
		if ((int64_t)lv < 0 || (int64_t)lv > 31)
			{ MCC_TRACE("br\n"); return 0; }
		return ast_narrow_fits(a, op0, c0, tt);
	}
	return 0;
}

static int ast_narrow_class_gate(int cls) { MCC_TRACE("enter\n");
	switch (cls) { MCC_TRACE("br\n");
	case 0:
		return ast_narrow_c0_env;
	case 1:
		return ast_narrow_c1_env;
	case 2:
		return ast_narrow_c2_env;
	default:
		return ast_narrow_c3_env;
	}
}

static int ast_narrow_binary(AstArena *a, AstLocal bin, int tt) { MCC_TRACE("enter\n");
	if (ast_kind(a, bin) != AST_Binary || ast_nchild(a, bin) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_intt(tt) || (tt & VT_VOLATILE) || ast_ii_width(tt) < 4)
		{ MCC_TRACE("br\n"); return 0; }
	int op = ast_op(a, bin);
	int distributive = ast_narrow_binop(op);
	int ranged = ast_vlat_env && !distributive && ast_narrow_binop_ranged(op);
	if (!distributive && !ranged)
		{ MCC_TRACE("br\n"); return 0; }
	int wt;
	uint64_t wref;
	if (!ast_ident_etype(a, bin, &wt, &wref) || !ast_ident_intt(wt) ||
			(wt & VT_VOLATILE) || ast_ii_width(wt) <= ast_ii_width(tt))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal op0 = ast_child(a, bin, 0), op1 = ast_child(a, bin, 1);
	int c0 = ast_narrow_class(a, op0, tt);
	int c1 = ast_narrow_class(a, op1, tt);
	if (c0 < 0 || c1 < 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_narrow_class_gate(c0) || !ast_narrow_class_gate(c1))
		{ MCC_TRACE("br\n"); return 0; }
	if (distributive) { MCC_TRACE("br\n");
		if (c0 != 0 && c0 != 1 && c1 != 0 && c1 != 1)
			{ MCC_TRACE("br\n"); return 0; }
	} else { MCC_TRACE("br\n");
		if (!ast_narrow_ranged_ok(a, op, tt, wt, op0, op1, c0, c1))
			{ MCC_TRACE("br\n"); return 0; }
		MCC_TRACE("narrow ranged op=%d tt=%d wt=%d\n", op, tt, wt);
	}
	AstLocal na = ast_narrow_make(a, op0, tt, c0);
	AstLocal nb = ast_narrow_make(a, op1, tt, c1);
	ast_set_type(a, bin, tt, 0);
	ast_clear_children(a, bin);
	ast_add_child(a, bin, na);
	ast_add_child(a, bin, nb);
	return 1;
}

static int ast_vlat_in_type(const AstVLat *v, int tt) { MCC_TRACE("enter\n");
	int w = ast_ii_width(tt);
	uint64_t umax;
	int64_t smax, smin;
	if (v->state != AST_VLAT_FACT || w <= 0 || w >= 8)
		{ MCC_TRACE("br\n"); return 0; }
	umax = ((uint64_t)1 << (w * 8)) - 1;
	if (tt & VT_UNSIGNED)
		{ MCC_TRACE("br\n"); return v->lo >= 0 && (uint64_t)v->hi <= umax; }
	smax = (int64_t)(umax >> 1);
	smin = -smax - 1;
	return v->lo >= smin && v->hi <= smax;
}

static int ast_narrow_elim_srcrange(AstArena *a, AstLocal c, AstVLat *out) { MCC_TRACE("enter\n");
	int off, ct, it;
	uint64_t cref, iref;
	AstLocal inner;
	if (ast_narrow_operand_off(a, c, &off))
		{ MCC_TRACE("br\n"); return ast_vlat_context_at(a, c, out); }
	if (ast_kind(a, c) != AST_Convert || ast_nchild(a, c) != 1)
		{ MCC_TRACE("br\n"); return 0; }
	inner = ast_first_child(a, c);
	if (!ast_narrow_operand_off(a, inner, &off))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, c, &ct, &cref) || !ast_ident_intt(ct) ||
			(ct & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, inner, &it, &iref) || !ast_ident_intt(it) ||
			(it & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_ii_width(it) >= ast_ii_width(ct))
		{ MCC_TRACE("br\n"); return 0; }
	if (!(it & VT_UNSIGNED) && (ct & VT_UNSIGNED))
		{ MCC_TRACE("br\n"); return 0; }
	return ast_vlat_context_at(a, inner, out);
}

static int ast_narrow_elim_fits(AstArena *a, AstLocal c, int tt) { MCC_TRACE("enter\n");
	int lt;
	uint64_t lv;
	AstVLat ctx;
	if (ast_ii_width(tt) < 1 || ast_ii_width(tt) >= 8)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_ident_cval(a, c, &lt, &lv))
		{ MCC_TRACE("br\n"); return ast_ii_cval_fits(lv, tt); }
	if (!ast_narrow_elim_srcrange(a, c, &ctx))
		{ MCC_TRACE("br\n"); return 0; }
	return ast_vlat_in_type(&ctx, tt);
}

static int ast_narrow_elim(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal c;
	int tt, ct;
	if (!ast_narrow_elim_env)
		{ MCC_TRACE("br\n"); return 0; }
	tt = ast_type_t(a, n);
	if (!ast_ident_intt(tt) || (tt & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	c = ast_first_child(a, n);
	ct = ast_type_t(a, c);
	if (!ast_ident_intt(ct) || (ct & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_ii_width(tt) < 1 || ast_ii_width(ct) > 4 ||
			ast_ii_width(ct) <= ast_ii_width(tt))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_ii_width(ct) >= 4 && (ct & VT_UNSIGNED))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_narrow_elim_fits(a, c, tt))
		{ MCC_TRACE("br\n"); return 0; }
	MCC_TRACE("narrow elim ct=%d tt=%d\n", ct, tt);
	ast_ident_adopt(a, n, c);
	return 1;
}

static int ast_narrow_node(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	uint16_t k = ast_kind(a, n);
	if (k == AST_Convert && ast_nchild(a, n) == 1) { MCC_TRACE("br\n");
		AstLocal bin = ast_first_child(a, n);
		if (ast_narrow_binary(a, bin, ast_type_t(a, n))) { MCC_TRACE("br\n");
			ast_ident_adopt(a, n, bin);
			return 1;
		}
		return ast_narrow_elim(a, n);
	}
	if (k == AST_Store && ast_nchild(a, n) == 2) { MCC_TRACE("br\n");
		AstLocal lval = ast_child(a, n, 0), rval = ast_child(a, n, 1);
		int tt;
		uint64_t tref;
		if (!ast_ident_etype(a, lval, &tt, &tref))
			{ MCC_TRACE("br\n"); return 0; }
		return ast_narrow_binary(a, rval, tt);
	}
	return 0;
}

static int ast_narrow_rec(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int sig = 0;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); sig += ast_narrow_rec(a, c); }
	sig += ast_narrow_node(a, n);
	return sig;
}

static int ast_narrow_run(AstArena *a) { MCC_TRACE("enter\n");
	int total = ast_narrow_rec(a, ast_root(a));
	if (ast_narrow_fix_env) { MCC_TRACE("br\n");
		int sig;
		do { MCC_TRACE("br\n");
			sig = ast_narrow_rec(a, ast_root(a));
			total += sig;
		} while (sig);
	}
	return total;
}

static int ast_cprop_koff[AST_CPROP_MAX];
static int ast_cprop_ktt[AST_CPROP_MAX];
static uint64_t ast_cprop_kval[AST_CPROP_MAX];
static int ast_cprop_kn;
static int ast_cprop_folds;

static int ast_cprop_is_local(AstArena *a, AstLocal n, int *off, int *tt) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	int r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	int t = ast_type_t(a, n);
	if (!ast_ident_intt(t) || (t & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	*off = (int)(int64_t)ast_ival(a, n);
	*tt = t;
	return 1;
}

static int ast_cprop_escapes_scan(AstArena *a, int off) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Unary)
			{ MCC_TRACE("br\n"); continue; }
		int op = ast_op(a, n);
		if (op != AST_OP_ADDR && op != AST_OP_MEMBER && op != AST_OP_MEMBER_ARROW)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		int r = ast_op(a, c);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM) &&
				(int)(int64_t)ast_ival(a, c) == off)
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int ast_cprop_escapes(AstArena *a, int off) { MCC_TRACE("enter\n");
	int r;
	ast_du_sync(a);
	if (ast_du_state < 0)
		{ MCC_TRACE("br\n"); r = ast_cprop_escapes_scan(a, off); }
	else
		{ MCC_TRACE("br\n"); r = (ast_du_slot_flags(a, off) & AST_DU_ESCAPED) ? 1 : 0; }
#if MCC_DEV
	if (ast_du_verify()) { MCC_TRACE("br\n");
		int s = ast_cprop_escapes_scan(a, off);
		if (r != s)
			{ MCC_TRACE("br\n"); ast_du_diverge("escapes", off, r, s); }
	}
#endif
	return r;
}

static int ast_cprop_safe_compute(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (ast_type_t(a, n) & VT_VOLATILE)
		{ MCC_TRACE("br\n"); return 0; }
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_Literal:
	case AST_Ref:
	case AST_Load:
	case AST_Convert:
	case AST_Binary:
		break;
	case AST_Unary:
		switch (ast_op(a, n)) { MCC_TRACE("br\n");
		case AST_OP_ADDR:
		case AST_OP_MEMBER:
		case AST_OP_MEMBER_ARROW:
			break;
		default:
			return 0;
		}
		break;
	case AST_If:
		if (ast_op(a, n) != 5)
			{ MCC_TRACE("br\n"); return 0; }
		break;
	default:
		return 0;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_cprop_safe(a, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static int ast_cprop_find(int off) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_cprop_kn; i++)
		{ MCC_TRACE("br\n"); if (ast_cprop_koff[i] == off)
			{ MCC_TRACE("br\n"); return i; } }
	return -1;
}

static void ast_cprop_kill(int off) { MCC_TRACE("enter\n");
	int i = ast_cprop_find(off);
	if (i < 0)
		{ MCC_TRACE("br\n"); return; }
	ast_cprop_kn--;
	ast_cprop_koff[i] = ast_cprop_koff[ast_cprop_kn];
	ast_cprop_ktt[i] = ast_cprop_ktt[ast_cprop_kn];
	ast_cprop_kval[i] = ast_cprop_kval[ast_cprop_kn];
}

static void ast_cprop_gen(int off, int tt, uint64_t v) { MCC_TRACE("enter\n");
	int i = ast_cprop_find(off);
	if (i < 0) { MCC_TRACE("br\n");
		if (ast_cprop_kn >= ast_cprop_window)
			{ MCC_TRACE("br\n"); return; }
		i = ast_cprop_kn++;
		ast_cprop_koff[i] = off;
	}
	ast_cprop_ktt[i] = tt;
	ast_cprop_kval[i] = v;
}

static int ast_cprop_tt_same(int lt, int tt) { MCC_TRACE("enter\n");
	return (lt & ~VT_CONSTANT) == (tt & ~VT_CONSTANT);
}

static int ast_cprop_lval_op(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case AST_OP_ADDR:
	case AST_OP_MEMBER:
	case AST_OP_MEMBER_ARROW:
	case TOK_INC:
	case TOK_DEC:
		return 1;
	default:
		return 0;
	}
}

static void ast_cprop_rewrite(AstArena *a, AstLocal n, int lval) { MCC_TRACE("enter\n");
	if (n == AST_NONE || !ast_cprop_kn)
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	if (k == AST_Ref && !lval) { MCC_TRACE("br\n");
		int off, tt;
		if (ast_cprop_is_local(a, n, &off, &tt)) { MCC_TRACE("br\n");
			int i = ast_cprop_find(off);
			if (i >= 0 && ast_cprop_tt_same(ast_cprop_ktt[i], tt)) { MCC_TRACE("br\n");
				ast_ident_setlit(a, n, tt, ast_cprop_kval[i]);
				ast_cprop_folds++;
			}
		}
		return;
	}
	if (k == AST_Store) { MCC_TRACE("br\n");
		ast_cprop_rewrite(a, ast_child(a, n, 0), 1);
		ast_cprop_rewrite(a, ast_child(a, n, 1), 0);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_cprop_rewrite(a, c, clval); }
}

static void ast_cprop_block(AstArena *a, AstLocal bb) { MCC_TRACE("enter\n");
	ast_cprop_kn = 0;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, s);
		if (k == AST_Store) { MCC_TRACE("br\n");
			AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
			if (!ast_cprop_safe(a, lval) || !ast_cprop_safe(a, val)) { MCC_TRACE("br\n");
				ast_cprop_kn = 0;
				continue;
			}
			ast_cprop_rewrite(a, val, 0);
			ast_cprop_rewrite(a, lval, 1);
			int off, tt;
			if (ast_cprop_is_local(a, lval, &off, &tt) && !ast_cprop_escapes(a, off)) { MCC_TRACE("br\n");
				int lt;
				uint64_t lv;
				if (ast_ident_cval(a, val, &lt, &lv) && ast_cprop_tt_same(lt, tt))
					{ MCC_TRACE("br\n"); ast_cprop_gen(off, tt, lv); }
				else
					{ MCC_TRACE("br\n"); ast_cprop_kill(off); }
			}
		} else if (k == AST_Return) { MCC_TRACE("br\n");
			AstLocal rv = ast_first_child(a, s);
			if (rv != AST_NONE && ast_cprop_safe(a, rv))
				{ MCC_TRACE("br\n"); ast_cprop_rewrite(a, rv, 0); }
			ast_cprop_kn = 0;
		} else if (k == AST_If && ast_op(a, s) == 0) { MCC_TRACE("br\n");
			AstLocal cond = ast_child(a, s, 0);
			if (ast_cprop_safe(a, cond))
				{ MCC_TRACE("br\n"); ast_cprop_rewrite(a, cond, 0); }
			ast_cprop_kn = 0;
		} else { MCC_TRACE("br\n");
			ast_cprop_kn = 0;
		}
	}
}

typedef struct {
	int koff[AST_CPROP_MAX];
	int ktt[AST_CPROP_MAX];
	uint64_t kval[AST_CPROP_MAX];
	int kn;
} AstCpropState;

static void ast_cprop_state_save(AstCpropState *st) { MCC_TRACE("enter\n");
	st->kn = ast_cprop_kn;
	memcpy(st->koff, ast_cprop_koff, (size_t)ast_cprop_kn * sizeof(int));
	memcpy(st->ktt, ast_cprop_ktt, (size_t)ast_cprop_kn * sizeof(int));
	memcpy(st->kval, ast_cprop_kval, (size_t)ast_cprop_kn * sizeof(uint64_t));
}

static void ast_cprop_state_load(const AstCpropState *st) { MCC_TRACE("enter\n");
	ast_cprop_kn = st->kn;
	memcpy(ast_cprop_koff, st->koff, (size_t)st->kn * sizeof(int));
	memcpy(ast_cprop_ktt, st->ktt, (size_t)st->kn * sizeof(int));
	memcpy(ast_cprop_kval, st->kval, (size_t)st->kn * sizeof(uint64_t));
}

static void ast_cprop_state_meet(const AstCpropState *st) { MCC_TRACE("enter\n");
	int i = 0;
	while (i < ast_cprop_kn) { MCC_TRACE("br\n");
		int j, keep = 0;
		for (j = 0; j < st->kn; j++)
			{ MCC_TRACE("br\n"); if (st->koff[j] == ast_cprop_koff[i]) { MCC_TRACE("br\n");
				keep = st->ktt[j] == ast_cprop_ktt[i] &&
							 st->kval[j] == ast_cprop_kval[i];
				break;
			} }
		if (keep)
			{ MCC_TRACE("br\n"); i++; }
		else
			{ MCC_TRACE("br\n"); ast_cprop_kill(ast_cprop_koff[i]); }
	}
}

static int ast_cprop_opaque(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Jump)
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_node_is_asm(a, n))
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_cprop_opaque(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static unsigned char *ast_cprop_vis;
static int ast_licm_written(AstArena *a, AstLocal n, int off);
static int ast_sccp_has_label(AstArena *a, AstLocal n);

static void ast_cprop_stmt(AstArena *a, AstLocal s);

static int ast_cprop_cond_writes(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_StoreVal) { MCC_TRACE("br\n");
		AstLocal st = (AstLocal)ast_ival(a, n);
		if (st == AST_NONE || st >= ast_count(a) || ast_kind(a, st) != AST_Store)
			{ MCC_TRACE("br\n"); return 1; }
		if (ast_licm_written(a, st, off))
			{ MCC_TRACE("br\n"); return 1; }
	}
	if (ast_licm_written(a, n, off))
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_cprop_cond_writes(a, c, off))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static void ast_cprop_cond_kill(AstArena *a, AstLocal cond) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_cprop_kn;)
		{ MCC_TRACE("br\n"); if (ast_cprop_escapes(a, ast_cprop_koff[i]) ||
				ast_cprop_cond_writes(a, cond, ast_cprop_koff[i]))
			{ MCC_TRACE("br\n"); ast_cprop_kill(ast_cprop_koff[i]); }
		else
			{ MCC_TRACE("br\n"); i++; } }
}

static void ast_cprop_stmts(AstArena *a, AstLocal bb) { MCC_TRACE("enter\n");
	if (bb == AST_NONE || ast_kind(a, bb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return; }
	ast_cprop_vis[bb] = 1;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s))
		{ MCC_TRACE("br\n"); ast_cprop_stmt(a, s); }
}

static int ast_cprop_arm_clean(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 1; }
	uint16_t k = ast_kind(a, n);
	if (k == AST_Return || k == AST_Jump)
		{ MCC_TRACE("br\n"); return 0; }
	if (k == AST_If && ((ast_op(a, n) >= 2 && ast_op(a, n) <= 6) || ast_op(a, n) == 8))
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_cprop_arm_clean(a, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

#define AST_CPROP_SWMAX 64

static int ast_cprop_switch_meet(AstArena *a, AstLocal sw) { MCC_TRACE("enter\n");
	AstLocal val = ast_child(a, sw, 0);
	AstLocal body = ast_child(a, sw, 1);
	if (body == AST_NONE || ast_kind(a, body) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	if (val == AST_NONE || !ast_cprop_safe(a, val))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_sccp_has_label(a, sw))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal segstart[AST_CPROP_SWMAX], segend[AST_CPROP_SWMAX];
	int nseg = 0, has_default = 0, pending = 0, inbody = 0, first = 1;
	AstLocal cs = AST_NONE, ce = AST_NONE;
	for (AstLocal c = ast_first_child(a, body); c != AST_NONE;
			 c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		uint16_t ck = ast_kind(a, c);
		int cop = ck == AST_Jump ? ast_op(a, c) : -1;
		if (cop == 2 || cop == 3) { MCC_TRACE("br\n");
			if (cop == 3)
				{ MCC_TRACE("br\n"); has_default = 1; }
			if (inbody)
				{ MCC_TRACE("br\n"); return 0; }
			pending = 1;
		} else if (cop == 0) { MCC_TRACE("br\n");
			if (nseg >= AST_CPROP_SWMAX)
				{ MCC_TRACE("br\n"); return 0; }
			if (inbody) { MCC_TRACE("br\n");
				segstart[nseg] = cs;
				segend[nseg] = ce;
				nseg++;
				inbody = 0;
			} else if (pending) { MCC_TRACE("br\n");
				segstart[nseg] = AST_NONE;
				segend[nseg] = AST_NONE;
				nseg++;
			} else { MCC_TRACE("br\n");
				return 0;
			}
			pending = 0;
		} else { MCC_TRACE("br\n");
			if (cop >= 0 || ck == AST_Return)
				{ MCC_TRACE("br\n"); return 0; }
			if (!ast_cprop_arm_clean(a, c))
				{ MCC_TRACE("br\n"); return 0; }
			if (!inbody) { MCC_TRACE("br\n");
				if (first || !pending)
					{ MCC_TRACE("br\n"); return 0; }
				cs = c;
				inbody = 1;
			}
			ce = c;
		}
		first = 0;
	}
	if (inbody) { MCC_TRACE("br\n");
		if (nseg >= AST_CPROP_SWMAX)
			{ MCC_TRACE("br\n"); return 0; }
		segstart[nseg] = cs;
		segend[nseg] = ce;
		nseg++;
	} else if (pending) { MCC_TRACE("br\n");
		if (nseg >= AST_CPROP_SWMAX)
			{ MCC_TRACE("br\n"); return 0; }
		segstart[nseg] = AST_NONE;
		segend[nseg] = AST_NONE;
		nseg++;
	}
	if (!has_default)
		{ MCC_TRACE("br\n"); return 0; }
	ast_cprop_vis[body] = 1;
	ast_cprop_rewrite(a, val, 0);
	AstCpropState in, acc;
	ast_cprop_state_save(&in);
	int have = 0;
	for (int i = 0; i < nseg; i++) { MCC_TRACE("br\n");
		ast_cprop_state_load(&in);
		if (segstart[i] != AST_NONE)
			{ MCC_TRACE("br\n"); for (AstLocal s = segstart[i];; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
				ast_cprop_stmt(a, s);
				if (s == segend[i])
					{ MCC_TRACE("br\n"); break; }
			} }
		if (!have) { MCC_TRACE("br\n");
			ast_cprop_state_save(&acc);
			have = 1;
		} else { MCC_TRACE("br\n");
			ast_cprop_state_meet(&acc);
			ast_cprop_state_save(&acc);
		}
	}
	if (have)
		{ MCC_TRACE("br\n"); ast_cprop_state_load(&acc); }
	return 1;
}

static void ast_cprop_stmt(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	uint16_t k = ast_kind(a, s);
	if (k == AST_Store) { MCC_TRACE("br\n");
		AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
		if (!ast_cprop_safe(a, lval) || !ast_cprop_safe(a, val)) { MCC_TRACE("br\n");
			ast_cprop_kn = 0;
			return;
		}
		ast_cprop_rewrite(a, val, 0);
		ast_cprop_rewrite(a, lval, 1);
		int off, tt;
		if (ast_cprop_is_local(a, lval, &off, &tt) && !ast_cprop_escapes(a, off)) { MCC_TRACE("br\n");
			int lt;
			uint64_t lv;
			if (ast_ident_cval(a, val, &lt, &lv) && ast_cprop_tt_same(lt, tt))
				{ MCC_TRACE("br\n"); ast_cprop_gen(off, tt, lv); }
			else
				{ MCC_TRACE("br\n"); ast_cprop_kill(off); }
		}
	} else if (k == AST_Return) { MCC_TRACE("br\n");
		if (ast_first_child(a, s) != AST_NONE &&
				ast_cprop_safe(a, ast_first_child(a, s)))
			{ MCC_TRACE("br\n"); ast_cprop_rewrite(a, ast_first_child(a, s), 0); }
		ast_cprop_kn = 0;
	} else if (k == AST_BasicBlock) { MCC_TRACE("br\n");
		ast_cprop_stmts(a, s);
	} else if (k == AST_If && ast_op(a, s) == 0) { MCC_TRACE("br\n");
		AstLocal cond = ast_child(a, s, 0);
		AstLocal tb = ast_child(a, s, 1), eb = ast_child(a, s, 2);
		if (cond != AST_NONE && ast_cprop_safe(a, cond))
			{ MCC_TRACE("br\n"); ast_cprop_rewrite(a, cond, 0); }
		else
			{ MCC_TRACE("br\n"); ast_cprop_cond_kill(a, cond); }
		if (ast_cprop_opaque(a, cond) || ast_cprop_opaque(a, tb) ||
				ast_cprop_opaque(a, eb)) { MCC_TRACE("br\n");
			ast_cprop_kn = 0;
			return;
		}
		AstCpropState in, tout;
		ast_cprop_state_save(&in);
		ast_cprop_stmts(a, tb);
		ast_cprop_state_save(&tout);
		ast_cprop_state_load(&in);
		ast_cprop_stmts(a, eb);
		ast_cprop_state_meet(&tout);
	} else if (k == AST_If && ((ast_op(a, s) >= 2 && ast_op(a, s) <= 6) || ast_op(a, s) == 8)) { MCC_TRACE("br\n");
		if (ast_op(a, s) == 6 && ast_cprop_switch_meet(a, s))
			{ MCC_TRACE("br\n"); return; }
		for (int i = 0; i < ast_cprop_kn;)
			{ MCC_TRACE("br\n"); if (ast_licm_written(a, s, ast_cprop_koff[i]))
				{ MCC_TRACE("br\n"); ast_cprop_kill(ast_cprop_koff[i]); }
			else
				{ MCC_TRACE("br\n"); i++; } }
		if (ast_sccp_has_label(a, s)) { MCC_TRACE("br\n");
			ast_cprop_kn = 0;
			return;
		}
		AstCpropState in;
		ast_cprop_state_save(&in);
		for (AstLocal c = ast_first_child(a, s); c != AST_NONE;
				 c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
			if (ast_kind(a, c) == AST_BasicBlock) { MCC_TRACE("br\n");
				ast_cprop_stmts(a, c);
				ast_cprop_state_load(&in);
			} else if (ast_cprop_safe(a, c)) { MCC_TRACE("br\n");
				ast_cprop_rewrite(a, c, 0);
			}
		}
	} else if (k == AST_Invoke && ast_call_window_env) { MCC_TRACE("br\n");
		for (int i = 0; i < ast_cprop_kn;)
			{ MCC_TRACE("br\n"); if (ast_cprop_escapes(a, ast_cprop_koff[i]) ||
					ast_licm_written(a, s, ast_cprop_koff[i]))
				{ MCC_TRACE("br\n"); ast_cprop_kill(ast_cprop_koff[i]); }
			else
				{ MCC_TRACE("br\n"); i++; } }
	} else { MCC_TRACE("br\n");
		ast_cprop_kn = 0;
	}
}

static int ast_cprop_run(AstArena *a) { MCC_TRACE("enter\n");
	ast_cprop_folds = 0;
	AstLocal nn = ast_count(a);
	/* setjmp: the second return re-enters this function on an edge this pass
	 * does not model, so a value forwarded across the call can be stale. See
	 * ast_body_has_setjmp(). */
	if (nn && ast_body_has_setjmp(a))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_cprop_join_env && nn) { MCC_TRACE("br\n");
		ast_cprop_vis = mcc_mallocz(nn);
		ast_cprop_kn = 0;
		ast_cprop_stmts(a, ast_root(a));
		for (AstLocal n = 0; n < nn; n++)
			{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_BasicBlock && !ast_cprop_vis[n])
				{ MCC_TRACE("br\n"); ast_cprop_block(a, n); } }
		mcc_free(ast_cprop_vis);
		ast_cprop_vis = NULL;
		return ast_cprop_folds;
	}
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_BasicBlock)
			{ MCC_TRACE("br\n"); ast_cprop_block(a, n); } }
	return ast_cprop_folds;
}

#define AST_DSE_MAX 128
static int ast_dse_koff[AST_DSE_MAX];
static int ast_dse_kwidth[AST_DSE_MAX];
static AstLocal ast_dse_kstore[AST_DSE_MAX];
static int ast_dse_kn;
static int ast_dse_folds;

static int ast_dse_find(int off) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_dse_kn; i++)
		{ MCC_TRACE("br\n"); if (ast_dse_koff[i] == off)
			{ MCC_TRACE("br\n"); return i; } }
	return -1;
}

static void ast_dse_kill(int off) { MCC_TRACE("enter\n");
	int i = ast_dse_find(off);
	if (i < 0)
		{ MCC_TRACE("br\n"); return; }
	ast_dse_kn--;
	ast_dse_koff[i] = ast_dse_koff[ast_dse_kn];
	ast_dse_kwidth[i] = ast_dse_kwidth[ast_dse_kn];
	ast_dse_kstore[i] = ast_dse_kstore[ast_dse_kn];
}

static void ast_dse_kill_reads(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE || !ast_dse_kn)
		{ MCC_TRACE("br\n"); return; }
	if (ast_kind(a, n) == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM))
			{ MCC_TRACE("br\n"); ast_dse_kill((int)(int64_t)ast_ival(a, n)); }
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_dse_kill_reads(a, c); }
}

static void ast_dse_gen(int off, int width, AstLocal store) { MCC_TRACE("enter\n");
	int i = ast_dse_find(off);
	if (i < 0) { MCC_TRACE("br\n");
		if (ast_dse_kn >= AST_DSE_MAX)
			{ MCC_TRACE("br\n"); return; }
		i = ast_dse_kn++;
		ast_dse_koff[i] = off;
	}
	ast_dse_kstore[i] = store;
	ast_dse_kwidth[i] = width;
}

static int ast_dse_local(AstArena *a, AstLocal n, int *off, int *width) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	int r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	int t = ast_type_t(a, n), bt = t & VT_BTYPE;
	if ((t & (VT_VOLATILE | VT_ARRAY | VT_BITFIELD)) || bt == VT_STRUCT ||
			bt == VT_VOID || bt == VT_FUNC)
		{ MCC_TRACE("br\n"); return 0; }
	CType ct;
	ct.t = t;
	ct.bp = ast_type_bp(a, n);
	ct.bs = ast_type_bs(a, n);
	ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
	int al, w = type_size(&ct, &al);
	if (w <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	*off = (int)(int64_t)ast_ival(a, n);
	*width = w;
	return 1;
}

static void ast_dse_block(AstArena *a, AstLocal bb) { MCC_TRACE("enter\n");
	ast_dse_kn = 0;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		if (ast_kind(a, s) != AST_Store) { MCC_TRACE("br\n");
			if (ast_dse_call_env && ast_kind(a, s) == AST_Invoke) { MCC_TRACE("br\n");
				ast_dse_kill_reads(a, s);
				continue;
			}
			ast_dse_kn = 0;
			continue;
		}
		AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
		ast_dse_kill_reads(a, val);
		if (!ast_cprop_safe(a, val)) { MCC_TRACE("br\n");
			ast_dse_kn = 0;
			continue;
		}
		int off, w;
		if (ast_dse_local(a, lval, &off, &w) && !ast_cprop_escapes(a, off)) { MCC_TRACE("br\n");
			int i = ast_dse_find(off);
			if (i >= 0 && w >= ast_dse_kwidth[i]) { MCC_TRACE("br\n");
				ast_set_kind(a, ast_dse_kstore[i], AST_Poison);
				ast_clear_children(a, ast_dse_kstore[i]);
				ast_dse_folds++;
			}
			ast_dse_gen(off, w, s);
		} else { MCC_TRACE("br\n");
			ast_dse_kn = 0;
		}
	}
}

static int ast_dse_run(AstArena *a) { MCC_TRACE("enter\n");
	ast_dse_folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_BasicBlock)
			{ MCC_TRACE("br\n"); ast_dse_block(a, n); } }
	return ast_dse_folds;
}

static int ast_sccp_folds;

static int ast_sccp_has_label_compute(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Jump && ast_op(a, n) == 4)
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_op(a, n) == AST_OP_ASMGEN) { MCC_TRACE("br\n");
		AstAsmEff e;
		memset(&e, 0, sizeof e);
		ast_asm_eff_node(a, n, &e);
		if (e.unknown || (e.eff & MCC_ASM_EFF_GOTO))
			{ MCC_TRACE("br\n"); return 1; }
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_sccp_has_label(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_sccp_has_case_compute(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Jump &&
			(ast_op(a, n) == 2 || ast_op(a, n) == 3))
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_kind(a, n) == AST_If && ast_op(a, n) == 6)
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_sccp_has_case(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_sccp_scan(AstArena *a) { MCC_TRACE("enter\n");
	int folded = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_If || ast_op(a, n) != 0)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal cond = ast_child(a, n, 0);
		int tt;
		uint64_t v;
		if (!ast_ident_cval(a, cond, &tt, &v))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal thenbb = ast_child(a, n, 1);
		AstLocal elsebb = ast_child(a, n, 2);
		AstLocal taken = v ? thenbb : elsebb;
		AstLocal dead = v ? elsebb : thenbb;
		if (ast_sccp_has_label(a, dead) || ast_sccp_has_case(a, dead))
			{ MCC_TRACE("br\n"); continue; }
		if (taken == AST_NONE) { MCC_TRACE("br\n");
			ast_set_kind(a, n, AST_Poison);
			ast_clear_children(a, n);
		} else { MCC_TRACE("br\n");
			ast_set_kind(a, n, AST_BasicBlock);
			ast_clear_children(a, n);
			ast_add_child(a, n, taken);
		}
		folded++;
	}
	return folded;
}

static int ast_nonnull_ref(AstArena *a, AstLocal op, const int *offs, int noff) { MCC_TRACE("enter\n");
	int r, off;
	if (ast_kind(a, op) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	r = ast_op(a, op);
	if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	off = (int)(int64_t)ast_ival(a, op);
	for (int i = 0; i < noff; i++)
		{ MCC_TRACE("br\n"); if (offs[i] == off)
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
static int ast_nonnull_params(AstArena *a, Sym *fsym, int *offs, int max) { MCC_TRACE("enter\n");
	int n = 0;
	if (!fsym || !fsym->type.ref || fsym->type.ref->f.func_type != FUNC_NEW)
		{ MCC_TRACE("br\n"); return 0; }
	for (Sym *p = fsym->type.ref->next; p; p = p->next) { MCC_TRACE("br\n");
		int v = p->v & ~SYM_FIELD;
		if (v < TOK_IDENT)
			{ MCC_TRACE("br\n"); continue; }
		Sym *ls = sym_find(v);
		if (!ls)
			{ MCC_TRACE("br\n"); continue; }
		if ((ls->r & VT_VALMASK) != VT_LOCAL || !(ls->r & VT_LVAL) || (ls->r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		if ((ls->type.t & VT_BTYPE) != VT_PTR)
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_local_is_readonly(a, (int)ls->c))
			{ MCC_TRACE("br\n"); continue; }
		if (n < max)
			{ MCC_TRACE("br\n"); offs[n++] = (int)ls->c; }
	}
	return n;
}

static int ast_nonnull_fold(AstArena *a, const int *offs, int noff) { MCC_TRACE("enter\n");
	int folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int op, lt;
		uint64_t lv;
		AstLocal x, y;
		if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
			{ MCC_TRACE("br\n"); continue; }
		op = ast_op(a, n);
		if (op != TOK_EQ && op != TOK_NE)
			{ MCC_TRACE("br\n"); continue; }
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		if (!((ast_nonnull_ref(a, x, offs, noff) && ast_ident_cval(a, y, &lt, &lv) && lv == 0) ||
					(ast_nonnull_ref(a, y, offs, noff) && ast_ident_cval(a, x, &lt, &lv) && lv == 0)))
			{ MCC_TRACE("br\n"); continue; }
		ast_ident_setlit(a, n, VT_INT, op == TOK_NE ? 1u : 0u);
		folds++;
	}
	return folds;
}
#endif

static int ast_constparam_probe(AstArena *a, int off, int64_t *val) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int op, lt;
		uint64_t lv;
		AstLocal x, y;
		if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
			{ MCC_TRACE("br\n"); continue; }
		op = ast_op(a, n);
		if (op != TOK_EQ && op != TOK_NE)
			{ MCC_TRACE("br\n"); continue; }
		x = ast_child(a, n, 0);
		y = ast_child(a, n, 1);
		if (ast_nonnull_ref(a, x, &off, 1) && ast_ident_cval(a, y, &lt, &lv)) { MCC_TRACE("br\n");
			*val = (int64_t)lv;
			return 1;
		}
		if (ast_nonnull_ref(a, y, &off, 1) && ast_ident_cval(a, x, &lt, &lv)) { MCC_TRACE("br\n");
			*val = (int64_t)lv;
			return 1;
		}
	}
	return 0;
}

static int ast_constparam_params(AstArena *a, Sym *fsym, int *offs, int64_t *vals, int max) { MCC_TRACE("enter\n");
	int n = 0;
	if (!fsym || !fsym->type.ref || fsym->type.ref->f.func_type != FUNC_NEW)
		{ MCC_TRACE("br\n"); return 0; }
	for (Sym *p = fsym->type.ref->next; p; p = p->next) { MCC_TRACE("br\n");
		int v = p->v & ~SYM_FIELD;
		int64_t cval;
		if (v < TOK_IDENT)
			{ MCC_TRACE("br\n"); continue; }
		Sym *ls = sym_find(v);
		if (!ls)
			{ MCC_TRACE("br\n"); continue; }
		if ((ls->r & VT_VALMASK) != VT_LOCAL || !(ls->r & VT_LVAL) || (ls->r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_ident_intt(ls->type.t))
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_local_is_readonly(a, (int)ls->c))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_du_slot_flags(a, (int)ls->c) & AST_DU_ESCAPED)
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_constparam_probe(a, (int)ls->c, &cval))
			{ MCC_TRACE("br\n"); continue; }
		if ((int64_t)(int32_t)cval != cval)
			{ MCC_TRACE("br\n"); continue; }
		if (n < max) { MCC_TRACE("br\n");
			offs[n] = (int)ls->c;
			vals[n] = cval;
			n++;
		}
	}
	return n;
}

static int ast_constparam_fold(AstArena *a, const int *offs, const int64_t *vals, int noff) { MCC_TRACE("enter\n");
	int folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int r, off, tt;
		uint64_t ref;
		if (ast_kind(a, n) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		r = ast_op(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		off = (int)(int64_t)ast_ival(a, n);
		for (int i = 0; i < noff; i++)
			{ MCC_TRACE("br\n"); if (offs[i] == off) { MCC_TRACE("br\n");
				if (!ast_ident_etype(a, n, &tt, &ref))
					{ MCC_TRACE("br\n"); tt = VT_INT; }
				ast_ident_setlit(a, n, tt, (uint64_t)vals[i]);
				folds++;
				break;
			} }
	}
	return folds;
}

static int ast_rangeparam_params(AstArena *a, Sym *fsym, int *offs, int64_t *los,
																 int64_t *his, int max) { MCC_TRACE("enter\n");
	int n = 0;
	if (!fsym || !fsym->type.ref || fsym->type.ref->f.func_type != FUNC_NEW)
		{ MCC_TRACE("br\n"); return 0; }
	for (Sym *p = fsym->type.ref->next; p; p = p->next) { MCC_TRACE("br\n");
		int v = p->v & ~SYM_FIELD;
		AstVLat vl;
		if (v < TOK_IDENT)
			{ MCC_TRACE("br\n"); continue; }
		Sym *ls = sym_find(v);
		if (!ls)
			{ MCC_TRACE("br\n"); continue; }
		if ((ls->r & VT_VALMASK) != VT_LOCAL || !(ls->r & VT_LVAL) || (ls->r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_ident_intt(ls->type.t))
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_local_is_readonly(a, (int)ls->c))
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_vlat_context(a, (int)ls->c, &vl) || vl.state != AST_VLAT_FACT)
			{ MCC_TRACE("br\n"); continue; }
		if (vl.lo > vl.hi)
			{ MCC_TRACE("br\n"); continue; }
		if (n < max) { MCC_TRACE("br\n");
			offs[n] = (int)ls->c;
			los[n] = vl.lo;
			his[n] = vl.hi;
			n++;
		}
	}
	return n;
}

static int ast_sccp_run(AstArena *a) { MCC_TRACE("enter\n");
	ast_sccp_folds = ast_sccp_scan(a);
	if (ast_sccp_fix_env) { MCC_TRACE("br\n");
		for (;;) { MCC_TRACE("br\n");
			int c = ast_cprop_run(a);
			int s = ast_sccp_scan(a);
			ast_sccp_folds += s;
			if (c == 0 && s == 0)
				{ MCC_TRACE("br\n"); break; }
		}
	}
	return ast_sccp_folds;
}

static int ast_jt_folds;

static int ast_jt_arm_empty(AstArena *a, AstLocal br) { MCC_TRACE("enter\n");
	return br == AST_NONE ||
				 (ast_kind(a, br) == AST_BasicBlock && ast_nchild(a, br) == 0);
}

static int ast_jt_run(AstArena *a) { MCC_TRACE("enter\n");
	ast_jt_folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_If || ast_op(a, n) != 0)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal cond = ast_child(a, n, 0);
		if (cond == AST_NONE || !ast_ident_pure(a, cond))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal thenbb = ast_child(a, n, 1);
		AstLocal elsebb = ast_child(a, n, 2);
		int cbt = ast_type_t(a, cond) & VT_BTYPE;
		if (ast_kind(a, cond) == AST_Literal && cbt != VT_FLOAT &&
				cbt != VT_DOUBLE && cbt != VT_LDOUBLE && cbt != VT_STRUCT) { MCC_TRACE("br\n");
			int taken = ast_ival(a, cond) != 0;
			AstLocal keep = taken ? thenbb : elsebb;
			AstLocal drop = taken ? elsebb : thenbb;
			if (drop == AST_NONE || (!ast_sccp_has_label(a, drop) &&
															 !ast_sccp_has_case(a, drop))) { MCC_TRACE("br\n");
				ast_clear_children(a, n);
				if (keep == AST_NONE) { MCC_TRACE("br\n");
					ast_set_kind(a, n, AST_Poison);
				} else { MCC_TRACE("br\n");
					ast_set_kind(a, n, AST_BasicBlock);
					ast_add_child(a, n, keep);
				}
				ast_jt_folds++;
				continue;
			}
		}
		if (ast_jt_arm_empty(a, thenbb) && ast_jt_arm_empty(a, elsebb)) { MCC_TRACE("br\n");
			ast_set_kind(a, n, AST_Poison);
			ast_clear_children(a, n);
			ast_jt_folds++;
			continue;
		}
		if (thenbb != AST_NONE && elsebb != AST_NONE &&
				ast_kind(a, thenbb) == AST_BasicBlock &&
				ast_kind(a, elsebb) == AST_BasicBlock &&
				!ast_sccp_has_label(a, thenbb) && !ast_sccp_has_label(a, elsebb) &&
				ast_ident_same(a, thenbb, elsebb)) { MCC_TRACE("br\n");
			ast_set_kind(a, n, AST_BasicBlock);
			ast_clear_children(a, n);
			ast_add_child(a, n, thenbb);
			ast_jt_folds++;
		}
	}
	return ast_jt_folds;
}

#define AST_TCO_LABEL (-0x54434f)
static int ast_tco_folds;

static int ast_tco_reads_off(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM) &&
				(int)(int64_t)ast_ival(a, n) == off)
			{ MCC_TRACE("br\n"); return 1; }
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_tco_reads_off(a, c, off))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_tco_local_addr_escapes(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, n);
		if (k == AST_Unary) { MCC_TRACE("br\n");
			int op = ast_op(a, n);
			if (op != AST_OP_ADDR && op != AST_OP_MEMBER &&
					op != AST_OP_MEMBER_ARROW)
				{ MCC_TRACE("br\n"); continue; }
			AstLocal c = ast_first_child(a, n);
			if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
				{ MCC_TRACE("br\n"); continue; }
			int r = ast_op(a, c);
			if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM))
				{ MCC_TRACE("br\n"); return 1; }
			continue;
		}
		if (k != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		if ((ast_op(a, n) & (VT_VALMASK | VT_SYM | VT_LVAL)) == VT_LOCAL)
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

typedef struct {
	AstLocal arg[AST_TCO_MAXP];
	int order[AST_TCO_MAXP];
	int no;
} AstTcoPlan;

static AstLocal ast_tco_selfcall(AstArena *a, AstLocal n, Sym *fsym, int np) { MCC_TRACE("enter\n");
	AstLocal cref;
	if (n != AST_NONE && ast_kind(a, n) == AST_Convert &&
			ast_nchild(a, n) == 1 && !ast_fbits(a, n)) { MCC_TRACE("br\n");
		AstLocal in = ast_first_child(a, n);
		if (in != AST_NONE && ast_kind(a, in) == AST_Invoke &&
				ast_type_t(a, in) == ast_type_t(a, n) &&
				ast_type_ref(a, in) == ast_type_ref(a, n) &&
				ast_type_bp(a, in) == ast_type_bp(a, n) &&
				ast_type_bs(a, in) == ast_type_bs(a, n))
			{ MCC_TRACE("br\n"); n = in; }
	}
	if (n == AST_NONE || ast_kind(a, n) != AST_Invoke)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if ((int)ast_nchild(a, n) != np + 1)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	cref = ast_child(a, n, 0);
	if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!(ast_op(a, cref) & VT_SYM) ||
			(void *)(uintptr_t)ast_sym(a, cref) != (void *)fsym)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	return n;
}

static int ast_tco_plan(AstArena *a, AstLocal inv, int np, const int *poff,
												AstTcoPlan *pl) { MCC_TRACE("enter\n");
	int need[AST_TCO_MAXP], emitted[AST_TCO_MAXP];
	int nwrite = 0;
	for (int i = 0; i < np; i++) { MCC_TRACE("br\n");
		pl->arg[i] = ast_child(a, inv, i + 1);
		if (!ast_cprop_safe(a, pl->arg[i]))
			{ MCC_TRACE("br\n"); return 0; }
	}
	for (int i = 0; i < np; i++) { MCC_TRACE("br\n");
		need[i] = 1;
		if (ast_kind(a, pl->arg[i]) == AST_Ref) { MCC_TRACE("br\n");
			int rr = ast_op(a, pl->arg[i]);
			if ((rr & VT_VALMASK) == VT_LOCAL && !(rr & VT_SYM) &&
					(int)(int64_t)ast_ival(a, pl->arg[i]) == poff[i])
				{ MCC_TRACE("br\n"); need[i] = 0; }
		}
		emitted[i] = !need[i];
		if (need[i])
			{ MCC_TRACE("br\n"); nwrite++; }
	}
	pl->no = 0;
	while (pl->no < nwrite) { MCC_TRACE("br\n");
		int pick = -1;
		for (int i = 0; i < np; i++) { MCC_TRACE("br\n");
			if (emitted[i])
				{ MCC_TRACE("br\n"); continue; }
			int blocked = 0;
			for (int k = 0; k < np; k++) { MCC_TRACE("br\n");
				if (k == i || emitted[k] || !need[k])
					{ MCC_TRACE("br\n"); continue; }
				if (ast_tco_reads_off(a, pl->arg[k], poff[i])) { MCC_TRACE("br\n");
					blocked = 1;
					break;
				}
			}
			if (!blocked) { MCC_TRACE("br\n");
				pick = i;
				break;
			}
		}
		if (pick < 0)
			{ MCC_TRACE("br\n"); return 0; }
		emitted[pick] = 1;
		pl->order[pl->no++] = pick;
	}
	return 1;
}

static void ast_tco_emit(AstArena *a, AstLocal dst, const AstTcoPlan *pl,
												 const int *poff, const int *ptt,
												 const uint64_t *pref) { MCC_TRACE("enter\n");
	for (int oi = 0; oi < pl->no; oi++) { MCC_TRACE("br\n");
		int i = pl->order[oi];
		AstLocal lref = ast_node(a, AST_Ref);
		ast_set_op(a, lref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, lref, (uint64_t)poff[i]);
		ast_set_type(a, lref, ptt[i], pref[i]);
		AstLocal cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, ptt[i], pref[i]);
		ast_add_child(a, cvt, pl->arg[i]);
		AstLocal st = ast_node(a, AST_Store);
		ast_add_child(a, st, lref);
		ast_add_child(a, st, cvt);
		ast_add_child(a, dst, st);
	}
	AstLocal jmp = ast_node(a, AST_Jump);
	ast_set_op(a, jmp, 5);
	ast_set_ival(a, jmp, (uint64_t)(unsigned)AST_TCO_LABEL);
	ast_add_child(a, dst, jmp);
}

static int ast_tco_run(AstArena *a, Sym *fsym) { MCC_TRACE("enter\n");
	ast_tco_folds = 0;
	if (!fsym || !fsym->type.ref)
		{ MCC_TRACE("br\n"); return 0; }
	if (fsym->type.ref->f.func_type != FUNC_NEW)
		{ MCC_TRACE("br\n"); return 0; }
	int poff[AST_TCO_MAXP], ptt[AST_TCO_MAXP];
	uint64_t pref[AST_TCO_MAXP];
	int np = 0;
	for (Sym *p = fsym->type.ref->next; p; p = p->next) { MCC_TRACE("br\n");
		int v = p->v & ~SYM_FIELD;
		if (v < TOK_IDENT || np >= ast_tco_maxp)
			{ MCC_TRACE("br\n"); return 0; }
		Sym *ls = sym_find(v);
		if (!ls)
			{ MCC_TRACE("br\n"); return 0; }
		if ((ls->r & VT_VALMASK) != VT_LOCAL || !(ls->r & VT_LVAL) || (ls->r & VT_SYM))
			{ MCC_TRACE("br\n"); return 0; }
		int t = ls->type.t;
		int tbt = t & VT_BTYPE;
		if ((!ast_ident_intt(t) && tbt != VT_FLOAT && tbt != VT_DOUBLE &&
				 tbt != VT_LDOUBLE &&
				 !(ast_tco_ptr_env && tbt == VT_PTR)) ||
				(t & VT_VOLATILE) || (t & (VT_ARRAY | VT_VLA)))
			{ MCC_TRACE("br\n"); return 0; }
		poff[np] = (int)ls->c;
		ptt[np] = t;
		pref[np] = (uint64_t)(uintptr_t)ls->type.ref;
		np++;
	}
	if (np < 1)
		{ MCC_TRACE("br\n"); return 0; }
	for (int i = 0; i < np; i++)
		{ MCC_TRACE("br\n"); if (ast_cprop_escapes(a, poff[i]))
			{ MCC_TRACE("br\n"); return 0; } }
	if (ast_tco_local_addr_escapes(a))
		{ MCC_TRACE("br\n"); return 0; }

	int converted = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal r = 0; r < nn; r++) { MCC_TRACE("br\n");
		if (ast_kind(a, r) != AST_Return)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal top = ast_first_child(a, r);
		AstLocal iff = AST_NONE, va = AST_NONE, vb = AST_NONE;
		AstTcoPlan pa, pb;
		int oka = 0, okb = 0;
		if (top != AST_NONE && ast_kind(a, top) == AST_If) { MCC_TRACE("br\n");
			if (ast_op(a, top) != 5 || ast_nchild(a, top) != 3 ||
					ast_ival(a, top) || ast_fbits(a, top) ||
					ast_nchild(a, r) != 1 || ast_fbits(a, r))
				{ MCC_TRACE("br\n"); continue; }
			va = ast_child(a, top, 1);
			vb = ast_child(a, top, 2);
			if (ast_child(a, top, 0) == AST_NONE || va == AST_NONE || vb == AST_NONE)
				{ MCC_TRACE("br\n"); continue; }
			AstLocal ia = ast_tco_selfcall(a, va, fsym, np);
			AstLocal ib = ast_tco_selfcall(a, vb, fsym, np);
			oka = ia != AST_NONE && ast_tco_plan(a, ia, np, poff, &pa);
			okb = ib != AST_NONE && ast_tco_plan(a, ib, np, poff, &pb);
			if (!oka && !okb)
				{ MCC_TRACE("br\n"); continue; }
			iff = top;
		} else { MCC_TRACE("br\n");
			AstLocal inv = ast_tco_selfcall(a, top, fsym, np);
			if (inv == AST_NONE)
				{ MCC_TRACE("br\n"); continue; }
			if (!ast_tco_plan(a, inv, np, poff, &pa))
				{ MCC_TRACE("br\n"); continue; }
		}
		if (iff == AST_NONE) { MCC_TRACE("br\n");
			ast_set_kind(a, r, AST_BasicBlock);
			ast_clear_children(a, r);
			ast_tco_emit(a, r, &pa, poff, ptt, pref);
		} else { MCC_TRACE("br\n");
			AstLocal cnd = ast_child(a, iff, 0);
			AstLocal ba = ast_node(a, AST_BasicBlock);
			AstLocal bb = ast_node(a, AST_BasicBlock);
			int rop = ast_op(a, r);
			uint64_t rival = ast_ival(a, r);
			if (oka) { MCC_TRACE("br\n");
				ast_tco_emit(a, ba, &pa, poff, ptt, pref);
			} else { MCC_TRACE("br\n");
				AstLocal rt = ast_node(a, AST_Return);
				ast_set_op(a, rt, rop);
				ast_set_ival(a, rt, rival);
				ast_add_child(a, rt, va);
				ast_add_child(a, ba, rt);
			}
			if (okb) { MCC_TRACE("br\n");
				ast_tco_emit(a, bb, &pb, poff, ptt, pref);
			} else { MCC_TRACE("br\n");
				AstLocal re = ast_node(a, AST_Return);
				ast_set_op(a, re, rop);
				ast_set_ival(a, re, rival);
				ast_add_child(a, re, vb);
				ast_add_child(a, bb, re);
			}
			ast_clear_children(a, iff);
			ast_set_op(a, iff, 0);
			ast_set_ival(a, iff, 0);
			ast_set_fbits(a, iff, 0);
			ast_set_type(a, iff, 0, 0);
			ast_add_child(a, iff, cnd);
			ast_add_child(a, iff, ba);
			ast_add_child(a, iff, bb);
			ast_set_kind(a, r, AST_BasicBlock);
			ast_set_op(a, r, 0);
			ast_set_ival(a, r, 0);
			ast_set_fbits(a, r, 0);
			ast_clear_children(a, r);
			ast_add_child(a, r, iff);
		}
		converted++;
	}
	if (!converted)
		{ MCC_TRACE("br\n"); return 0; }

	AstLocal root = ast_root(a);
	AstLocal lbl = ast_node(a, AST_Jump);
	ast_set_op(a, lbl, 4);
	ast_set_ival(a, lbl, (uint64_t)(unsigned)AST_TCO_LABEL);
	AstLocal first = ast_first_child(a, root);
	ast_clear_children(a, root);
	ast_add_child(a, root, lbl);
	for (AstLocal c = first; c != AST_NONE;) { MCC_TRACE("br\n");
		AstLocal nx = ast_next_sib(a, c);
		ast_add_child(a, root, c);
		c = nx;
	}
	ast_fn_tco = 1;
	ast_tco_folds = converted;
	return converted;
}

static AstLocal ast_cse_expr[AST_CSE_MAX];
static AstLocal ast_cse_ref[AST_CSE_MAX];
static int ast_cse_off[AST_CSE_MAX];
static int ast_cse_n;
static int ast_cse_folds;

static int ast_cse_scalar(int t) { MCC_TRACE("enter\n");
	int bt = t & VT_BTYPE;
	return ast_ident_intt(t) || bt == VT_FLOAT || bt == VT_DOUBLE || bt == VT_PTR;
}

static int ast_cse_wide(int t) { MCC_TRACE("enter\n");
	int bt = t & VT_BTYPE;
	return ast_cse_scalar(t) || bt == VT_LDOUBLE || bt == VT_QFLOAT ||
				 bt == VT_FLOAT128 || bt == VT_INT128 || bt == VT_QLONG;
}

static int ast_cse_is_local(AstArena *a, AstLocal n, int *off, int *tt) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	int r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	int t = ast_type_t(a, n);
	if (!ast_cse_wide(t) || (t & (VT_VOLATILE | VT_BITFIELD)))
		{ MCC_TRACE("br\n"); return 0; }
	*off = (int)(int64_t)ast_ival(a, n);
	*tt = t;
	return 1;
}

static int ast_cse_regpure_compute(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int t = ast_type_t(a, n);
	if (t & VT_VOLATILE)
		{ MCC_TRACE("br\n"); return 0; }
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_Literal:
		return 1;
	case AST_Ref: {
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			{ MCC_TRACE("br\n"); return 0; }
		if ((t & VT_BITFIELD) || !ast_cse_wide(t))
			{ MCC_TRACE("br\n"); return 0; }
		return 1;
	}
	case AST_Convert:
		if (t & VT_BITFIELD)
			{ MCC_TRACE("br\n"); return 0; }
		break;
	case AST_Binary:
		switch (ast_op(a, n)) { MCC_TRACE("br\n");
		case '+':
		case '-':
		case '*':
		case '&':
		case '|':
		case '^':
		case TOK_SHL:
		case TOK_SHR:
		case TOK_SAR:
		case TOK_LT:
		case TOK_GT:
		case TOK_LE:
		case TOK_GE:
		case TOK_EQ:
		case TOK_NE:
		case TOK_ULT:
		case TOK_UGE:
		case TOK_ULE:
		case TOK_UGT:
			break;
		default:
			return 0;
		}
		break;
	default:
		return 0;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_cse_regpure(a, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static void ast_cse_setref(AstArena *a, AstLocal n, AstLocal ref) { MCC_TRACE("enter\n");
	a->epoch++;
	ast_clear_children(a, n);
	a->kind[n] = AST_Ref;
	a->op[n] = a->op[ref];
	a->type_t[n] = a->type_t[ref];
	a->type_bp[n] = a->type_bp[ref];
	a->type_bs[n] = a->type_bs[ref];
	a->type_ref[n] = a->type_ref[ref];
	a->ival[n] = a->ival[ref];
	a->fbits[n] = a->fbits[ref];
	a->sym[n] = a->sym[ref];
}

static int ast_cse_commutative_op(int op) { MCC_TRACE("enter\n");
	if ((op == TOK_EQ || op == TOK_NE) && ast_cse_comm_rel_env)
		{ MCC_TRACE("br\n"); return 1; }
	return op == '+' || op == '*' || op == '&' || op == '|' || op == '^';
}

static int ast_cse_same(AstArena *a, AstLocal x, AstLocal y) { MCC_TRACE("enter\n");
	if (ast_ident_same(a, x, y))
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_cse_comm_env && ast_kind(a, x) == AST_Binary &&
			ast_kind(a, y) == AST_Binary && ast_nchild(a, x) == 2 &&
			ast_nchild(a, y) == 2 && ast_op(a, x) == ast_op(a, y) &&
			ast_cse_commutative_op(ast_op(a, x)) && ast_type_t(a, x) == ast_type_t(a, y)) { MCC_TRACE("br\n");
		AstLocal x0 = ast_child(a, x, 0), x1 = ast_child(a, x, 1);
		AstLocal y0 = ast_child(a, y, 0), y1 = ast_child(a, y, 1);
		return ast_ident_same(a, x0, y1) && ast_ident_same(a, x1, y0);
	}
	if (ast_cse_comm_env && ast_cse_comm_rel_env && ast_kind(a, x) == AST_Binary &&
			ast_kind(a, y) == AST_Binary && ast_nchild(a, x) == 2 &&
			ast_nchild(a, y) == 2 && ast_rel_swap(ast_op(a, x)) == ast_op(a, y) &&
			ast_type_t(a, x) == ast_type_t(a, y)) { MCC_TRACE("br\n");
		AstLocal x0 = ast_child(a, x, 0), x1 = ast_child(a, x, 1);
		AstLocal y0 = ast_child(a, y, 0), y1 = ast_child(a, y, 1);
		return ast_ident_same(a, x0, y1) && ast_ident_same(a, x1, y0);
	}
	return 0;
}

static int ast_cse_try_match(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_cse_n; i++) { MCC_TRACE("br\n");
		if (ast_cse_expr[i] == n)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_cse_same(a, ast_cse_expr[i], n)) { MCC_TRACE("br\n");
			ast_cse_setref(a, n, ast_cse_ref[i]);
			ast_cse_folds++;
			return 1;
		}
	}
	return 0;
}

static void ast_cse_subst(AstArena *a, AstLocal n, int lval) { MCC_TRACE("enter\n");
	if (n == AST_NONE || !ast_cse_n)
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	if (!lval && ast_cse_try_match(a, n))
		{ MCC_TRACE("br\n"); return; }
	if (k == AST_Store) { MCC_TRACE("br\n");
		ast_cse_subst(a, ast_child(a, n, 0), 1);
		ast_cse_subst(a, ast_child(a, n, 1), 0);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_cse_subst(a, c, clval); }
}

static void ast_cse_kill(AstArena *a, int off) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_cse_n;) { MCC_TRACE("br\n");
		if (ast_cse_off[i] == off || ast_tco_reads_off(a, ast_cse_expr[i], off)) { MCC_TRACE("br\n");
			ast_cse_n--;
			ast_cse_expr[i] = ast_cse_expr[ast_cse_n];
			ast_cse_ref[i] = ast_cse_ref[ast_cse_n];
			ast_cse_off[i] = ast_cse_off[ast_cse_n];
		} else { MCC_TRACE("br\n");
			i++;
		}
	}
}

static MCC_OPT_TLS int ast_licm_folds;

static int ast_asm_writes_off(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	int op;
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	op = ast_op(a, n);
	if (op == AST_OP_ASMOPS) { MCC_TRACE("br\n");
		for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
			{ MCC_TRACE("br\n"); if (ast_tco_reads_off(a, c, off))
				{ MCC_TRACE("br\n"); return 1; } }
		return 0;
	}
	if (op == AST_OP_ASMGEN) { MCC_TRACE("br\n");
		AstAsmEff e;
		memset(&e, 0, sizeof e);
		ast_asm_eff_node(a, n, &e);
		if ((e.unknown || (e.eff & MCC_ASM_EFF_MEM)) && ast_cprop_escapes(a, off))
			{ MCC_TRACE("br\n"); return 1; }
		return 0;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_asm_writes_off(a, c, off))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_licm_written(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	uint16_t k = ast_kind(a, n);
	if (ast_fn_asm_live && ast_asm_writes_off(a, n, off))
		{ MCC_TRACE("br\n"); return 1; }
	if (k == AST_Store && ast_ref_is_local_off(a, ast_child(a, n, 0), off))
		{ MCC_TRACE("br\n"); return 1; }
	if (k == AST_Unary && ast_ref_is_local_off(a, ast_first_child(a, n), off))
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_licm_written(a, c, off))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_licm_operands_ok(AstArena *a, AstLocal loop, AstLocal e) { MCC_TRACE("enter\n");
	if (e == AST_NONE)
		{ MCC_TRACE("br\n"); return 1; }
	int off, tt;
	if (ast_kind(a, e) == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, e);
		if ((r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM)) { MCC_TRACE("br\n");
			off = (int)(int64_t)ast_ival(a, e);
			if (ast_cprop_escapes(a, off))
				{ MCC_TRACE("br\n"); return 0; }
			if (ast_licm_written(a, loop, off))
				{ MCC_TRACE("br\n"); return 0; }
		}
	} else if (ast_cprop_is_local(a, e, &off, &tt)) { MCC_TRACE("br\n");
		if (ast_cprop_escapes(a, off))
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_licm_written(a, loop, off))
			{ MCC_TRACE("br\n"); return 0; }
	}
	for (AstLocal c = ast_first_child(a, e); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_licm_operands_ok(a, loop, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static void ast_licm_subst(AstArena *a, AstLocal n, AstLocal e, AstLocal ref,
													 int lval) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	if (!lval && n != e && ast_ident_same(a, e, n)) { MCC_TRACE("br\n");
		ast_cse_setref(a, n, ref);
		ast_licm_folds++;
		return;
	}
	if (k == AST_Store) { MCC_TRACE("br\n");
		ast_licm_subst(a, ast_child(a, n, 0), e, ref, 1);
		ast_licm_subst(a, ast_child(a, n, 1), e, ref, 0);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_licm_subst(a, c, e, ref, clval); }
}

static int ast_licm_is_loop(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	if (ast_kind(a, s) != AST_If)
		{ MCC_TRACE("br\n"); return 0; }
	int op = ast_op(a, s);
	return op == 2 || op == 3 || op == 4 || op == 8;
}

#define AST_COST_PRESS_MAX 64
#define AST_COST_FP_BUDGET 8
#ifdef AST_PROMO_CALLEE_N
#define AST_COST_GP_BUDGET AST_PROMO_CALLEE_N
#else
#define AST_COST_GP_BUDGET 4
#endif
#define AST_COST_SPILL_W 48

static void ast_cost_press_scan(AstArena *a, AstLocal n, int *off, int *flt,
																int *nc) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	if (ast_kind(a, n) == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM)) { MCC_TRACE("br\n");
			int o = (int)(int64_t)ast_ival(a, n);
			int tt = ast_type_t(a, n);
			int bt = tt & VT_BTYPE;
			int scalar = (bt == VT_INT || bt == VT_LLONG || bt == VT_PTR ||
										bt == VT_FLOAT || bt == VT_DOUBLE) &&
									 !(tt & (VT_ARRAY | VT_BITFIELD));
			int j;
			for (j = 0; j < *nc; j++)
				{ MCC_TRACE("br\n"); if (off[j] == o)
					{ MCC_TRACE("br\n"); break; } }
			if (scalar && j == *nc && *nc < AST_COST_PRESS_MAX) { MCC_TRACE("br\n");
				off[*nc] = o;
				flt[*nc] = (bt == VT_FLOAT || bt == VT_DOUBLE);
				(*nc)++;
			}
		}
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_cost_press_scan(a, c, off, flt, nc); }
}

static int ast_cost_is_expr(int k) { MCC_TRACE("enter\n");
	return k == AST_Unary || k == AST_Binary || k == AST_Convert ||
				 k == AST_Invoke || k == AST_Load || k == AST_Store;
}

static int ast_cost_su(AstArena *a, AstLocal n, int fp) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	int k = ast_kind(a, n);
	int bt = ast_type_t(a, n) & VT_BTYPE;
	int isfp = (bt == VT_FLOAT || bt == VT_DOUBLE);
	int b1 = 0, b2 = 0, nch = 0;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		int v = ast_cost_su(a, c, fp);
		if (v > b1) { MCC_TRACE("br\n");
			b2 = b1;
			b1 = v;
		} else if (v > b2)
			{ MCC_TRACE("br\n"); b2 = v; }
		nch++;
	}
	if (!ast_cost_is_expr(k)) { MCC_TRACE("br\n");
		if (nch == 0)
			{ MCC_TRACE("br\n"); return (k == AST_Ref || k == AST_Literal) && isfp == fp ? 1 : 0; }
		return b1;
	}
	if (nch == 0)
		{ MCC_TRACE("br\n"); return isfp == fp ? 1 : 0; }
	int need = (b1 == b2 && b1 > 0) ? b1 + 1 : b1;
	if (isfp == fp && need == 0)
		{ MCC_TRACE("br\n"); need = 1; }
	return need;
}

static long ast_cost_spill(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n, p;
	long pen = 0;
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (!ast_licm_is_loop(a, n))
			{ MCC_TRACE("br\n"); continue; }
		int d = 1;
		for (p = ast_parent(a, n); p != AST_NONE; p = ast_parent(a, p))
			{ MCC_TRACE("br\n"); if (ast_licm_is_loop(a, p))
				{ MCC_TRACE("br\n"); d++; } }
		int off[AST_COST_PRESS_MAX], flt[AST_COST_PRESS_MAX], nc = 0;
		ast_cost_press_scan(a, n, off, flt, &nc);
		int ni = 0, nf = 0, ex = 0;
		for (int j = 0; j < nc; j++)
			{ MCC_TRACE("br\n"); if (flt[j])
				{ MCC_TRACE("br\n"); nf++; }
			else
				{ MCC_TRACE("br\n"); ni++; } }
		ni += ast_cost_su(a, n, 0);
		nf += ast_cost_su(a, n, 1);
		if (ni > AST_COST_GP_BUDGET)
			{ MCC_TRACE("br\n"); ex += ni - AST_COST_GP_BUDGET; }
		if (nf > AST_COST_FP_BUDGET)
			{ MCC_TRACE("br\n"); ex += nf - AST_COST_FP_BUDGET; }
		if (ex) { MCC_TRACE("br\n");
			int dd = d > 4 ? 4 : d;
			pen += (long)ex * AST_COST_SPILL_W * ((long)1 << (2 * (dd - 1)));
		}
	}
	return pen;
}

static int ast_cost_opw(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (ast_kind(a, n) != AST_Binary)
		{ MCC_TRACE("br\n"); return 1; }
	switch (ast_op(a, n)) { MCC_TRACE("br\n");
	case '/':
	case '%':
	case TOK_PDIV:
	case TOK_UDIV:
	case TOK_UMOD:
		return 20;
	case '*':
		return 3;
	default:
		return 1;
	}
}

long ast_cost_score(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n, p;
	int nodes = (int)nn, calls = 0, maxdepth = 0;
	if (ast_cost_ops_env) { MCC_TRACE("br\n");
		nodes = 0;
		for (n = 0; n < nn; n++)
			{ MCC_TRACE("br\n"); nodes += ast_cost_opw(a, n); }
	}
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) == AST_Invoke)
			{ MCC_TRACE("br\n"); calls++; }
		if (ast_licm_is_loop(a, n)) { MCC_TRACE("br\n");
			int d = 1;
			for (p = ast_parent(a, n); p != AST_NONE; p = ast_parent(a, p))
				{ MCC_TRACE("br\n"); if (ast_licm_is_loop(a, p))
					{ MCC_TRACE("br\n"); d++; } }
			if (d > maxdepth)
				{ MCC_TRACE("br\n"); maxdepth = d; }
		}
	}
	long s = (long)nodes * (maxdepth + 1) * (calls + 1);
	if (ast_cost_spill_env)
		{ MCC_TRACE("br\n"); s += ast_cost_spill(a); }
	return s;
}

static void ast_fn_cost(AstArena *a, const char *fn) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n, p;
	int nodes = (int)nn, calls = 0, maxdepth = 0;
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) == AST_Invoke)
			{ MCC_TRACE("br\n"); calls++; }
		if (ast_licm_is_loop(a, n)) { MCC_TRACE("br\n");
			int d = 1;
			for (p = ast_parent(a, n); p != AST_NONE; p = ast_parent(a, p))
				{ MCC_TRACE("br\n"); if (ast_licm_is_loop(a, p))
					{ MCC_TRACE("br\n"); d++; } }
			if (d > maxdepth)
				{ MCC_TRACE("br\n"); maxdepth = d; }
		}
	}
	fprintf(stderr, "ast-cost: %s nodes=%d loopdepth=%d calls=%d score=%ld\n", fn,
					nodes, maxdepth, calls, ast_cost_score(a));
}

static int ast_bf_eqkey(AstArena *a, AstLocal n, AstLocal *key) { MCC_TRACE("enter\n");
	AstLocal l, r;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != TOK_EQ ||
			ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) { MCC_TRACE("br\n");
		*key = l;
		return 1;
	}
	if (ast_kind(a, l) == AST_Literal) { MCC_TRACE("br\n");
		*key = r;
		return 1;
	}
	return 0;
}

static int ast_bf_cond_key(AstArena *a, AstLocal n, AstLocal *key) { MCC_TRACE("enter\n");
	AstLocal cond;
	if (ast_kind(a, n) != AST_If || ast_op(a, n) != 0)
		{ MCC_TRACE("br\n"); return 0; }
	cond = ast_first_child(a, n);
	return cond != AST_NONE && ast_bf_eqkey(a, cond, key);
}

static void ast_bf_report(AstArena *a, const char *fn) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), n, m;
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstLocal key, k2;
		int cnt = 0, dup = 0;
		if (!ast_bf_cond_key(a, n, &key))
			{ MCC_TRACE("br\n"); continue; }
		for (m = 0; m < nn; m++) { MCC_TRACE("br\n");
			if (!ast_bf_cond_key(a, m, &k2) || !ast_ident_same(a, key, k2))
				{ MCC_TRACE("br\n"); continue; }
			if (m < n) { MCC_TRACE("br\n");
				dup = 1;
				break;
			}
			cnt++;
		}
		if (!dup && cnt >= 3)
			{ MCC_TRACE("br\n"); fprintf(stderr, "bitflag-candidate: %s cluster=%d\n", fn, cnt); }
	}
}

static int ast_bf_folds;

AstLocal ast_dup_sub(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal d = ast_node(a, ast_kind(a, n));
	ast_set_op(a, d, ast_op(a, n));
	ast_copy_type(a, d, a, n);
	ast_set_ival(a, d, ast_ival(a, n));
	ast_set_fbits(a, d, ast_fbits(a, n));
	ast_set_sym(a, d, ast_sym(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_add_child(a, d, ast_dup_sub(a, c)); }
	return d;
}

static int ast_inline_scalar_ok(int tt) { MCC_TRACE("enter\n");
	int bt = tt & VT_BTYPE;
	return bt == VT_BOOL || bt == VT_BYTE || bt == VT_SHORT || bt == VT_INT ||
				 bt == VT_LLONG || bt == VT_PTR || bt == VT_FLOAT || bt == VT_DOUBLE ||
				 bt == VT_LDOUBLE || bt == VT_QFLOAT || bt == VT_QLONG || bt == VT_INT128;
}

static int ast_inline_type_ok(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int tt = ast_type_t(a, n);
	if (tt & VT_VOLATILE)
		{ MCC_TRACE("br\n"); return 0; }
	int bt = tt & VT_BTYPE;
	if (bt == VT_STRUCT)
		{ MCC_TRACE("br\n"); return 0; }
	return 1;
}

static int ast_inline_node_allowed(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	uint16_t k = ast_kind(a, n);
	switch (k) { MCC_TRACE("br\n");
	case AST_Literal:
	case AST_Ref:
	case AST_Load:
	case AST_Convert:
	case AST_Binary:
		return 1;
	case AST_Unary: {
		int op = ast_op(a, n);
		return op == AST_OP_ADDR || op == AST_OP_MEMBER || op == AST_OP_MEMBER_ARROW;
	}
	case AST_If:
		return ast_op(a, n) == 5 && ast_nchild(a, n) == 3;
	default:
		return 0;
	}
}

static int ast_inline_expr_pure(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_node_is_asm(a, n))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_inline_type_ok(a, n) || !ast_inline_node_allowed(a, n))
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_inline_expr_pure(a, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static AstLocal ast_inline_local_init(AstArena *a, int off) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), found = AST_NONE;
	int count = 0;
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Store)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal d = ast_first_child(a, n);
		if (d == AST_NONE || ast_kind(a, d) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		int r = ast_op(a, d);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM) ||
				(int)(int64_t)ast_ival(a, d) != off)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal rhs = ast_next_sib(a, d);
		if (rhs == AST_NONE)
			{ MCC_TRACE("br\n"); return AST_NONE; }
		count++;
		found = rhs;
	}
	return count == 1 ? found : AST_NONE;
}

static int ast_inline_ref_temp_ok(AstArena *a, struct AstInlineFn *e, AstLocal n) { MCC_TRACE("enter\n");
	int r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
		{ MCC_TRACE("br\n"); return 1; }
	int off = (int)(int64_t)ast_ival(a, n);
	for (int i = 0; i < e->nparams; i++)
		{ MCC_TRACE("br\n"); if (e->param_off[i] == off)
			{ MCC_TRACE("br\n"); return 1; } }
	return ast_inline_local_init(a, off) != AST_NONE;
}

static int ast_inline_expr_ok(AstArena *a, struct AstInlineFn *e, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	uint16_t k = ast_kind(a, n);
	if (!ast_inline_type_ok(a, n) || !ast_inline_node_allowed(a, n))
		{ MCC_TRACE("br\n"); return 0; }
	if (k == AST_Ref && !ast_inline_ref_temp_ok(a, e, n))
		{ MCC_TRACE("br\n"); return 0; }
	if (k == AST_Unary && ast_op(a, n) == AST_OP_ADDR) { MCC_TRACE("br\n");
		AstLocal c0 = ast_first_child(a, n);
		if (c0 != AST_NONE && ast_kind(a, c0) == AST_Ref) { MCC_TRACE("br\n");
			int r = ast_op(a, c0);
			if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) { MCC_TRACE("br\n");
				int off = (int)(int64_t)ast_ival(a, c0), isparam = 0;
				for (int i = 0; i < e->nparams; i++)
					{ MCC_TRACE("br\n"); if (e->param_off[i] == off)
						{ MCC_TRACE("br\n"); isparam = 1; break; } }
				if (!isparam)
					{ MCC_TRACE("br\n"); return 0; }
			}
		}
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_inline_expr_ok(a, e, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static int ast_inline_pass_simple(struct AstInlineFn *e) { MCC_TRACE("enter\n");
	if (!e->graftable)
		{ MCC_TRACE("br\n"); return 0; }
	AstArena *a = e->ast;
	AstLocal root = ast_root(a);
	if (ast_kind(a, root) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	int nret = 0;
	for (AstLocal s = ast_first_child(a, root); s != AST_NONE; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, s);
		if (k == AST_Store) { MCC_TRACE("br\n");
			AstLocal d = ast_first_child(a, s);
			if (d == AST_NONE || ast_kind(a, d) != AST_Ref)
				{ MCC_TRACE("br\n"); return 0; }
			int r = ast_op(a, d);
			if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
				{ MCC_TRACE("br\n"); return 0; }
			int doff = (int)(int64_t)ast_ival(a, d);
			for (int pi = 0; pi < e->nparams; pi++)
				{ MCC_TRACE("br\n"); if (e->param_off[pi] == doff)
					{ MCC_TRACE("br\n"); return 0; } }
			if (ast_inline_local_init(a, doff) == AST_NONE)
				{ MCC_TRACE("br\n"); return 0; }
			AstLocal rhs = ast_next_sib(a, d);
			if (rhs == AST_NONE || !ast_inline_expr_ok(a, e, rhs))
				{ MCC_TRACE("br\n"); return 0; }
			continue;
		}
		if (k == AST_Return) { MCC_TRACE("br\n");
			if (ast_nchild(a, s) != 1 || !ast_inline_expr_ok(a, e, ast_first_child(a, s)))
				{ MCC_TRACE("br\n"); return 0; }
			nret++;
			continue;
		}
		{ MCC_TRACE("br\n"); return 0; }
	}
	return nret == 1;
}

static AstLocal ast_inline_copy_expr(AstArena *dst, AstArena *src, AstLocal n,
																		 const AstLocal *argmap, const int *param_off,
																		 int nparams, int depth) { MCC_TRACE("enter\n");
	if (depth > 48)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (ast_kind(src, n) == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(src, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) { MCC_TRACE("br\n");
			int off = (int)(int64_t)ast_ival(src, n);
			for (int i = 0; i < nparams; i++)
				{ MCC_TRACE("br\n"); if (param_off[i] == off) { MCC_TRACE("br\n");
					AstLocal cv = ast_node(dst, AST_Convert);
					ast_copy_type(dst, cv, src, n);
					ast_add_child(dst, cv, ast_dup_sub(dst, argmap[i]));
					return cv;
				} }
			AstLocal init = ast_inline_local_init(src, off);
			if (init == AST_NONE)
				{ MCC_TRACE("br\n"); return AST_NONE; }
			return ast_inline_copy_expr(dst, src, init, argmap, param_off, nparams, depth + 1);
		}
	}
	AstLocal d = ast_node(dst, ast_kind(src, n));
	ast_set_op(dst, d, ast_op(src, n));
	ast_copy_type(dst, d, src, n);
	ast_set_ival(dst, d, ast_ival(src, n));
	ast_set_fbits(dst, d, ast_fbits(src, n));
	ast_set_sym(dst, d, ast_sym(src, n));
	for (AstLocal c = ast_first_child(src, n); c != AST_NONE; c = ast_next_sib(src, c)) { MCC_TRACE("br\n");
		AstLocal cc = ast_inline_copy_expr(dst, src, c, argmap, param_off, nparams, depth + 1);
		if (cc == AST_NONE)
			{ MCC_TRACE("br\n"); return AST_NONE; }
		ast_add_child(dst, d, cc);
	}
	return d;
}

static int ast_inline_graft_node(AstArena *a, AstLocal call, struct AstInlineFn *e) { MCC_TRACE("enter\n");
	int nargs = (int)ast_nchild(a, call) - 1;
	if (nargs != e->nparams)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_inline_depth >= ast_inline_depth_max)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_graft_budget < (int)ast_count(e->ast))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_graft_limit >= 0 && ast_graft_total >= ast_graft_limit)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal argmap[AST_INLINE_MAX_PARAMS];
	for (int i = 0; i < e->nparams; i++) { MCC_TRACE("br\n");
		AstLocal arg = ast_child(a, call, (uint32_t)(i + 1));
		if (arg == AST_NONE || !ast_inline_expr_pure(a, arg))
			{ MCC_TRACE("br\n"); return 0; }
		argmap[i] = arg;
	}
	int callt = ast_type_t(a, call);
	uint64_t callr = ast_type_ref(a, call);
	if (!ast_inline_scalar_ok(callt))
		{ MCC_TRACE("br\n"); return 0; }
	AstArena *ca = e->ast;
	AstLocal cnn = ast_count(ca), rv = AST_NONE;
	for (AstLocal n = 0; n < cnn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(ca, n) == AST_Return && ast_nchild(ca, n) == 1) { MCC_TRACE("br\n");
			rv = ast_first_child(ca, n);
			break;
		} }
	if (rv == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	ast_graft_total++;
	ast_graft_budget -= (int)ast_count(ca);
	AstLocal nv = ast_inline_copy_expr(a, ca, rv, argmap, e->param_off, e->nparams, 0);
	if (nv == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	ast_set_kind(a, call, AST_Convert);
	ast_set_op(a, call, 0);
	ast_set_type(a, call, callt, callr);
	ast_set_ival(a, call, 0);
	ast_set_fbits(a, call, 0);
	ast_set_sym(a, call, 0);
	ast_clear_children(a, call);
	ast_add_child(a, call, nv);
	return 1;
}

static void ast_inline_mark_divfed(AstArena *a, AstLocal nn, unsigned char *fed) { MCC_TRACE("enter\n");
	for (AstLocal d = 0; d < nn; d++) { MCC_TRACE("br\n");
		if (ast_kind(a, d) != AST_Binary || ast_nchild(a, d) != 2 || ast_op(a, d) != '/')
			{ MCC_TRACE("br\n"); continue; }
		for (AstLocal c = ast_first_child(a, d); c != AST_NONE; c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
			AstLocal u = c;
			while (u != AST_NONE && ast_kind(a, u) == AST_Convert)
				u = ast_first_child(a, u);
			if (u != AST_NONE && ast_kind(a, u) == AST_Invoke)
				{ MCC_TRACE("br\n"); fed[u] = 1; }
		}
	}
}

static int ast_inline_run(AstArena *a) { MCC_TRACE("enter\n");
	if (!ast_inline_pass_env)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal nn = ast_count(a);
	int grafted = 0;
	ast_inline_depth = 0;
	ast_graft_budget = ast_graft_budget_max;
	unsigned char *divfed = NULL;
	if (ast_inline_divguard && nn) { MCC_TRACE("br\n");
		divfed = mcc_mallocz(nn);
		ast_inline_mark_divfed(a, nn, divfed);
	}
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Invoke)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal cref = ast_child(a, n, 0);
		if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		void *csym = (void *)(uintptr_t)ast_sym(a, cref);
		struct AstInlineFn *e = ast_inline_find(csym);
		if (!e || !e->graftable || !ast_inline_pass_simple(e))
			{ MCC_TRACE("br\n"); continue; }
		if (divfed && divfed[n] && !is_float(ast_type_t(a, n)))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_inline_graft_node(a, n, e)) { MCC_TRACE("br\n");
			grafted++;
			MCC_TRACE("inline graft call=%u callee=%s\n", (unsigned)n,
								get_tok_str(((Sym *)csym)->v, NULL));
		}
	}
	if (divfed)
		{ MCC_TRACE("br\n"); mcc_free(divfed); }
	return grafted;
}

static int ast_bf_cmpconst(AstArena *a, AstLocal n, int cmpop, AstLocal *key,
													 uint64_t *v) { MCC_TRACE("enter\n");
	AstLocal l, r, k, c;
	int ct, kt;
	uint64_t cv, kref;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != cmpop ||
			ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) { MCC_TRACE("br\n");
		k = l;
		c = r;
	} else if (ast_kind(a, l) == AST_Literal) { MCC_TRACE("br\n");
		k = r;
		c = l;
	} else { MCC_TRACE("br\n");
		return 0;
	}
	if (!ast_ident_cval(a, c, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, k, &kt, &kref) || !ast_ident_intt(kt))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, k))
		{ MCC_TRACE("br\n"); return 0; }
	*key = k;
	*v = cv;
	return 1;
}

static int ast_bf_eqconst(AstArena *a, AstLocal n, AstLocal *key, uint64_t *v) { MCC_TRACE("enter\n");
	return ast_bf_cmpconst(a, n, TOK_EQ, key, v);
}

#define AST_BF_MAXVALS 256

static int ast_bf_cond_parse_op(AstArena *a, AstLocal cond, int joinop,
																int cmpop, AstLocal *key, uint64_t *vals,
																int *cnt) { MCC_TRACE("enter\n");
	AstLocal k;
	uint64_t v;
	if (ast_bf_cmpconst(a, cond, cmpop, &k, &v)) { MCC_TRACE("br\n");
		if (*key == AST_NONE)
			{ MCC_TRACE("br\n"); *key = k; }
		else if (!ast_ident_same(a, *key, k))
			{ MCC_TRACE("br\n"); return 0; }
		if (*cnt >= AST_BF_MAXVALS)
			{ MCC_TRACE("br\n"); return 0; }
		vals[(*cnt)++] = v;
		return 1;
	}
	if (ast_kind(a, cond) != AST_Binary || ast_op(a, cond) != joinop ||
			ast_nchild(a, cond) < 2)
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal c = ast_first_child(a, cond); c != AST_NONE;
			 c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		if (!ast_bf_cmpconst(a, c, cmpop, &k, &v))
			{ MCC_TRACE("br\n"); return 0; }
		if (*key == AST_NONE)
			{ MCC_TRACE("br\n"); *key = k; }
		else if (!ast_ident_same(a, *key, k))
			{ MCC_TRACE("br\n"); return 0; }
		if (*cnt >= AST_BF_MAXVALS)
			{ MCC_TRACE("br\n"); return 0; }
		vals[(*cnt)++] = v;
	}
	return 1;
}

static int ast_bf_cond_parse(AstArena *a, AstLocal cond, AstLocal *key,
														 uint64_t *vals, int *cnt) { MCC_TRACE("enter\n");
	return ast_bf_cond_parse_op(a, cond, TOK_LOR, TOK_EQ, key, vals, cnt);
}

static int ast_bf_window(const uint64_t *vals, int cnt, uint64_t *mask,
												 uint64_t *base) { MCC_TRACE("enter\n");
	int i;
	int64_t b = (int64_t)vals[0];
	uint64_t m = 0;
	for (i = 1; i < cnt; i++)
		{ MCC_TRACE("br\n"); if ((int64_t)vals[i] < b)
			{ MCC_TRACE("br\n"); b = (int64_t)vals[i]; } }
	for (i = 0; i < cnt; i++) { MCC_TRACE("br\n");
		uint64_t d = vals[i] - (uint64_t)b;
		if (d > 63)
			{ MCC_TRACE("br\n"); return 0; }
		m |= (uint64_t)1 << d;
	}
	*mask = m;
	*base = (uint64_t)b;
	return 1;
}

static int ast_bf_has_label(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Jump) { MCC_TRACE("br\n");
		int op = ast_op(a, n);
		if (op == 2 || op == 3 || op == 4)
			{ MCC_TRACE("br\n"); return 1; }
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_bf_has_label(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static AstLocal ast_bf_lit(AstArena *a, int tt, uint64_t v) { MCC_TRACE("enter\n");
	AstLocal n = ast_node(a, AST_Literal);
	ast_set_op(a, n, VT_CONST);
	ast_set_type(a, n, tt, 0);
	ast_set_ival(a, n, v);
	return n;
}

static AstLocal ast_bf_bin(AstArena *a, int op, int tt, AstLocal l, AstLocal r) { MCC_TRACE("enter\n");
	AstLocal n = ast_node(a, AST_Binary);
	ast_set_op(a, n, op);
	ast_set_type(a, n, tt, 0);
	ast_add_child(a, n, l);
	ast_add_child(a, n, r);
	return n;
}

#ifdef MCC_TARGET_I386
static AstLocal ast_bf_localref(AstArena *a, int off, int tt, uint64_t tref) { MCC_TRACE("enter\n");
	AstLocal r = ast_node(a, AST_Ref);
	ast_set_op(a, r, VT_LOCAL | VT_LVAL);
	ast_set_ival(a, r, (uint64_t)off);
	ast_set_type(a, r, tt, tref);
	return r;
}
#endif

static AstLocal ast_bf_ucast(AstArena *a, int tt, AstLocal key) { MCC_TRACE("enter\n");
	AstLocal n = ast_node(a, AST_Convert);
	ast_set_type(a, n, tt, 0);
	ast_add_child(a, n, ast_dup_sub(a, key));
	return n;
}

static AstLocal ast_bf_keyexpr(AstArena *a, int kw, AstLocal key,
															 uint64_t base) { MCC_TRACE("enter\n");
	AstLocal u = ast_bf_ucast(a, kw, key);
	if (base == 0)
		{ MCC_TRACE("br\n"); return u; }
	return ast_bf_bin(a, '-', kw, u, ast_bf_lit(a, kw, base));
}

static AstLocal ast_bf_build(AstArena *a, AstLocal key, uint64_t mask,
														 uint64_t base) { MCC_TRACE("enter\n");
	int kt, kw;
	uint64_t kref;
	ast_ident_etype(a, key, &kt, &kref);
	kw = (kt & VT_BTYPE) == VT_LLONG ? VT_LLONG | VT_UNSIGNED
																	 : VT_INT | VT_UNSIGNED;
	int mw = VT_LLONG | VT_UNSIGNED;
	AstLocal guard = ast_bf_bin(a, TOK_ULT, VT_INT,
															ast_bf_keyexpr(a, kw, key, base),
															ast_bf_lit(a, kw, 64));
	AstLocal amt = ast_bf_bin(a, '&', kw, ast_bf_keyexpr(a, kw, key, base),
														ast_bf_lit(a, kw, 63));
	AstLocal bit = ast_bf_bin(
			a, '&', mw, ast_bf_bin(a, TOK_SHR, mw, ast_bf_lit(a, mw, mask), amt),
			ast_bf_lit(a, mw, 1));
	AstLocal cvt = ast_node(a, AST_Convert);
	ast_set_type(a, cvt, VT_INT, 0);
	ast_add_child(a, cvt, bit);
	return ast_bf_bin(a, '&', VT_INT, cvt, guard);
}

static void ast_bf_drop(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	ast_set_kind(a, n, AST_Poison);
	ast_clear_children(a, n);
}

static int ast_bf_try_if(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	AstLocal key = AST_NONE, drop[64];
	uint64_t vals[AST_BF_MAXVALS], mask = 0, base = 0;
	int cnt = 0, ndrop = 0;
	AstLocal cond0 = ast_child(a, s, 0);
	AstLocal thenbb = ast_child(a, s, 1);
	if (thenbb == AST_NONE || ast_kind(a, thenbb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_bf_cond_parse(a, cond0, &key, vals, &cnt))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal tail = ast_child(a, s, 2);
	for (;;) { MCC_TRACE("br\n");
		if (tail == AST_NONE || ast_kind(a, tail) != AST_BasicBlock ||
				ast_nchild(a, tail) != 1 || ndrop > 60)
			{ MCC_TRACE("br\n"); break; }
		AstLocal inner = ast_first_child(a, tail);
		if (ast_kind(a, inner) != AST_If || ast_op(a, inner) != 0)
			{ MCC_TRACE("br\n"); break; }
		AstLocal icond = ast_child(a, inner, 0);
		AstLocal ithen = ast_child(a, inner, 1);
		if (ithen == AST_NONE || !ast_ident_same(a, thenbb, ithen))
			{ MCC_TRACE("br\n"); break; }
		if (ast_bf_has_label(a, icond) || ast_bf_has_label(a, ithen))
			{ MCC_TRACE("br\n"); break; }
		AstLocal k2 = key;
		int c2 = cnt;
		if (!ast_bf_cond_parse(a, icond, &k2, vals, &c2))
			{ MCC_TRACE("br\n"); break; }
		key = k2;
		cnt = c2;
		drop[ndrop++] = tail;
		drop[ndrop++] = inner;
		drop[ndrop++] = icond;
		tail = ast_child(a, inner, 2);
	}
	if (cnt < ast_bitflag_min || key == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_bf_window(vals, cnt, &mask, &base))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal cond = ast_bf_build(a, key, mask, base);
	if (ast_fbits(a, cond0) & AST_FB_LANDOR_INVERT)
		{ MCC_TRACE("br\n"); cond = ast_bf_bin(a, '^', VT_INT, cond, ast_bf_lit(a, VT_INT, 1)); }
	ast_clear_children(a, s);
	ast_add_child(a, s, cond);
	ast_add_child(a, s, thenbb);
	if (tail != AST_NONE)
		{ MCC_TRACE("br\n"); ast_add_child(a, s, tail); }
	ast_bf_drop(a, cond0);
	for (int i = 0; i < ndrop; i++)
		{ MCC_TRACE("br\n"); ast_bf_drop(a, drop[i]); }
	return 1;
}

static int ast_bf_try_ifne(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	AstLocal key = AST_NONE, drop[64];
	uint64_t vals[AST_BF_MAXVALS], mask = 0, base = 0;
	int cnt = 0, ndrop = 0;
	if (ast_child(a, s, 2) != AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal cond0 = ast_child(a, s, 0);
	if (ast_bf_has_label(a, cond0))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_bf_cond_parse_op(a, cond0, TOK_LAND, TOK_NE, &key, vals, &cnt))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal body = ast_child(a, s, 1);
	for (;;) { MCC_TRACE("br\n");
		if (body == AST_NONE || ast_kind(a, body) != AST_BasicBlock ||
				ast_nchild(a, body) != 1 || ndrop > 60)
			{ MCC_TRACE("br\n"); break; }
		AstLocal inner = ast_first_child(a, body);
		if (ast_kind(a, inner) != AST_If || ast_op(a, inner) != 0)
			{ MCC_TRACE("br\n"); break; }
		if (ast_child(a, inner, 2) != AST_NONE)
			{ MCC_TRACE("br\n"); break; }
		AstLocal icond = ast_child(a, inner, 0);
		if (ast_bf_has_label(a, icond))
			{ MCC_TRACE("br\n"); break; }
		AstLocal k2 = key;
		int c2 = cnt;
		if (!ast_bf_cond_parse_op(a, icond, TOK_LAND, TOK_NE, &k2, vals, &c2))
			{ MCC_TRACE("br\n"); break; }
		key = k2;
		cnt = c2;
		drop[ndrop++] = body;
		drop[ndrop++] = inner;
		drop[ndrop++] = icond;
		body = ast_child(a, inner, 1);
	}
	if (cnt < ast_bitflag_min || key == AST_NONE || body == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_bf_window(vals, cnt, &mask, &base))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal member = ast_bf_build(a, key, mask, base);
	AstLocal cond = (ast_fbits(a, cond0) & AST_FB_LANDOR_INVERT)
			? member
			: ast_bf_bin(a, '^', VT_INT, member, ast_bf_lit(a, VT_INT, 1));
	ast_clear_children(a, s);
	ast_add_child(a, s, cond);
	ast_add_child(a, s, body);
	ast_bf_drop(a, cond0);
	for (int i = 0; i < ndrop; i++)
		{ MCC_TRACE("br\n"); ast_bf_drop(a, drop[i]); }
	return 1;
}

static int ast_bf_try_lor(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal key = AST_NONE;
	uint64_t vals[AST_BF_MAXVALS], mask = 0, base = 0;
	int cnt = 0;
	if (ast_fbits(a, n) & AST_FB_LANDOR_INVERT)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_bf_cond_parse(a, n, &key, vals, &cnt))
		{ MCC_TRACE("br\n"); return 0; }
	if (cnt < ast_bitflag_min || key == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_bf_window(vals, cnt, &mask, &base))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal res = ast_bf_build(a, key, mask, base);
	AstLocal bit = ast_first_child(a, res);
	AstLocal guard = ast_next_sib(a, bit);
	ast_bf_drop(a, res);
	ast_set_op(a, n, '&');
	ast_set_type(a, n, VT_INT, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, bit);
	ast_add_child(a, n, guard);
	return 1;
}

static int ast_bf_try_land(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal key = AST_NONE;
	uint64_t vals[AST_BF_MAXVALS], mask = 0, base = 0;
	int cnt = 0;
	if (ast_fbits(a, n) & AST_FB_LANDOR_INVERT)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_bf_cond_parse_op(a, n, TOK_LAND, TOK_NE, &key, vals, &cnt))
		{ MCC_TRACE("br\n"); return 0; }
	if (cnt < ast_bitflag_min || key == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_bf_window(vals, cnt, &mask, &base))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal member = ast_bf_build(a, key, mask, base);
	ast_set_op(a, n, '^');
	ast_set_type(a, n, VT_INT, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, member);
	ast_add_child(a, n, ast_bf_lit(a, VT_INT, 1));
	return 1;
}

static int ast_bf_run(AstArena *a) { MCC_TRACE("enter\n");
	ast_bf_folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_If && ast_op(a, n) == 0)
			{ MCC_TRACE("br\n"); ast_bf_folds += ast_bf_try_if(a, n); } }
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_If && ast_op(a, n) == 0)
			{ MCC_TRACE("br\n"); ast_bf_folds += ast_bf_try_ifne(a, n); } }
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LOR)
			{ MCC_TRACE("br\n"); ast_bf_folds += ast_bf_try_lor(a, n); } }
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LAND)
			{ MCC_TRACE("br\n"); ast_bf_folds += ast_bf_try_land(a, n); } }
	return ast_bf_folds;
}

static MCC_OPT_TLS int ast_range_folds;

static int ast_range_bound(AstArena *a, AstLocal n, AstLocal *key, int64_t *bound,
													 int *is_lower) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), eff, ct, kt, keyleft;
	AstLocal l, r, k, c;
	uint64_t cv, kref;
	int64_t cval;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (op != TOK_LE && op != TOK_GE && op != TOK_LT && op != TOK_GT)
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) { MCC_TRACE("br\n");
		k = l;
		c = r;
		keyleft = 1;
	} else if (ast_kind(a, l) == AST_Literal) { MCC_TRACE("br\n");
		k = r;
		c = l;
		keyleft = 0;
	} else { MCC_TRACE("br\n");
		return 0;
	}
	if (!ast_ident_cval(a, c, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, k, &kt, &kref) || !ast_ident_intt(kt) || (kt & VT_UNSIGNED))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, k))
		{ MCC_TRACE("br\n"); return 0; }
	cval = (int64_t)cv;
	eff = op;
	if (!keyleft)
		{ MCC_TRACE("br\n"); eff = op == TOK_LE ? TOK_GE : op == TOK_GE ? TOK_LE : op == TOK_LT ? TOK_GT : TOK_LT; }
	switch (eff) { MCC_TRACE("br\n");
	case TOK_LE:
		*is_lower = 0;
		*bound = cval;
		break;
	case TOK_LT:
		if (cval == INT64_MIN)
			{ MCC_TRACE("br\n"); return 0; }
		*is_lower = 0;
		*bound = cval - 1;
		break;
	case TOK_GE:
		*is_lower = 1;
		*bound = cval;
		break;
	case TOK_GT:
		if (cval == INT64_MAX)
			{ MCC_TRACE("br\n"); return 0; }
		*is_lower = 1;
		*bound = cval + 1;
		break;
	default:
		return 0;
	}
	*key = k;
	return 1;
}

#define AST_VLAT_CAP 2048
static const AstArena *ast_vlat_arena;
static uint64_t ast_vlat_epoch;
static int ast_vlat_state;
static int ast_vlat_n;
static int ast_vlat_off[AST_VLAT_CAP];
static AstVLat ast_vlat_slot[AST_VLAT_CAP];

static AstVLat *ast_vlat_find(int off, int create) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_vlat_n; i++)
		{ MCC_TRACE("br\n"); if (ast_vlat_off[i] == off)
			{ MCC_TRACE("br\n"); return &ast_vlat_slot[i]; } }
	if (!create || ast_vlat_n >= AST_VLAT_CAP)
		{ MCC_TRACE("br\n"); return NULL; }
	ast_vlat_off[ast_vlat_n] = off;
	ast_vlat_slot[ast_vlat_n] = ast_vlat_top();
	return &ast_vlat_slot[ast_vlat_n++];
}

static int ast_vlat_eq(const AstVLat *x, const AstVLat *y) { MCC_TRACE("enter\n");
	return x->state == y->state && x->lo == y->lo && x->hi == y->hi &&
				 x->kzero == y->kzero && x->kone == y->kone && x->tt == y->tt;
}

static AstVLat ast_vlat_type_full(int tt) { MCC_TRACE("enter\n");
	int w = ast_ii_width(tt);
	int64_t smax;
	if (w <= 0 || w > 8)
		{ MCC_TRACE("br\n"); return ast_vlat_bottom(); }
	if (w == 8)
		{ MCC_TRACE("br\n"); return ast_vlat_full_fact(INT64_MIN, INT64_MAX, tt); }
	if (tt & VT_UNSIGNED)
		{ MCC_TRACE("br\n"); return ast_vlat_full_fact(0, (int64_t)(((uint64_t)1 << (w * 8)) - 1), tt); }
	smax = ((int64_t)1 << (w * 8 - 1)) - 1;
	return ast_vlat_full_fact(-smax - 1, smax, tt);
}

static void ast_vlat_transfer(AstArena *a, AstLocal ifnode, int off, AstVLat *el) { MCC_TRACE("enter\n");
	AstLocal cond = ast_child(a, ifnode, 0);
	AstLocal parts[2];
	int nparts = 1, i;
	if (cond == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	parts[0] = cond;
	if (ast_kind(a, cond) == AST_Binary && ast_op(a, cond) == TOK_LAND &&
			ast_nchild(a, cond) == 2) { MCC_TRACE("br\n");
		parts[0] = ast_child(a, cond, 0);
		parts[1] = ast_child(a, cond, 1);
		nparts = 2;
	}
	for (i = 0; i < nparts; i++) { MCC_TRACE("br\n");
		AstLocal key;
		int64_t bound;
		int is_lower, r;
		if (!ast_range_bound(a, parts[i], &key, &bound, &is_lower))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_kind(a, key) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		r = ast_op(a, key);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		if ((int)(int64_t)ast_ival(a, key) != off)
			{ MCC_TRACE("br\n"); continue; }
		ast_vlat_refine_bound(el, bound, is_lower);
	}
}

static int ast_vlat_use_of(AstArena *a, AstLocal u, int *off, int *kt) { MCC_TRACE("enter\n");
	int r;
	uint64_t kref;
	if (ast_kind(a, u) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	r = ast_op(a, u);
	if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM) || !(r & VT_LVAL))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, u, kt, &kref) || !ast_ident_intt(*kt))
		{ MCC_TRACE("br\n"); return 0; }
	*off = (int)(int64_t)ast_ival(a, u);
	return ast_local_is_readonly(a, *off);
}

static AstVLat ast_vlat_element(AstArena *a, AstLocal u, int off, int kt) { MCC_TRACE("enter\n");
	AstVLat el = ast_vlat_type_full(kt);
	AstLocal child = u;
	AstLocal p = ast_parent(a, u);
	while (p != AST_NONE) { MCC_TRACE("br\n");
		if (ast_kind(a, p) == AST_If) { MCC_TRACE("br\n");
			int op = ast_op(a, p), apply = 0;
			if (op == 0)
				{ MCC_TRACE("br\n"); apply = child == ast_child(a, p, 1); }
			else if (op == 2 || op == 3 || op == 4)
				{ MCC_TRACE("br\n"); apply = 1; }
			if (apply)
				{ MCC_TRACE("br\n"); ast_vlat_transfer(a, p, off, &el); }
		}
		child = p;
		p = ast_parent(a, p);
	}
	return el;
}

#if MCC_DEV
static void ast_vlat_check_sound(const char *q, AstArena *a, int off,
																 const AstVLat *cached);
#endif

static int ast_vlat_pass(AstArena *a) { MCC_TRACE("enter\n");
	int changed = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal u = 0; u < nn; u++) { MCC_TRACE("br\n");
		int off, kt;
		AstVLat *slot, merged;
		if (!ast_vlat_use_of(a, u, &off, &kt))
			{ MCC_TRACE("br\n"); continue; }
		merged = ast_vlat_element(a, u, off, kt);
		slot = ast_vlat_find(off, 1);
		if (!slot) { MCC_TRACE("br\n");
			ast_vlat_state = -1;
			return 0;
		}
		merged = ast_vlat_meet(*slot, merged);
		if (!ast_vlat_eq(&merged, slot)) { MCC_TRACE("br\n");
			*slot = merged;
			changed = 1;
		}
	}
	return changed;
}

static int ast_vlat_build(AstArena *a) { MCC_TRACE("enter\n");
	int iters = 0, changed, bound = (int)ast_count(a) + 1;
	ast_vlat_n = 0;
	ast_vlat_state = 1;
	do { MCC_TRACE("br\n");
		changed = ast_vlat_pass(a);
		iters++;
	} while (changed && iters < bound && ast_vlat_state >= 0);
	if (ast_vlat_env)
		{ MCC_TRACE("br\n"); MCC_TRACE("vlat build slots=%d iters=%d\n", ast_vlat_n, iters); }
#if MCC_DEV
	for (int i = 0; i < ast_vlat_n; i++)
		{ MCC_TRACE("br\n"); ast_vlat_check_sound("build", a, ast_vlat_off[i], &ast_vlat_slot[i]); }
#endif
	return ast_vlat_n;
}

static void ast_vlat_sync(AstArena *a) { MCC_TRACE("enter\n");
	if (ast_vlat_state && ast_vlat_arena == a && ast_vlat_epoch == a->epoch)
		{ MCC_TRACE("br\n"); return; }
	ast_vlat_arena = a;
	ast_vlat_epoch = a->epoch;
	ast_vlat_build(a);
}

static void ast_vlat_invalidate(const AstArena *a) { MCC_TRACE("enter\n");
	if (ast_vlat_arena == a) { MCC_TRACE("br\n");
		ast_vlat_arena = NULL;
		ast_vlat_state = 0;
	}
}

#if MCC_DEV
static void ast_vlat_diverge(const char *q, int off, int64_t clo, int64_t chi) { MCC_TRACE("enter\n");
	fprintf(stderr,
					"mcc: AST side-car divergence: %s(off=%d) cached=[%lld,%lld]\n", q,
					off, (long long)clo, (long long)chi);
	abort();
}

static AstVLat ast_vlat_recompute(AstArena *a, int off) { MCC_TRACE("enter\n");
	AstVLat acc = ast_vlat_top();
	AstLocal nn = ast_count(a);
	for (AstLocal u = 0; u < nn; u++) { MCC_TRACE("br\n");
		int uoff, kt;
		if (!ast_vlat_use_of(a, u, &uoff, &kt) || uoff != off)
			{ MCC_TRACE("br\n"); continue; }
		acc = ast_vlat_meet(acc, ast_vlat_element(a, u, off, kt));
	}
	return acc;
}

static void ast_vlat_check_sound(const char *q, AstArena *a, int off,
																 const AstVLat *cached) { MCC_TRACE("enter\n");
	AstVLat fresh;
	if (cached->state != AST_VLAT_FACT)
		{ MCC_TRACE("br\n"); return; }
	fresh = ast_vlat_recompute(a, off);
	if (fresh.state != AST_VLAT_FACT)
		{ MCC_TRACE("br\n"); return; }
	if (cached->lo > fresh.lo || cached->hi < fresh.hi ||
			(cached->kzero & ~fresh.kzero) || (cached->kone & ~fresh.kone))
		{ MCC_TRACE("br\n"); ast_vlat_diverge(q, off, cached->lo, cached->hi); }
}
#endif

static int ast_vlat_narrowing(AstArena *a, int off, int width_tt) { MCC_TRACE("enter\n");
	AstVLat *v;
	if (!ast_vlat_env)
		{ MCC_TRACE("br\n"); return 0; }
	ast_vlat_sync(a);
	if (ast_vlat_state < 0)
		{ MCC_TRACE("br\n"); return 0; }
	v = ast_vlat_find(off, 0);
	if (!v)
		{ MCC_TRACE("br\n"); return 0; }
#if MCC_DEV
	ast_vlat_check_sound("narrowing", a, off, v);
#endif
	return ast_vlat_fits_bytes(v, ast_ii_width(width_tt));
}

static int ast_vlat_context(AstArena *a, int off, AstVLat *out) { MCC_TRACE("enter\n");
	AstVLat *v;
	if (!ast_vlat_env)
		{ MCC_TRACE("br\n"); return 0; }
	ast_vlat_sync(a);
	if (ast_vlat_state < 0)
		{ MCC_TRACE("br\n"); return 0; }
	v = ast_vlat_find(off, 0);
	if (!v || v->state != AST_VLAT_FACT)
		{ MCC_TRACE("br\n"); return 0; }
#if MCC_DEV
	ast_vlat_check_sound("context", a, off, v);
#endif
	*out = *v;
	return 1;
}

static int ast_vlat_context_at(AstArena *a, AstLocal use, AstVLat *out) { MCC_TRACE("enter\n");
	int off, kt;
	AstVLat el;
	if (!ast_vlat_env)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_vlat_use_of(a, use, &off, &kt))
		{ MCC_TRACE("br\n"); return 0; }
	el = ast_vlat_element(a, use, off, kt);
	if (el.state != AST_VLAT_FACT)
		{ MCC_TRACE("br\n"); return 0; }
#if MCC_DEV
	ast_vlat_sync(a);
	if (ast_vlat_state >= 0) { MCC_TRACE("br\n");
		AstVLat *whole = ast_vlat_find(off, 0);
		if (whole && whole->state == AST_VLAT_FACT &&
				(el.lo < whole->lo || el.hi > whole->hi ||
				 (whole->kzero & ~el.kzero) || (whole->kone & ~el.kone)))
			{ MCC_TRACE("br\n"); ast_vlat_diverge("context_at", off, el.lo, el.hi); }
	}
#endif
	*out = el;
	return 1;
}

static int ast_range_try(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal c0 = ast_child(a, n, 0), c1 = ast_child(a, n, 1), k0, k1, key, kexpr, hlit;
	int64_t b0, b1, lo, hi;
	int low0, low1, kt, kw;
	uint64_t kref, span;
	if (!ast_range_bound(a, c0, &k0, &b0, &low0) ||
			!ast_range_bound(a, c1, &k1, &b1, &low1))
		{ MCC_TRACE("br\n"); return 0; }
	if (low0 == low1 || !ast_ident_same(a, k0, k1))
		{ MCC_TRACE("br\n"); return 0; }
	key = k0;
	if (low0) { MCC_TRACE("br\n");
		lo = b0;
		hi = b1;
	} else { MCC_TRACE("br\n");
		lo = b1;
		hi = b0;
	}
	if (lo > hi)
		{ MCC_TRACE("br\n"); return 0; }
	ast_ident_etype(a, key, &kt, &kref);
	kw = (kt & VT_BTYPE) == VT_LLONG ? VT_LLONG | VT_UNSIGNED : VT_INT | VT_UNSIGNED;
	span = (uint64_t)hi - (uint64_t)lo;
	kexpr = ast_bf_keyexpr(a, kw, key, (uint64_t)lo);
	hlit = ast_bf_lit(a, kw, span);
	MCC_TRACE("range fold key=%u lo=%lld hi=%lld span=%llu kw=0x%x\n", (unsigned)key,
						(long long)lo, (long long)hi, (unsigned long long)span, kw);
	ast_set_op(a, n, (unsigned)TOK_ULE ^ ((ast_fbits(a, n) & AST_FB_LANDOR_INVERT) ? 1u : 0u));
	ast_set_type(a, n, VT_INT, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, kexpr);
	ast_add_child(a, n, hlit);
	return 1;
}

static int ast_range_bound_or(AstArena *a, AstLocal n, AstLocal *key, int64_t *bound,
															int *is_below) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), eff, ct, kt, keyleft;
	AstLocal l, r, k, c;
	uint64_t cv, kref;
	int64_t cval;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (op != TOK_LE && op != TOK_GE && op != TOK_LT && op != TOK_GT)
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) { MCC_TRACE("br\n");
		k = l;
		c = r;
		keyleft = 1;
	} else if (ast_kind(a, l) == AST_Literal) { MCC_TRACE("br\n");
		k = r;
		c = l;
		keyleft = 0;
	} else { MCC_TRACE("br\n");
		return 0;
	}
	if (!ast_ident_cval(a, c, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, k, &kt, &kref) || !ast_ident_intt(kt) || (kt & VT_UNSIGNED))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, k))
		{ MCC_TRACE("br\n"); return 0; }
	cval = (int64_t)cv;
	eff = op;
	if (!keyleft)
		{ MCC_TRACE("br\n"); eff = op == TOK_LE ? TOK_GE : op == TOK_GE ? TOK_LE : op == TOK_LT ? TOK_GT : TOK_LT; }
	switch (eff) { MCC_TRACE("br\n");
	case TOK_LT:
		*is_below = 1;
		*bound = cval;
		break;
	case TOK_LE:
		if (cval == INT64_MAX)
			{ MCC_TRACE("br\n"); return 0; }
		*is_below = 1;
		*bound = cval + 1;
		break;
	case TOK_GT:
		*is_below = 0;
		*bound = cval;
		break;
	case TOK_GE:
		if (cval == INT64_MIN)
			{ MCC_TRACE("br\n"); return 0; }
		*is_below = 0;
		*bound = cval - 1;
		break;
	default:
		return 0;
	}
	*key = k;
	return 1;
}

static int ast_range_try_lor(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal c0 = ast_child(a, n, 0), c1 = ast_child(a, n, 1), k0, k1, key, kexpr, hlit;
	int64_t b0, b1, lo, hi;
	int bel0, bel1, kt, kw;
	uint64_t kref, span;
	if (!ast_range_bound_or(a, c0, &k0, &b0, &bel0) ||
			!ast_range_bound_or(a, c1, &k1, &b1, &bel1))
		{ MCC_TRACE("br\n"); return 0; }
	if (bel0 == bel1 || !ast_ident_same(a, k0, k1))
		{ MCC_TRACE("br\n"); return 0; }
	key = k0;
	if (bel0) { MCC_TRACE("br\n");
		lo = b0;
		hi = b1;
	} else { MCC_TRACE("br\n");
		lo = b1;
		hi = b0;
	}
	if (lo > hi)
		{ MCC_TRACE("br\n"); return 0; }
	ast_ident_etype(a, key, &kt, &kref);
	kw = (kt & VT_BTYPE) == VT_LLONG ? VT_LLONG | VT_UNSIGNED : VT_INT | VT_UNSIGNED;
	span = (uint64_t)hi - (uint64_t)lo;
	kexpr = ast_bf_keyexpr(a, kw, key, (uint64_t)lo);
	hlit = ast_bf_lit(a, kw, span);
	MCC_TRACE("range fold(or) key=%u lo=%lld hi=%lld span=%llu kw=0x%x\n", (unsigned)key,
						(long long)lo, (long long)hi, (unsigned long long)span, kw);
	ast_set_op(a, n, (unsigned)TOK_UGT ^ ((ast_fbits(a, n) & AST_FB_LANDOR_INVERT) ? 1u : 0u));
	ast_set_type(a, n, VT_INT, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, kexpr);
	ast_add_child(a, n, hlit);
	return 1;
}

static int ast_range_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	ast_range_folds = 0;
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LAND &&
				ast_nchild(a, n) == 2)
			{ MCC_TRACE("br\n"); ast_range_folds += ast_range_try(a, n); } }
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LOR &&
				ast_nchild(a, n) == 2)
			{ MCC_TRACE("br\n"); ast_range_folds += ast_range_try_lor(a, n); } }
	return ast_range_folds;
}

static MCC_OPT_TLS int ast_divmagic_folds;

#define AST_DIVMAGIC_DUP_MAX 8

static MCC_OPT_TLS const AstArena *ast_divmagic_base_arena;
static MCC_OPT_TLS AstLocal ast_divmagic_base;

static void ast_divmagic_invalidate(const AstArena *a) { MCC_TRACE("enter\n");
	if (ast_divmagic_base_arena == a) { MCC_TRACE("br\n");
		ast_divmagic_base_arena = NULL;
		ast_divmagic_base = 0;
	}
}

static int ast_divmagic_dup_budget(AstArena *a, AstLocal n, int budget) { MCC_TRACE("enter\n");
	if (budget < 0)
		{ MCC_TRACE("br\n"); return budget; }
	budget--;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); budget = ast_divmagic_dup_budget(a, c, budget); }
	return budget;
}

static int ast_divmagic_lowered(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n >= ast_divmagic_base)
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_divmagic_lowered(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_divmagic_dup_ok(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (ast_divmagic_dup_budget(a, n, AST_DIVMAGIC_DUP_MAX) >= 0)
		{ MCC_TRACE("br\n"); return 1; }
	return !ast_divmagic_lowered(a, n);
}

#ifdef MCC_TARGET_I386
static int ast_divmagic_materialize(AstArena *a, AstLocal n, AstLocal x, int xt,
																		uint64_t xref, int *off_out);
#endif

static int ast_divmagic_try(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), nt, ct, xt;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1), xu, prod, hi, hi32;
	uint64_t nref, xref, cv;
	uint32_t C;
	MccMagicU mag;
	const int U64 = VT_LLONG | VT_UNSIGNED, U32 = VT_INT | VT_UNSIGNED;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != U32)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	C = (uint32_t)cv;
	if (C < 3 || (C & (C - 1)) == 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != U32)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	mag = mcc_magicu(C);
	if (mag.a && mag.s < 1)
		{ MCC_TRACE("br\n"); return 0; }
#ifdef MCC_TARGET_I386
	int xoff;
	if (!ast_divmagic_materialize(a, n, x, xt, xref, &xoff))
		{ MCC_TRACE("br\n"); return 0; }
#define DMX() ast_bf_localref(a, xoff, xt, xref)
#else
	if (!ast_divmagic_dup_ok(a, x))
		{ MCC_TRACE("br\n"); return 0; }
#define DMX() ast_dup_sub(a, x)
#endif
	{
		AstLocal inner;
		uint64_t shamt;
#ifdef MCC_TARGET_I386
		xu = ast_bf_ucast(a, U64, ast_bf_localref(a, xoff, xt, xref));
#else
		xu = ast_bf_ucast(a, U64, x);
#endif
		prod = ast_bf_bin(a, '*', U64, xu, ast_bf_lit(a, U64, mag.M));
		hi = ast_bf_bin(a, TOK_SHR, U64, prod, ast_bf_lit(a, U64, 32));
		hi32 = ast_node(a, AST_Convert);
		ast_set_type(a, hi32, U32, 0);
		ast_add_child(a, hi32, hi);
		if (!mag.a) { MCC_TRACE("br\n");
			inner = hi32;
			shamt = (uint64_t)mag.s;
		} else { MCC_TRACE("br\n");
			AstLocal sub = ast_bf_bin(a, '-', U32, DMX(), hi32);
			AstLocal shr1 = ast_bf_bin(a, TOK_SHR, U32, sub, ast_bf_lit(a, U32, 1));
			inner = ast_bf_bin(a, '+', U32, shr1, ast_dup_sub(a, hi32));
			shamt = (uint64_t)(mag.s - 1);
		}
		MCC_TRACE("divmagic %s C=%u M=0x%x s=%d add=%d\n", op == '/' ? "div" : "rem", C, mag.M,
							mag.s, mag.a);
		ast_set_type(a, n, U32, 0);
		ast_set_ival(a, n, 0);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_clear_children(a, n);
		if (op == '/') { MCC_TRACE("br\n");
			ast_set_op(a, n, TOK_SHR);
			ast_add_child(a, n, inner);
			ast_add_child(a, n, ast_bf_lit(a, U32, shamt));
		} else { MCC_TRACE("br\n");
			AstLocal q = ast_bf_bin(a, TOK_SHR, U32, inner, ast_bf_lit(a, U32, shamt));
			AstLocal qC = ast_bf_bin(a, '*', U32, q, ast_bf_lit(a, U32, C));
			ast_set_op(a, n, '-');
			ast_add_child(a, n, DMX());
			ast_add_child(a, n, qC);
		}
	}
#undef DMX
	return 1;
}

static int ast_divmagic_try_spow2(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), nt, ct, xt, k, neg;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1), bias, sum;
	uint64_t nref, xref, cv, t, ac;
	int64_t C;
	const int S32 = VT_INT;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != VT_INT)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	C = (int64_t)cv;
	neg = C < 0;
	ac = (uint64_t)(neg ? -C : C);
	if (ac < 2 || (ac & (ac - 1)) != 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != VT_INT)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_divmagic_dup_ok(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	for (k = 0, t = ac; t > 1; t >>= 1)
		{ MCC_TRACE("br\n"); k++; }
	bias = ast_bf_bin(a, '&', S32,
										ast_bf_bin(a, TOK_SAR, S32, ast_dup_sub(a, x), ast_bf_lit(a, S32, 31)),
										ast_bf_lit(a, S32, ac - 1));
	sum = ast_bf_bin(a, '+', S32, ast_dup_sub(a, x), bias);
	MCC_TRACE("divmagic spow2 %s C=%lld k=%d neg=%d\n", op == '/' ? "div" : "rem", (long long)C,
						k, neg);
	if (op == '/') { MCC_TRACE("br\n");
		ast_set_type(a, n, S32, 0);
		ast_set_ival(a, n, 0);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_clear_children(a, n);
		if (neg) { MCC_TRACE("br\n");
			AstLocal q = ast_bf_bin(a, TOK_SAR, S32, sum, ast_bf_lit(a, S32, (uint64_t)k));
			ast_set_op(a, n, '-');
			ast_add_child(a, n, ast_bf_lit(a, S32, 0));
			ast_add_child(a, n, q);
		} else { MCC_TRACE("br\n");
			ast_set_op(a, n, TOK_SAR);
			ast_add_child(a, n, sum);
			ast_add_child(a, n, ast_bf_lit(a, S32, (uint64_t)k));
		}
	} else { MCC_TRACE("br\n");
		AstLocal quot = ast_bf_bin(a, TOK_SAR, S32, sum, ast_bf_lit(a, S32, (uint64_t)k));
		AstLocal shl = ast_bf_bin(a, TOK_SHL, S32, quot, ast_bf_lit(a, S32, (uint64_t)k));
		AstLocal xdup = ast_dup_sub(a, x);
		ast_set_op(a, n, '-');
		ast_set_type(a, n, S32, 0);
		ast_set_ival(a, n, 0);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_clear_children(a, n);
		ast_add_child(a, n, xdup);
		ast_add_child(a, n, shl);
	}
	return 1;
}

#ifdef MCC_TARGET_I386
static int ast_ltemp_insert_before(AstArena *a, AstLocal parent, AstLocal pivot,
																	 AstLocal node);

static int ast_divmagic_materialize(AstArena *a, AstLocal n, AstLocal x, int xt,
																		uint64_t xref, int *off_out) { MCC_TRACE("enter\n");
	if (ast_ltemp_n >= AST_LTEMP_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal stmt = n, bb = ast_parent(a, n);
	while (bb != AST_NONE && ast_kind(a, bb) != AST_BasicBlock) { MCC_TRACE("br\n");
		stmt = bb;
		bb = ast_parent(a, stmt);
	}
	if (bb == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	int off = (ast_ltemp_cur - 8) & -8;
	AstLocal sref = ast_bf_localref(a, off, xt, xref);
	AstLocal st = ast_node(a, AST_Store);
	ast_add_child(a, st, sref);
	ast_add_child(a, st, ast_dup_sub(a, x));
	if (!ast_ltemp_insert_before(a, bb, stmt, st))
		{ MCC_TRACE("br\n"); return 0; }
	ast_ltemp_cur = off;
	ast_ltemp_add(off, 8);
	*off_out = off;
	return 1;
}
#endif

static int ast_divmagic_try_signed(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), nt, ct, xt;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1);
	AstLocal Mi, xi, prod, hi, q0, q1, q2, cvt, signbit;
	uint64_t nref, xref, cv, ac;
	int64_t C;
	MccMagicS mag;
	const int S64 = VT_LLONG, S32 = VT_INT, U32 = VT_INT | VT_UNSIGNED;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != VT_INT)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	C = (int64_t)cv;
	if (C >= -1 && C <= 1)
		{ MCC_TRACE("br\n"); return 0; }
	ac = (uint64_t)(C < 0 ? -C : C);
	if ((ac & (ac - 1)) == 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != VT_INT)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
#ifdef MCC_TARGET_I386
	int xoff;
	if (!ast_divmagic_materialize(a, n, x, xt, xref, &xoff))
		{ MCC_TRACE("br\n"); return 0; }
#define DMX() ast_bf_localref(a, xoff, xt, xref)
#else
	if (!ast_divmagic_dup_ok(a, x))
		{ MCC_TRACE("br\n"); return 0; }
#define DMX() ast_dup_sub(a, x)
#endif
	mag = mcc_magics((int32_t)C);
	Mi = ast_bf_lit(a, S64, (uint64_t)(int64_t)mag.M);
#ifdef MCC_TARGET_I386
	xi = ast_bf_ucast(a, S64, ast_bf_localref(a, xoff, xt, xref));
#else
	xi = ast_bf_ucast(a, S64, x);
#endif
	prod = ast_bf_bin(a, '*', S64, Mi, xi);
	hi = ast_bf_bin(a, TOK_SAR, S64, prod, ast_bf_lit(a, S64, 32));
	q0 = ast_node(a, AST_Convert);
	ast_set_type(a, q0, S32, 0);
	ast_add_child(a, q0, hi);
	if (C > 0 && mag.M < 0)
		{ MCC_TRACE("br\n"); q1 = ast_bf_bin(a, '+', S32, q0, DMX()); }
	else if (C < 0 && mag.M > 0)
		{ MCC_TRACE("br\n"); q1 = ast_bf_bin(a, '-', S32, q0, DMX()); }
	else
		{ MCC_TRACE("br\n"); q1 = q0; }
	q2 = ast_bf_bin(a, TOK_SAR, S32, q1, ast_bf_lit(a, S32, (uint64_t)mag.s));
	cvt = ast_node(a, AST_Convert);
	ast_set_type(a, cvt, U32, 0);
	ast_add_child(a, cvt, ast_dup_sub(a, q2));
	signbit = ast_bf_bin(a, TOK_SHR, U32, cvt, ast_bf_lit(a, U32, 31));
	{
		AstLocal sbs = ast_node(a, AST_Convert);
		ast_set_type(a, sbs, S32, 0);
		ast_add_child(a, sbs, signbit);
		signbit = sbs;
	}
	MCC_TRACE("divmagic signed %s C=%lld M=0x%x s=%d\n", op == '/' ? "div" : "rem",
						(long long)C, (unsigned)mag.M, mag.s);
	ast_set_type(a, n, S32, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	if (op == '/') { MCC_TRACE("br\n");
		ast_set_op(a, n, '+');
		ast_add_child(a, n, q2);
		ast_add_child(a, n, signbit);
	} else { MCC_TRACE("br\n");
		AstLocal qexpr = ast_bf_bin(a, '+', S32, q2, signbit);
		AstLocal qC = ast_bf_bin(a, '*', S32, qexpr, ast_bf_lit(a, S32, (uint64_t)(uint32_t)C));
		ast_set_op(a, n, '-');
		ast_add_child(a, n, DMX());
		ast_add_child(a, n, qC);
	}
#undef DMX
	return 1;
}

#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_I386)
static int ast_divmagic_try_u64(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), nt, ct, xt;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1), hi, inner;
	uint64_t nref, xref, cv, shamt;
	uint64_t C;
	MccMagicU64 mag;
	const int U64 = VT_LLONG | VT_UNSIGNED;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != U64)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	C = cv;
	if (C < 3 || (C & (C - 1)) == 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != U64)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	mag = mcc_magicu64(C);
	if (mag.a && mag.s < 1)
		{ MCC_TRACE("br\n"); return 0; }
#ifdef MCC_TARGET_I386
	int xoff;
	if (!ast_divmagic_materialize(a, n, x, xt, xref, &xoff))
		{ MCC_TRACE("br\n"); return 0; }
#define DMX() ast_bf_localref(a, xoff, xt, xref)
#else
	if (!ast_divmagic_dup_ok(a, x))
		{ MCC_TRACE("br\n"); return 0; }
#define DMX() ast_dup_sub(a, x)
#endif
	hi = ast_bf_bin(a, AST_OP_MULHU, U64, DMX(), ast_bf_lit(a, U64, mag.M));
	if (!mag.a) { MCC_TRACE("br\n");
		inner = hi;
		shamt = (uint64_t)mag.s;
	} else { MCC_TRACE("br\n");
		AstLocal sub = ast_bf_bin(a, '-', U64, DMX(), hi);
		AstLocal shr1 = ast_bf_bin(a, TOK_SHR, U64, sub, ast_bf_lit(a, U64, 1));
		inner = ast_bf_bin(a, '+', U64, shr1, ast_dup_sub(a, hi));
		shamt = (uint64_t)(mag.s - 1);
	}
	MCC_TRACE("divmagic u64 %s C=%llu M=0x%llx s=%d add=%d\n", op == '/' ? "div" : "rem",
						(unsigned long long)C, (unsigned long long)mag.M, mag.s, mag.a);
	ast_set_type(a, n, U64, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	if (op == '/') { MCC_TRACE("br\n");
		ast_set_op(a, n, TOK_SHR);
		ast_add_child(a, n, inner);
		ast_add_child(a, n, ast_bf_lit(a, U64, shamt));
	} else { MCC_TRACE("br\n");
		AstLocal q = ast_bf_bin(a, TOK_SHR, U64, inner, ast_bf_lit(a, U64, shamt));
		AstLocal qC = ast_bf_bin(a, '*', U64, q, ast_bf_lit(a, U64, C));
		ast_set_op(a, n, '-');
		ast_add_child(a, n, DMX());
		ast_add_child(a, n, qC);
	}
#undef DMX
	return 1;
}

static int ast_divmagic_try_s64_pow2(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), nt, ct, xt, k, neg;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1), bias, sum;
	uint64_t nref, xref, cv, t, ac;
	int64_t C;
	const int S64 = VT_LLONG;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != VT_LLONG)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	C = (int64_t)cv;
	neg = C < 0;
	ac = (uint64_t)(neg ? -C : C);
	if (ac < 2 || (ac & (ac - 1)) != 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != VT_LLONG)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_divmagic_dup_ok(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	for (k = 0, t = ac; t > 1; t >>= 1)
		{ MCC_TRACE("br\n"); k++; }
	bias = ast_bf_bin(a, '&', S64,
										ast_bf_bin(a, TOK_SAR, S64, ast_dup_sub(a, x), ast_bf_lit(a, S64, 63)),
										ast_bf_lit(a, S64, ac - 1));
	sum = ast_bf_bin(a, '+', S64, ast_dup_sub(a, x), bias);
	MCC_TRACE("divmagic s64 spow2 %s C=%lld k=%d neg=%d\n", op == '/' ? "div" : "rem",
						(long long)C, k, neg);
	if (op == '/') { MCC_TRACE("br\n");
		ast_set_type(a, n, S64, 0);
		ast_set_ival(a, n, 0);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_clear_children(a, n);
		if (neg) { MCC_TRACE("br\n");
			AstLocal q = ast_bf_bin(a, TOK_SAR, S64, sum, ast_bf_lit(a, S64, (uint64_t)k));
			ast_set_op(a, n, '-');
			ast_add_child(a, n, ast_bf_lit(a, S64, 0));
			ast_add_child(a, n, q);
		} else { MCC_TRACE("br\n");
			ast_set_op(a, n, TOK_SAR);
			ast_add_child(a, n, sum);
			ast_add_child(a, n, ast_bf_lit(a, S64, (uint64_t)k));
		}
	} else { MCC_TRACE("br\n");
		AstLocal quot = ast_bf_bin(a, TOK_SAR, S64, sum, ast_bf_lit(a, S64, (uint64_t)k));
		AstLocal shl = ast_bf_bin(a, TOK_SHL, S64, quot, ast_bf_lit(a, S64, (uint64_t)k));
		AstLocal xdup = ast_dup_sub(a, x);
		ast_set_op(a, n, '-');
		ast_set_type(a, n, S64, 0);
		ast_set_ival(a, n, 0);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_clear_children(a, n);
		ast_add_child(a, n, xdup);
		ast_add_child(a, n, shl);
	}
	return 1;
}

static int ast_divmagic_try_s64(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), nt, ct, xt;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1);
	AstLocal q0, q1, q2, cvt, signbit;
	uint64_t nref, xref, cv, ac;
	int64_t C;
	MccMagicS64 mag;
	const int S64 = VT_LLONG, U64 = VT_LLONG | VT_UNSIGNED;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != VT_LLONG)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		{ MCC_TRACE("br\n"); return 0; }
	C = (int64_t)cv;
	if (C >= -1 && C <= 1)
		{ MCC_TRACE("br\n"); return 0; }
	ac = (uint64_t)(C < 0 ? -C : C);
	if ((ac & (ac - 1)) == 0)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != VT_LLONG)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	mag = mcc_magics64(C);
#ifdef MCC_TARGET_I386
	int xoff;
	if (!ast_divmagic_materialize(a, n, x, xt, xref, &xoff))
		{ MCC_TRACE("br\n"); return 0; }
#define DMX() ast_bf_localref(a, xoff, xt, xref)
#else
	if (!ast_divmagic_dup_ok(a, x))
		{ MCC_TRACE("br\n"); return 0; }
#define DMX() ast_dup_sub(a, x)
#endif
	q0 = ast_bf_bin(a, AST_OP_MULHS, S64, ast_bf_lit(a, S64, (uint64_t)mag.M), DMX());
	if (C > 0 && mag.M < 0)
		{ MCC_TRACE("br\n"); q1 = ast_bf_bin(a, '+', S64, q0, DMX()); }
	else if (C < 0 && mag.M > 0)
		{ MCC_TRACE("br\n"); q1 = ast_bf_bin(a, '-', S64, q0, DMX()); }
	else
		{ MCC_TRACE("br\n"); q1 = q0; }
	q2 = ast_bf_bin(a, TOK_SAR, S64, q1, ast_bf_lit(a, S64, (uint64_t)mag.s));
	cvt = ast_node(a, AST_Convert);
	ast_set_type(a, cvt, U64, 0);
	ast_add_child(a, cvt, ast_dup_sub(a, q2));
	signbit = ast_bf_bin(a, TOK_SHR, U64, cvt, ast_bf_lit(a, U64, 63));
	{
		AstLocal sbs = ast_node(a, AST_Convert);
		ast_set_type(a, sbs, S64, 0);
		ast_add_child(a, sbs, signbit);
		signbit = sbs;
	}
	MCC_TRACE("divmagic s64 %s C=%lld M=0x%llx s=%d\n", op == '/' ? "div" : "rem",
						(long long)C, (unsigned long long)mag.M, mag.s);
	ast_set_type(a, n, S64, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	if (op == '/') { MCC_TRACE("br\n");
		ast_set_op(a, n, '+');
		ast_add_child(a, n, q2);
		ast_add_child(a, n, signbit);
	} else { MCC_TRACE("br\n");
		AstLocal qexpr = ast_bf_bin(a, '+', S64, q2, signbit);
		AstLocal qC = ast_bf_bin(a, '*', S64, qexpr, ast_bf_lit(a, S64, (uint64_t)C));
		ast_set_op(a, n, '-');
		ast_add_child(a, n, DMX());
		ast_add_child(a, n, qC);
	}
#undef DMX
	return 1;
}
#endif

static int ast_divmagic_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	ast_divmagic_folds = 0;
	if (ast_divmagic_base_arena != a || ast_divmagic_base > nn) { MCC_TRACE("br\n");
		ast_divmagic_base_arena = a;
		ast_divmagic_base = nn;
	}
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_Binary && (ast_op(a, n) == '/' || ast_op(a, n) == '%') &&
				ast_nchild(a, n) == 2) { MCC_TRACE("br\n");
			int f = ast_divmagic_try(a, n);
			if (!f)
				{ MCC_TRACE("br\n"); f = ast_divmagic_try_spow2(a, n); }
			if (!f)
				{ MCC_TRACE("br\n"); f = ast_divmagic_try_signed(a, n); }
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_I386)
			if (!f)
				{ MCC_TRACE("br\n"); f = ast_divmagic_try_u64(a, n); }
			if (!f)
				{ MCC_TRACE("br\n"); f = ast_divmagic_try_s64_pow2(a, n); }
			if (!f)
				{ MCC_TRACE("br\n"); f = ast_divmagic_try_s64(a, n); }
#endif
			ast_divmagic_folds += f;
		} }
	return ast_divmagic_folds;
}

static int ast_divrem_div_op(int op) { MCC_TRACE("enter\n");
	if (op == '%')
		{ MCC_TRACE("br\n"); return '/'; }
	if (op == TOK_UMOD)
		{ MCC_TRACE("br\n"); return TOK_UDIV; }
	return 0;
}

static int ast_divrem_rewrite(AstArena *a, AstLocal n, AstLocal da, AstLocal db,
															int divop, int toff, AstLocal qref) { MCC_TRACE("enter\n");
	int hits = 0;
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Binary && ast_nchild(a, n) == 2 &&
			ast_divrem_div_op(ast_op(a, n)) == divop) { MCC_TRACE("br\n");
		AstLocal ma = ast_child(a, n, 0), mb = ast_child(a, n, 1);
		int t;
		uint64_t r;
		if (ast_kind(a, mb) != AST_Literal && ast_ident_same(a, ma, da) &&
				ast_ident_same(a, mb, db) && ast_ident_pure(a, ma) && ast_ident_pure(a, mb) &&
				!ast_tco_reads_off(a, da, toff) && !ast_tco_reads_off(a, db, toff) &&
				ast_ident_etype(a, n, &t, &r)) { MCC_TRACE("br\n");
			AstLocal qb = ast_bf_bin(a, '*', t, ast_dup_sub(a, qref), ast_dup_sub(a, mb));
			AstLocal keepa = ast_dup_sub(a, ma);
			ast_set_op(a, n, '-');
			ast_set_ival(a, n, 0);
			ast_set_fbits(a, n, 0);
			ast_set_sym(a, n, 0);
			ast_clear_children(a, n);
			ast_add_child(a, n, keepa);
			ast_add_child(a, n, qb);
			return 1;
		}
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); hits += ast_divrem_rewrite(a, c, da, db, divop, toff, qref); }
	return hits;
}

static int ast_divrem_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	int folds = 0;
	for (AstLocal bb = 0; bb < nn; bb++) { MCC_TRACE("br\n");
		if (ast_kind(a, bb) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal prev = AST_NONE;
		for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
			if (prev != AST_NONE && ast_kind(a, prev) == AST_Store) { MCC_TRACE("br\n");
				AstLocal lval = ast_child(a, prev, 0), val = ast_child(a, prev, 1);
				int toff, tt;
				if (val != AST_NONE && ast_kind(a, val) == AST_Binary && ast_nchild(a, val) == 2 &&
						(ast_op(a, val) == '/' || ast_op(a, val) == TOK_UDIV) &&
						ast_cse_is_local(a, lval, &toff, &tt)) { MCC_TRACE("br\n");
					AstLocal da = ast_child(a, val, 0), db = ast_child(a, val, 1);
					if (ast_kind(a, db) != AST_Literal)
						{ MCC_TRACE("br\n"); folds += ast_divrem_rewrite(a, s, da, db, ast_op(a, val), toff, lval); }
				}
			}
			prev = s;
		}
	}
	ast_divrem_folds = folds;
	return folds;
}

static int ast_slp_mem(AstArena *a, AstLocal m, AstLocal *base, int64_t *idx,
											 int *et, uint64_t *eref) { MCC_TRACE("enter\n");
	if (ast_kind(a, m) != AST_Load || ast_nchild(a, m) != 1)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal addr = ast_child(a, m, 0);
	if (ast_kind(a, addr) != AST_Binary || ast_op(a, addr) != '+' ||
			ast_nchild(a, addr) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal b = ast_child(a, addr, 0), ix = ast_child(a, addr, 1);
	int it, t;
	uint64_t iv, tref;
	if (!ast_ident_cval(a, ix, &it, &iv))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, b) != AST_Ref || !(ast_type_t(a, b) & VT_ARRAY))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, m, &t, &tref) ||
			((t & VT_BTYPE) != VT_FLOAT && (t & VT_BTYPE) != VT_DOUBLE))
		{ MCC_TRACE("br\n"); return 0; }
	*base = b;
	*idx = (int64_t)iv;
	*et = t;
	*eref = tref;
	return 1;
}

static AstLocal ast_slp_vload(AstArena *a, CType *V, CType *PV, AstLocal addr) { MCC_TRACE("enter\n");
	AstLocal conv = ast_node(a, AST_Convert);
	ast_set_type(a, conv, PV->t, (uint64_t)(uintptr_t)PV->ref);
	ast_add_child(a, conv, addr);
	AstLocal ld = ast_node(a, AST_Load);
	ast_set_type(a, ld, V->t, (uint64_t)(uintptr_t)V->ref);
	ast_add_child(a, ld, conv);
	return ld;
}

static int ast_slp_try(AstArena *a, AstLocal s0) { MCC_TRACE("enter\n");
	AstLocal lval0, val0, a0, b0, baseC, baseA, baseB;
	int64_t idxC, idxA, idxB;
	int etC, etA, etB, op, esz, lanes, j;
	uint64_t erC, erA, erB;
	AstLocal stmts[16];
	if (ast_kind(a, s0) != AST_Store || ast_nchild(a, s0) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	lval0 = ast_child(a, s0, 0);
	val0 = ast_child(a, s0, 1);
	if (ast_kind(a, val0) != AST_Binary || ast_nchild(a, val0) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	op = ast_op(a, val0);
	if (op != '+' && op != '-' && op != '*' && op != '/')
		{ MCC_TRACE("br\n"); return 0; }
	a0 = ast_child(a, val0, 0);
	b0 = ast_child(a, val0, 1);
	if (!ast_slp_mem(a, lval0, &baseC, &idxC, &etC, &erC) ||
			!ast_slp_mem(a, a0, &baseA, &idxA, &etA, &erA) ||
			!ast_slp_mem(a, b0, &baseB, &idxB, &etB, &erB))
		{ MCC_TRACE("br\n"); return 0; }
	if (etC != etA || etC != etB || idxA != idxC || idxB != idxC)
		{ MCC_TRACE("br\n"); return 0; }
	esz = (etC & VT_BTYPE) == VT_FLOAT ? 4 : (etC & VT_BTYPE) == VT_DOUBLE ? 8 : 0;
	if (!esz)
		{ MCC_TRACE("br\n"); return 0; }
	lanes = 16 / esz;
	stmts[0] = s0;
	{
		AstLocal s = ast_next_sib(a, s0);
		for (j = 1; j < lanes; j++) { MCC_TRACE("br\n");
			AstLocal lv, vv, aj, bj, bC, bA, bB;
			int64_t iC, iA, iB;
			int tC, tA, tB;
			uint64_t rC, rA, rB;
			if (s == AST_NONE || ast_kind(a, s) != AST_Store || ast_nchild(a, s) != 2)
				{ MCC_TRACE("br\n"); return 0; }
			lv = ast_child(a, s, 0);
			vv = ast_child(a, s, 1);
			if (ast_kind(a, vv) != AST_Binary || ast_op(a, vv) != op ||
					ast_nchild(a, vv) != 2)
				{ MCC_TRACE("br\n"); return 0; }
			aj = ast_child(a, vv, 0);
			bj = ast_child(a, vv, 1);
			if (!ast_slp_mem(a, lv, &bC, &iC, &tC, &rC) ||
					!ast_slp_mem(a, aj, &bA, &iA, &tA, &rA) ||
					!ast_slp_mem(a, bj, &bB, &iB, &tB, &rB))
				{ MCC_TRACE("br\n"); return 0; }
			if (tC != etC || tA != etA || tB != etB)
				{ MCC_TRACE("br\n"); return 0; }
			if (!ast_ident_same(a, bC, baseC) || !ast_ident_same(a, bA, baseA) ||
					!ast_ident_same(a, bB, baseB))
				{ MCC_TRACE("br\n"); return 0; }
			if (iC != idxC + j || iA != idxA + j || iB != idxB + j)
				{ MCC_TRACE("br\n"); return 0; }
			stmts[j] = s;
			s = ast_next_sib(a, s);
		}
	}
	{
		CType ebase, V, PV;
		AstLocal addrC, addrA, addrB, vldC, vldA, vldB, vbin;
		memset(&ebase, 0, sizeof ebase);
		ebase.t = etC & (VT_BTYPE | VT_UNSIGNED | VT_LONG);
		ebase.ref = (Sym *)(uintptr_t)erC;
		mk_vector_type(&V, &ebase, lanes);
		PV = V;
		mk_pointer(&PV);
		addrC = ast_dup_sub(a, ast_child(a, lval0, 0));
		addrA = ast_dup_sub(a, ast_child(a, a0, 0));
		addrB = ast_dup_sub(a, ast_child(a, b0, 0));
		vldC = ast_slp_vload(a, &V, &PV, addrC);
		vldA = ast_slp_vload(a, &V, &PV, addrA);
		vldB = ast_slp_vload(a, &V, &PV, addrB);
		vbin = ast_node(a, AST_Binary);
		ast_set_op(a, vbin, op);
		ast_set_type(a, vbin, V.t, (uint64_t)(uintptr_t)V.ref);
		ast_add_child(a, vbin, vldA);
		ast_add_child(a, vbin, vldB);
		ast_clear_children(a, s0);
		ast_set_type(a, s0, V.t, (uint64_t)(uintptr_t)V.ref);
		ast_add_child(a, s0, vldC);
		ast_add_child(a, s0, vbin);
		for (j = 1; j < lanes; j++) { MCC_TRACE("br\n");
			ast_set_kind(a, stmts[j], AST_BasicBlock);
			ast_clear_children(a, stmts[j]);
			ast_set_type(a, stmts[j], 0, 0);
		}
	}
	return lanes;
}

static int ast_vectorize_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a), bb;
	int folds = 0;
	if (!ast_vectorize_env)
		{ MCC_TRACE("br\n"); return 0; }
	for (bb = 0; bb < nn; bb++) { MCC_TRACE("br\n");
		AstLocal s, nxt;
		if (ast_kind(a, bb) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		for (s = ast_first_child(a, bb); s != AST_NONE; s = nxt) { MCC_TRACE("br\n");
			nxt = ast_next_sib(a, s);
			if (ast_slp_try(a, s))
				{ MCC_TRACE("br\n"); folds++; }
		}
	}
	ast_slp_folds = folds;
	return folds;
}

static MCC_OPT_TLS int ast_abs_folds;

static AstLocal ast_abs_neg_of(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal l;
	int ct;
	uint64_t cv;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != '-' || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	l = ast_child(a, n, 0);
	if (ast_kind(a, l) != AST_Literal || !ast_ident_cval(a, l, &ct, &cv) || cv != 0)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	return ast_child(a, n, 1);
}

static int ast_abs_is_zero(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int ct;
	uint64_t cv;
	return ast_kind(a, n) == AST_Literal && ast_ident_cval(a, n, &ct, &cv) && cv == 0;
}

static int ast_abs_cmp_zero(AstArena *a, AstLocal n, AstLocal *key, int *rel) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), ct, keyleft;
	AstLocal l, r, k, c;
	uint64_t cv;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (op != TOK_LT && op != TOK_LE && op != TOK_GT && op != TOK_GE)
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) { MCC_TRACE("br\n");
		k = l;
		c = r;
		keyleft = 1;
	} else if (ast_kind(a, l) == AST_Literal) { MCC_TRACE("br\n");
		k = r;
		c = l;
		keyleft = 0;
	} else { MCC_TRACE("br\n");
		return 0;
	}
	if (!ast_ident_cval(a, c, &ct, &cv) || cv != 0)
		{ MCC_TRACE("br\n"); return 0; }
	*rel = keyleft ? op
								 : op == TOK_LT ? TOK_GT : op == TOK_GT ? TOK_LT : op == TOK_LE ? TOK_GE : TOK_LE;
	*key = k;
	return 1;
}

static AstLocal ast_abs_signmask(AstArena *a, AstLocal key, int ty, int sh) { MCC_TRACE("enter\n");
	return ast_bf_bin(a, TOK_SAR, ty, ast_dup_sub(a, key), ast_bf_lit(a, ty, (uint64_t)sh));
}

static int ast_abs_try(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal cond, tval, fval, key, negval, posval;
	int rel, kt, neg_is_t, mode, ty, sh;
	uint64_t kref;
	if (ast_kind(a, n) != AST_If || ast_op(a, n) != 5 || ast_nchild(a, n) != 3)
		{ MCC_TRACE("br\n"); return 0; }
	cond = ast_child(a, n, 0);
	tval = ast_child(a, n, 1);
	fval = ast_child(a, n, 2);
	if (!ast_abs_cmp_zero(a, cond, &key, &rel))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, key, &kt, &kref) || (kt & VT_UNSIGNED))
		{ MCC_TRACE("br\n"); return 0; }
	if ((kt & VT_BTYPE) == VT_INT) { MCC_TRACE("br\n");
		ty = VT_INT;
		sh = 31;
	} else if ((kt & VT_BTYPE) == VT_LLONG) { MCC_TRACE("br\n");
		ty = VT_LLONG;
		sh = 63;
	} else { MCC_TRACE("br\n");
		return 0;
	}
	if (!ast_ident_pure(a, key))
		{ MCC_TRACE("br\n"); return 0; }
	neg_is_t = (rel == TOK_LT || rel == TOK_LE);
	negval = neg_is_t ? tval : fval;
	posval = neg_is_t ? fval : tval;
	{
		AstLocal nn = ast_abs_neg_of(a, negval), pn = ast_abs_neg_of(a, posval);
		if (nn != AST_NONE && ast_ident_same(a, nn, key) && ast_ident_same(a, posval, key))
			{ MCC_TRACE("br\n"); mode = 0; }
		else if (pn != AST_NONE && ast_ident_same(a, pn, key) && ast_ident_same(a, negval, key))
			{ MCC_TRACE("br\n"); mode = 1; }
		else if (ast_abs_is_zero(a, negval) && ast_ident_same(a, posval, key))
			{ MCC_TRACE("br\n"); mode = 2; }
		else if (ast_abs_is_zero(a, posval) && ast_ident_same(a, negval, key))
			{ MCC_TRACE("br\n"); mode = 3; }
		else
			{ MCC_TRACE("br\n"); return 0; }
	}
	MCC_TRACE("abs/clamp fold key=%u mode=%d ty=0x%x\n", (unsigned)key, mode, ty);
	ast_set_type(a, n, ty, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_kind(a, n, AST_Binary);
	ast_clear_children(a, n);
	if (mode == 0) { MCC_TRACE("br\n");
		ast_set_op(a, n, '-');
		ast_add_child(a, n,
									ast_bf_bin(a, '^', ty, ast_dup_sub(a, key), ast_abs_signmask(a, key, ty, sh)));
		ast_add_child(a, n, ast_abs_signmask(a, key, ty, sh));
	} else if (mode == 1) { MCC_TRACE("br\n");
		ast_set_op(a, n, '-');
		ast_add_child(a, n, ast_abs_signmask(a, key, ty, sh));
		ast_add_child(a, n,
									ast_bf_bin(a, '^', ty, ast_dup_sub(a, key), ast_abs_signmask(a, key, ty, sh)));
	} else if (mode == 2) { MCC_TRACE("br\n");
		ast_set_op(a, n, '-');
		ast_add_child(a, n, ast_dup_sub(a, key));
		ast_add_child(a, n,
									ast_bf_bin(a, '&', ty, ast_dup_sub(a, key), ast_abs_signmask(a, key, ty, sh)));
	} else { MCC_TRACE("br\n");
		ast_set_op(a, n, '&');
		ast_add_child(a, n, ast_dup_sub(a, key));
		ast_add_child(a, n, ast_abs_signmask(a, key, ty, sh));
	}
	return 1;
}

static int ast_abs_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	ast_abs_folds = 0;
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_If && ast_op(a, n) == 5 && ast_nchild(a, n) == 3) { MCC_TRACE("br\n");
			ast_abs_folds += ast_abs_try(a, n);
		} }
	return ast_abs_folds;
}

#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) ||                 \
		defined(MCC_TARGET_RISCV64) || defined(MCC_TARGET_I386) ||                 \
		defined(MCC_TARGET_ARM)
#define AST_SELECT_ARCH 1
#else
#define AST_SELECT_ARCH 0
#endif

static MCC_OPT_TLS int ast_select_folds;

static int ast_sel_relop(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case TOK_LT:
	case TOK_GT:
	case TOK_LE:
	case TOK_GE:
	case TOK_EQ:
	case TOK_NE:
	case TOK_ULT:
	case TOK_UGE:
	case TOK_ULE:
	case TOK_UGT:
		return 1;
	}
	return 0;
}

static int ast_sel_safe(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (ast_type_t(a, n) & VT_VOLATILE)
		{ MCC_TRACE("br\n"); return 0; }
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_Literal:
	case AST_Ref:
		return 1;
	case AST_Convert:
		break;
	case AST_Binary:
		switch (ast_op(a, n)) { MCC_TRACE("br\n");
		case '+':
		case '-':
		case '*':
		case '&':
		case '|':
		case '^':
		case TOK_SHL:
		case TOK_SHR:
		case TOK_SAR:
			break;
		default:
			if (!ast_sel_relop(ast_op(a, n)))
				{ MCC_TRACE("br\n"); return 0; }
		}
		break;
	default:
		return 0;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_sel_safe(a, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static int ast_sel_gpr(int tt) { MCC_TRACE("enter\n");
	if (USING_TWO_WORDS(tt))
		{ MCC_TRACE("br\n"); return 0; }
	return ast_ident_intt(tt) || (tt & VT_BTYPE) == VT_PTR;
}

static int ast_select_try(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal cond, tval, fval;
	int tt1, tt2;
	uint64_t r1, r2;
	if (!AST_SELECT_ARCH)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) != AST_If || ast_op(a, n) != 5 || ast_nchild(a, n) != 3)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_ival(a, n) == AST_SEL_MARK)
		{ MCC_TRACE("br\n"); return 0; }
	cond = ast_child(a, n, 0);
	tval = ast_child(a, n, 1);
	fval = ast_child(a, n, 2);
	if (ast_kind(a, cond) != AST_Binary || !ast_sel_relop(ast_op(a, cond)))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_sel_safe(a, tval) || !ast_sel_safe(a, fval))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, tval, &tt1, &r1) || !ast_ident_etype(a, fval, &tt2, &r2))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_sel_gpr(tt1) || !ast_sel_gpr(tt2))
		{ MCC_TRACE("br\n"); return 0; }
	MCC_TRACE("select mark n=%u\n", (unsigned)n);
	ast_set_ival(a, n, AST_SEL_MARK);
	return 1;
}

static int ast_select_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	ast_select_folds = 0;
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_If && ast_op(a, n) == 5 && ast_nchild(a, n) == 3)
			{ MCC_TRACE("br\n"); ast_select_folds += ast_select_try(a, n); } }
	return ast_select_folds;
}

static MCC_OPT_TLS int ast_reassoc_folds;

static int ast_reassoc_try(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), c1t, c2t, xt, nt, iop, additive, result_op;
	AstLocal inner, c2n, x, c1n;
	uint64_t c1v, c2v, combined, xref, nref, width;
	if (op != '&' && op != '|' && op != '^' && op != '+' && op != '-' && op != '*' &&
			op != TOK_SHL && op != TOK_SHR && op != TOK_SAR)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	{
		AstLocal ch0 = ast_child(a, n, 0), ch1 = ast_child(a, n, 1);
		int comm = (op == '+' || op == '*' || op == '&' || op == '|' || op == '^');
		if (ast_kind(a, ch1) == AST_Literal && ast_ident_cval(a, ch1, &c2t, &c2v))
			{ MCC_TRACE("br\n"); inner = ch0; }
		else if (comm && ast_kind(a, ch0) == AST_Literal && ast_ident_cval(a, ch0, &c2t, &c2v))
			{ MCC_TRACE("br\n"); inner = ch1; }
		else
			{ MCC_TRACE("br\n"); return 0; }
	}
	(void)c2n;
	if (ast_kind(a, inner) != AST_Binary || ast_nchild(a, inner) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	iop = ast_op(a, inner);
	additive = (op == '+' || op == '-');
	if (additive ? (iop != '+' && iop != '-') : (iop != op))
		{ MCC_TRACE("br\n"); return 0; }
	{
		AstLocal ich0 = ast_child(a, inner, 0), ich1 = ast_child(a, inner, 1);
		int icomm = (iop == '+' || iop == '*' || iop == '&' || iop == '|' || iop == '^');
		if (ast_kind(a, ich1) == AST_Literal && ast_ident_cval(a, ich1, &c1t, &c1v))
			{ MCC_TRACE("br\n"); x = ich0; }
		else if (icomm && ast_kind(a, ich0) == AST_Literal && ast_ident_cval(a, ich0, &c1t, &c1v))
			{ MCC_TRACE("br\n"); x = ich1; }
		else
			{ MCC_TRACE("br\n"); return 0; }
	}
	(void)c1n;
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_intt(nt))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || !ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	width = (nt & VT_BTYPE) == VT_LLONG ? 64 : 32;
	result_op = op;
	if (additive) { MCC_TRACE("br\n");
		combined = (iop == '+' ? c1v : (uint64_t)(0 - c1v)) +
							 (op == '+' ? c2v : (uint64_t)(0 - c2v));
		result_op = '+';
	} else
		{ MCC_TRACE("br\n"); switch (op) { MCC_TRACE("br\n");
		case '&': combined = c1v & c2v; break;
		case '|': combined = c1v | c2v; break;
		case '^': combined = c1v ^ c2v; break;
		case '*': combined = c1v * c2v; break;
		default:
			if (c1v >= width || c2v >= width || c1v + c2v >= width)
				{ MCC_TRACE("br\n"); return 0; }
			combined = c1v + c2v;
			break;
		} }
	MCC_TRACE("reassoc op=%d iop=%d c1=%llu c2=%llu -> op=%d %llu\n", op, iop,
						(unsigned long long)c1v, (unsigned long long)c2v, result_op,
						(unsigned long long)combined);
	ast_set_op(a, n, result_op);
	ast_set_type(a, n, nt, nref);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, ast_dup_sub(a, x));
	ast_add_child(a, n, ast_bf_lit(a, nt, combined));
	return 1;
}

static int ast_reassoc_shlshr(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int nt, ct, ict, xt;
	AstLocal inner, c2n, x, c1n;
	uint64_t c1v, c2v, nref, xref, width, mask;
	if (ast_op(a, n) != TOK_SAR || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	inner = ast_child(a, n, 0);
	c2n = ast_child(a, n, 1);
	if (ast_kind(a, c2n) != AST_Literal || !ast_ident_cval(a, c2n, &ct, &c2v))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, inner) != AST_Binary || ast_op(a, inner) != TOK_SHL ||
			ast_nchild(a, inner) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	x = ast_child(a, inner, 0);
	c1n = ast_child(a, inner, 1);
	if (ast_kind(a, c1n) != AST_Literal || !ast_ident_cval(a, c1n, &ict, &c1v) || c1v != c2v)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_intt(nt) || !(nt & VT_UNSIGNED))
		{ MCC_TRACE("br\n"); return 0; }
	width = (nt & VT_BTYPE) == VT_LLONG ? 64 : 32;
	if (c1v == 0 || c1v >= width)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || !ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	mask = width == 64 ? (~0ULL >> c1v) : (uint64_t)(0xFFFFFFFFu >> c1v);
	MCC_TRACE("reassoc shlshr c=%llu -> & 0x%llx\n", (unsigned long long)c1v,
						(unsigned long long)mask);
	ast_set_op(a, n, '&');
	ast_set_type(a, n, nt, nref);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, ast_dup_sub(a, x));
	ast_add_child(a, n, ast_bf_lit(a, nt, mask));
	return 1;
}

static int ast_reassoc_shrshl(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int nt, ct, ict, xt;
	AstLocal inner, c2n, x, c1n;
	uint64_t c1v, c2v, nref, xref, width, mask;
	if (ast_op(a, n) != TOK_SHL || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	inner = ast_child(a, n, 0);
	c2n = ast_child(a, n, 1);
	if (ast_kind(a, c2n) != AST_Literal || !ast_ident_cval(a, c2n, &ct, &c2v))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, inner) != AST_Binary || ast_op(a, inner) != TOK_SAR ||
			ast_nchild(a, inner) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	x = ast_child(a, inner, 0);
	c1n = ast_child(a, inner, 1);
	if (ast_kind(a, c1n) != AST_Literal || !ast_ident_cval(a, c1n, &ict, &c1v) || c1v != c2v)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_intt(nt))
		{ MCC_TRACE("br\n"); return 0; }
	width = (nt & VT_BTYPE) == VT_LLONG ? 64 : 32;
	if (c1v == 0 || c1v >= width)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x, &xt, &xref) || !ast_ident_pure(a, x))
		{ MCC_TRACE("br\n"); return 0; }
	mask = width == 64 ? (~0ULL << c1v) : (uint64_t)(0xFFFFFFFFu << c1v);
	MCC_TRACE("reassoc shrshl c=%llu -> & 0x%llx\n", (unsigned long long)c1v,
						(unsigned long long)mask);
	ast_set_op(a, n, '&');
	ast_set_type(a, n, nt, nref);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, ast_dup_sub(a, x));
	ast_add_child(a, n, ast_bf_lit(a, nt, mask));
	return 1;
}

static int ast_reassoc_mulconst(AstArena *a, AstLocal n, AstLocal *x, uint64_t *cv) { MCC_TRACE("enter\n");
	AstLocal l, r;
	int ct;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != '*' || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal && ast_ident_cval(a, r, &ct, cv)) { MCC_TRACE("br\n");
		*x = l;
		return 1;
	}
	if (ast_kind(a, l) == AST_Literal && ast_ident_cval(a, l, &ct, cv)) { MCC_TRACE("br\n");
		*x = r;
		return 1;
	}
	return 0;
}

static int ast_reassoc_muldist(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int op = ast_op(a, n), nt, xt;
	AstLocal l, r, x1, x2;
	uint64_t c1, c2, combined, nref, xref;
	if ((op != '+' && op != '-') || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	{
		int got_l = ast_reassoc_mulconst(a, l, &x1, &c1);
		int got_r = ast_reassoc_mulconst(a, r, &x2, &c2);
		if (!got_l && !got_r)
			{ MCC_TRACE("br\n"); return 0; }
		if (!got_l) { MCC_TRACE("br\n");
			x1 = l;
			c1 = 1;
		}
		if (!got_r) { MCC_TRACE("br\n");
			x2 = r;
			c2 = 1;
		}
	}
	if (!ast_ident_same(a, x1, x2))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_intt(nt))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, x1, &xt, &xref) || !ast_ident_pure(a, x1))
		{ MCC_TRACE("br\n"); return 0; }
	combined = op == '+' ? c1 + c2 : c1 - c2;
	if (combined == 0 || combined == 1)
		{ MCC_TRACE("br\n"); return 0; }
	MCC_TRACE("reassoc muldist op=%d c1=%llu c2=%llu -> x*%llu\n", op, (unsigned long long)c1,
						(unsigned long long)c2, (unsigned long long)combined);
	ast_set_op(a, n, '*');
	ast_set_type(a, n, nt, nref);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, ast_dup_sub(a, x1));
	ast_add_child(a, n, ast_bf_lit(a, nt, combined));
	return 1;
}

static int ast_reassoc_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	ast_reassoc_folds = 0;
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_Binary && ast_nchild(a, n) == 2) { MCC_TRACE("br\n");
			int f = ast_reassoc_assoc_env ? ast_reassoc_try(a, n) : 0;
			if (!f && ast_reassoc_shlshr_env)
				{ MCC_TRACE("br\n"); f = ast_reassoc_shlshr(a, n); }
			if (!f && ast_reassoc_shrshl_env)
				{ MCC_TRACE("br\n"); f = ast_reassoc_shrshl(a, n); }
			if (!f && ast_reassoc_muldist_env)
				{ MCC_TRACE("br\n"); f = ast_reassoc_muldist(a, n); }
			ast_reassoc_folds += f;
		} }
	return ast_reassoc_folds;
}

static MCC_OPT_TLS int ast_sethi_folds;

static int ast_sethi_commutative(int op) { MCC_TRACE("enter\n");
	return op == '+' || op == '*' || op == '&' || op == '|' || op == '^';
}

static int ast_sethi_cmp_root(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_kind(a, n) != AST_Binary)
		{ MCC_TRACE("br\n"); return 0; }
	switch (ast_op(a, n)) { MCC_TRACE("br\n");
	case TOK_LT:
	case TOK_GT:
	case TOK_LE:
	case TOK_GE:
	case TOK_EQ:
	case TOK_NE:
	case TOK_ULT:
	case TOK_UGE:
	case TOK_ULE:
	case TOK_UGT:
	case TOK_LAND:
	case TOK_LOR:
		return 1;
	}
	return 0;
}

static int ast_sethi_num(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2) { MCC_TRACE("br\n");
		if (ast_sethi_leaf_env && ast_kind(a, n) == AST_Literal)
			{ MCC_TRACE("br\n"); return 0; }
		return 1;
	}
	int l = ast_sethi_num(a, ast_child(a, n, 0));
	int r = ast_sethi_num(a, ast_child(a, n, 1));
	return l == r ? l + 1 : (l > r ? l : r);
}

#define AST_SETHI_NARY_MAX 128

static int ast_sethi_chain_node(AstArena *a, AstLocal n, int op) { MCC_TRACE("enter\n");
	return n != AST_NONE && ast_kind(a, n) == AST_Binary &&
	       ast_nchild(a, n) == 2 && ast_op(a, n) == op;
}

static int ast_sethi_nary_leaf_ok(AstArena *a, AstLocal leaf, int t0) { MCC_TRACE("enter\n");
	int lt;
	uint64_t lref;
	return leaf != AST_NONE && ast_cse_regpure(a, leaf) &&
	       !ast_sethi_cmp_root(a, leaf) && ast_ident_etype(a, leaf, &lt, &lref) &&
	       ast_ident_common(lt, lt) == t0;
}

static int ast_sethi_nary_chain(AstArena *a, AstLocal top) { MCC_TRACE("enter\n");
	int op = ast_op(a, top);
	int t0;
	uint64_t t0ref;
	if (!ast_ident_etype(a, top, &t0, &t0ref) || !ast_ident_intt(t0))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal nodes[AST_SETHI_NARY_MAX];
	AstLocal leaves[AST_SETHI_NARY_MAX + 1];
	int keys[AST_SETHI_NARY_MAX + 1];
	int nnodes = 0, nleaf = 0;
	AstLocal cur = top;
	while (ast_sethi_chain_node(a, cur, op)) { MCC_TRACE("br\n");
		if (nnodes >= AST_SETHI_NARY_MAX || nleaf >= AST_SETHI_NARY_MAX)
			{ MCC_TRACE("br\n"); return 0; }
		AstLocal r = ast_child(a, cur, 1);
		if (!ast_sethi_nary_leaf_ok(a, r, t0))
			{ MCC_TRACE("br\n"); return 0; }
		nodes[nnodes++] = cur;
		leaves[nleaf++] = r;
		cur = ast_child(a, cur, 0);
	}
	if (!ast_sethi_nary_leaf_ok(a, cur, t0))
		{ MCC_TRACE("br\n"); return 0; }
	leaves[nleaf++] = cur;
	if (nleaf < 3)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal order[AST_SETHI_NARY_MAX + 1];
	for (int i = 0; i <= nnodes; i++)
		{ MCC_TRACE("br\n"); order[i] = leaves[nnodes - i]; }
	for (int i = 0; i <= nnodes; i++)
		{ MCC_TRACE("br\n"); keys[i] = ast_sethi_num(a, order[i]); }
	int changed = 0;
	for (int i = 1; i <= nnodes; i++) { MCC_TRACE("br\n");
		AstLocal v = order[i];
		int k = keys[i];
		int j = i - 1;
		while (j >= 0 && keys[j] < k) { MCC_TRACE("br\n");
			order[j + 1] = order[j];
			keys[j + 1] = keys[j];
			j--;
		}
		order[j + 1] = v;
		keys[j + 1] = k;
		if (j + 1 != i)
			{ MCC_TRACE("br\n"); changed = 1; }
	}
	if (!changed)
		{ MCC_TRACE("br\n"); return 0; }
	for (int i = 0; i < nnodes; i++) { MCC_TRACE("br\n");
		AstLocal node = nodes[i];
		AstLocal left = (i == nnodes - 1) ? order[0] : nodes[i + 1];
		AstLocal right = order[nnodes - i];
		ast_clear_children(a, node);
		ast_add_child(a, node, left);
		ast_add_child(a, node, right);
	}
	MCC_TRACE("sethi nary op=%c n=%d\n", op, nnodes + 1);
	return 1;
}

static int ast_sethi_run(AstArena *a) { MCC_TRACE("enter\n");
	ast_sethi_folds = 0;
	AstLocal nn = ast_count(a);
	if (ast_sethi_nary_env) { MCC_TRACE("br\n");
		for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
			if (ast_kind(a, n) != AST_Binary ||
					!ast_sethi_commutative(ast_op(a, n)))
				{ MCC_TRACE("br\n"); continue; }
			if (ast_nchild(a, n) != 2)
				{ MCC_TRACE("br\n"); continue; }
			AstLocal p = ast_parent(a, n);
			if (p != AST_NONE && ast_kind(a, p) == AST_Binary &&
					ast_op(a, p) == ast_op(a, n) && ast_child(a, p, 0) == n)
				{ MCC_TRACE("br\n"); continue; }
			ast_sethi_folds += ast_sethi_nary_chain(a, n);
		}
	}
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_Binary || !ast_sethi_commutative(ast_op(a, n)))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_nchild(a, n) != 2)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal c0 = ast_child(a, n, 0), c1 = ast_child(a, n, 1);
		if (!ast_cse_regpure(a, c0) || !ast_cse_regpure(a, c1))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_sethi_cmp_root(a, c0) || ast_sethi_cmp_root(a, c1))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_sethi_num(a, c1) <= ast_sethi_num(a, c0))
			{ MCC_TRACE("br\n"); continue; }
		ast_clear_children(a, n);
		ast_add_child(a, n, c1);
		ast_add_child(a, n, c0);
		ast_sethi_folds++;
	}
	return ast_sethi_folds;
}

static void ast_licm_at_loop(AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	if (ast_sccp_has_label(a, s))
		{ MCC_TRACE("br\n"); return; }
	for (int i = 0; i < ast_cse_n; i++) { MCC_TRACE("br\n");
		AstLocal e = ast_cse_expr[i], ref = ast_cse_ref[i];
		int foff = ast_cse_off[i];
		if (ast_cprop_escapes(a, foff) || ast_licm_written(a, s, foff))
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_licm_operands_ok(a, s, e))
			{ MCC_TRACE("br\n"); continue; }
		ast_licm_subst(a, s, e, ref, 0);
	}
}

static int ast_ltemp_binop_ok(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case '+':
	case '-':
	case '*':
	case '&':
	case '|':
	case '^':
	case TOK_SHL:
	case TOK_SHR:
	case TOK_SAR:
		return 1;
	}
	return 0;
}

static void ast_ltemp_count_occ(AstArena *a, AstLocal n, AstLocal e, int lval,
																int *cnt) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	if (!lval && n != e && ast_ident_same(a, e, n)) { MCC_TRACE("br\n");
		(*cnt)++;
		return;
	}
	if (k == AST_Store) { MCC_TRACE("br\n");
		ast_ltemp_count_occ(a, ast_child(a, n, 0), e, 1, cnt);
		ast_ltemp_count_occ(a, ast_child(a, n, 1), e, 0, cnt);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_ltemp_count_occ(a, c, e, clval, cnt); }
}

static MCC_OPT_TLS AstLocal ast_ltemp_cand;

static void ast_ltemp_scan(AstArena *a, AstLocal loop, AstLocal n, int lval) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_ltemp_cand != AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	if (!lval && k == AST_Binary && ast_ltemp_binop_ok(ast_op(a, n)) &&
			ast_cse_regpure(a, n) && ast_licm_operands_ok(a, loop, n)) { MCC_TRACE("br\n");
		int et;
		uint64_t er;
		if (ast_ident_etype(a, n, &et, &er) && ast_cse_wide(et)) { MCC_TRACE("br\n");
			int cnt = 0;
			ast_ltemp_count_occ(a, loop, n, 0, &cnt);
			if (cnt >= 1) { MCC_TRACE("br\n");
				ast_ltemp_cand = n;
				return;
			}
		}
	}
	if (k == AST_Store) { MCC_TRACE("br\n");
		ast_ltemp_scan(a, loop, ast_child(a, n, 0), 1);
		ast_ltemp_scan(a, loop, ast_child(a, n, 1), 0);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_ltemp_scan(a, loop, c, clval); }
}

static int ast_ltemp_insert_before(AstArena *a, AstLocal parent, AstLocal pivot,
																	 AstLocal node) { MCC_TRACE("enter\n");
	a->epoch++;
	if (a->first_child[parent] == pivot) { MCC_TRACE("br\n");
		a->parent[node] = parent;
		a->next_sib[node] = pivot;
		a->first_child[parent] = node;
		a->nchild[parent]++;
		return 1;
	}
	for (AstLocal c = a->first_child[parent]; c != AST_NONE; c = a->next_sib[c])
		{ MCC_TRACE("br\n"); if (a->next_sib[c] == pivot) { MCC_TRACE("br\n");
			a->parent[node] = parent;
			a->next_sib[node] = pivot;
			a->next_sib[c] = node;
			a->nchild[parent]++;
			return 1;
		} }
	return 0;
}

static int ast_ltemp_materialize(AstArena *a, AstLocal loop, AstLocal e) { MCC_TRACE("enter\n");
	int et;
	uint64_t er;
	if (ast_ltemp_n >= AST_LTEMP_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ident_etype(a, e, &et, &er) || !ast_cse_wide(et))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal parent = ast_parent(a, loop);
	if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	int lal, lsz = ast_ltemp_size(et, er, &lal);
	int off = ast_ltemp_mint(lsz, lal);
	if (!off)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal lref = ast_node(a, AST_Ref);
	ast_set_op(a, lref, VT_LOCAL | VT_LVAL);
	ast_set_ival(a, lref, (uint64_t)off);
	ast_set_type(a, lref, et, er);
	AstLocal cvt = ast_node(a, AST_Convert);
	ast_set_type(a, cvt, et, er);
	ast_add_child(a, cvt, ast_dup_sub(a, e));
	AstLocal st = ast_node(a, AST_Store);
	ast_add_child(a, st, lref);
	ast_add_child(a, st, cvt);
	if (!ast_ltemp_insert_before(a, parent, loop, st))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal tref = ast_node(a, AST_Ref);
	ast_set_op(a, tref, VT_LOCAL | VT_LVAL);
	ast_set_ival(a, tref, (uint64_t)off);
	ast_set_type(a, tref, et, er);
	ast_licm_subst(a, loop, e, tref, 0);
	ast_cse_setref(a, e, tref);
	ast_licm_folds++;
	return 1;
}

static int ast_ltemp_run(AstArena *a) { MCC_TRACE("enter\n");
	int did = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_If)
			{ MCC_TRACE("br\n"); continue; }
		int op = ast_op(a, n);
		if (op < 2 || op > 4)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_sccp_has_label(a, n))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal parent = ast_parent(a, n);
		if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		for (int per = 0; per < AST_LTEMP_PER_LOOP && ast_ltemp_n < AST_LTEMP_MAX; per++) { MCC_TRACE("br\n");
			ast_ltemp_cand = AST_NONE;
			ast_ltemp_scan(a, n, n, 0);
			if (ast_ltemp_cand == AST_NONE)
				{ MCC_TRACE("br\n"); break; }
			if (!ast_ltemp_materialize(a, n, ast_ltemp_cand))
				{ MCC_TRACE("br\n"); break; }
			did++;
		}
	}
	return did;
}

static int ast_ivsr_width(int tt) { MCC_TRACE("enter\n");
	switch (tt & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_BOOL:
	case VT_BYTE:
		return 1;
	case VT_SHORT:
		return 2;
	case VT_INT:
		return 4;
	case VT_LLONG:
		return 8;
	}
	return 0;
}

static int ast_ivsr_incr_of(AstArena *a, AstLocal s, int *ivoff, int *ivtt,
														int64_t *stride) { MCC_TRACE("enter\n");
	uint16_t k = ast_kind(a, s);
	if (k == AST_Unary && (ast_op(a, s) == TOK_INC || ast_op(a, s) == TOK_DEC)) { MCC_TRACE("br\n");
		AstLocal r = ast_first_child(a, s);
		int off, tt;
		if (r != AST_NONE && ast_cprop_is_local(a, r, &off, &tt) &&
				ast_ident_intt(tt)) { MCC_TRACE("br\n");
			*ivoff = off;
			*ivtt = tt;
			*stride = ast_op(a, s) == TOK_INC ? 1 : -1;
			return 1;
		}
		return 0;
	}
	if (k == AST_Store) { MCC_TRACE("br\n");
		AstLocal lval = ast_child(a, s, 0), rhs = ast_child(a, s, 1);
		int off, tt;
		if (!ast_cprop_is_local(a, lval, &off, &tt) || !ast_ident_intt(tt))
			{ MCC_TRACE("br\n"); return 0; }
		if (rhs == AST_NONE || ast_kind(a, rhs) != AST_Binary ||
				ast_nchild(a, rhs) != 2)
			{ MCC_TRACE("br\n"); return 0; }
		int bop = ast_op(a, rhs);
		if (bop != '+' && bop != '-')
			{ MCC_TRACE("br\n"); return 0; }
		AstLocal x = ast_child(a, rhs, 0), y = ast_child(a, rhs, 1);
		if (ast_ref_is_local_off(a, x, off) && ast_kind(a, y) == AST_Literal) { MCC_TRACE("br\n");
			int64_t kk = (int64_t)ast_ival(a, y);
			*ivoff = off;
			*ivtt = tt;
			*stride = bop == '+' ? kk : -kk;
			return 1;
		}
		if (bop == '+' && ast_ref_is_local_off(a, y, off) &&
				ast_kind(a, x) == AST_Literal) { MCC_TRACE("br\n");
			*ivoff = off;
			*ivtt = tt;
			*stride = (int64_t)ast_ival(a, x);
			return 1;
		}
	}
	return 0;
}

static int ast_ivsr_count_writes(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	uint16_t k = ast_kind(a, n);
	int cnt = 0;
	if (k == AST_Store && ast_ref_is_local_off(a, ast_child(a, n, 0), off))
		{ MCC_TRACE("br\n"); cnt++; }
	if (k == AST_Unary && ast_ref_is_local_off(a, ast_first_child(a, n), off))
		{ MCC_TRACE("br\n"); cnt++; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); cnt += ast_ivsr_count_writes(a, c, off); }
	return cnt;
}

static MCC_OPT_TLS AstLocal ast_ivsr_target;

static AstLocal ast_ivsr_cofactor(AstArena *a, AstLocal loop, AstLocal mul,
																	int ivoff, int ivtt) { MCC_TRACE("enter\n");
	int et;
	uint64_t er;
	if (ast_kind(a, mul) != AST_Binary || ast_op(a, mul) != '*' ||
			ast_nchild(a, mul) != 2 || !ast_cse_regpure(a, mul))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!ast_ident_etype(a, mul, &et, &er) || !ast_ident_intt(et))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!ast_ivsr_width(et) || ast_ivsr_width(et) != ast_ivsr_width(ivtt))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	AstLocal x = ast_child(a, mul, 0), y = ast_child(a, mul, 1), c;
	if (ast_ref_is_local_off(a, x, ivoff))
		{ MCC_TRACE("br\n"); c = y; }
	else if (ast_ref_is_local_off(a, y, ivoff))
		{ MCC_TRACE("br\n"); c = x; }
	else
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!ast_cse_regpure(a, c) || !ast_licm_operands_ok(a, loop, c))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	return c;
}

static void ast_ivsr_scan(AstArena *a, AstLocal loop, AstLocal n, int ivoff,
													int ivtt) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_ivsr_target != AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	if (ast_ivsr_cofactor(a, loop, n, ivoff, ivtt) != AST_NONE) { MCC_TRACE("br\n");
		ast_ivsr_target = n;
		return;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_ivsr_scan(a, loop, c, ivoff, ivtt); }
}

static int ast_ivsr_run(AstArena *a) { MCC_TRACE("enter\n");
	int did = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_If)
			{ MCC_TRACE("br\n"); continue; }
		int op = ast_op(a, n);
		if (op != 3 && op != 8)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_sccp_has_label(a, n))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal parent = ast_parent(a, n);
		if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal incrbb = ast_child(a, n, op == 3 ? 1 : 0);
		AstLocal body = ast_child(a, n, op == 3 ? 2 : 1);
		if (incrbb == AST_NONE || body == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_kind(a, incrbb) != AST_BasicBlock ||
				ast_kind(a, body) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		int ivoff = 0, ivtt = 0, found = 0;
		int64_t stride = 0;
		for (AstLocal s = ast_first_child(a, incrbb); s != AST_NONE;
				 s = ast_next_sib(a, s))
			{ MCC_TRACE("br\n"); if (ast_ivsr_incr_of(a, s, &ivoff, &ivtt, &stride)) { MCC_TRACE("br\n");
				found = 1;
				break;
			} }
		if (!found)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_cprop_escapes(a, ivoff))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_ivsr_count_writes(a, n, ivoff) != 1)
			{ MCC_TRACE("br\n"); continue; }
		ast_ivsr_target = AST_NONE;
		ast_ivsr_scan(a, n, body, ivoff, ivtt);
		if (ast_ivsr_target == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_ltemp_n >= AST_LTEMP_MAX)
			{ MCC_TRACE("br\n"); break; }
		AstLocal mul = ast_ivsr_target;
		AstLocal c = ast_ivsr_cofactor(a, n, mul, ivoff, ivtt);
		if (c == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		int et;
		uint64_t er;
		ast_ident_etype(a, mul, &et, &er);
		int off = (ast_ltemp_cur - 8) & -8;
		AstLocal lref = ast_node(a, AST_Ref);
		ast_set_op(a, lref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, lref, (uint64_t)off);
		ast_set_type(a, lref, et, er);
		AstLocal cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, et, er);
		ast_add_child(a, cvt, ast_dup_sub(a, mul));
		AstLocal st = ast_node(a, AST_Store);
		ast_add_child(a, st, lref);
		ast_add_child(a, st, cvt);
		if (!ast_ltemp_insert_before(a, parent, n, st))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal iref = ast_node(a, AST_Ref);
		ast_set_op(a, iref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, iref, (uint64_t)off);
		ast_set_type(a, iref, et, er);
		AstLocal lit = ast_node(a, AST_Literal);
		ast_set_op(a, lit, VT_CONST);
		ast_set_type(a, lit, et, 0);
		ast_set_ival(a, lit, (uint64_t)stride);
		AstLocal delta = ast_node(a, AST_Binary);
		ast_set_op(a, delta, '*');
		ast_set_type(a, delta, et, er);
		ast_add_child(a, delta, lit);
		ast_add_child(a, delta, ast_dup_sub(a, c));
		AstLocal add = ast_node(a, AST_Binary);
		ast_set_op(a, add, '+');
		ast_set_type(a, add, et, er);
		ast_add_child(a, add, iref);
		ast_add_child(a, add, delta);
		AstLocal cvt2 = ast_node(a, AST_Convert);
		ast_set_type(a, cvt2, et, er);
		ast_add_child(a, cvt2, add);
		AstLocal iwref = ast_node(a, AST_Ref);
		ast_set_op(a, iwref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, iwref, (uint64_t)off);
		ast_set_type(a, iwref, et, er);
		AstLocal ist = ast_node(a, AST_Store);
		ast_add_child(a, ist, iwref);
		ast_add_child(a, ist, cvt2);
		ast_add_child(a, incrbb, ist);
		AstLocal uref = ast_node(a, AST_Ref);
		ast_set_op(a, uref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, uref, (uint64_t)off);
		ast_set_type(a, uref, et, er);
		ast_licm_subst(a, body, mul, uref, 0);
		ast_cse_setref(a, mul, uref);
		ast_licm_folds++;
		ast_ltemp_cur = off;
		ast_ltemp_add(off, 8);
		did++;
	}
	return did;
}

static int ast_ivsr_is_iv_ref(AstArena *a, AstLocal n, int ivoff) { MCC_TRACE("enter\n");
	while (n != AST_NONE && ast_kind(a, n) == AST_Convert && ast_nchild(a, n) == 1)
		{ MCC_TRACE("br\n"); n = ast_first_child(a, n); }
	return ast_ref_is_local_off(a, n, ivoff);
}

static MCC_OPT_TLS AstLocal ast_ivsr_ptr_target;

static int ast_ivsr_ptr_base_varies(AstArena *a, AstLocal loop, AstLocal n,
																		int ivoff) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_ref_is_local_off(a, n, ivoff))
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_kind(a, n) == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM) &&
				ast_licm_written(a, loop, (int)(int64_t)ast_ival(a, n)))
			{ MCC_TRACE("br\n"); return 1; }
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_ivsr_ptr_base_varies(a, loop, c, ivoff))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static AstLocal ast_ivsr_ptr_cofactor(AstArena *a, AstLocal loop, AstLocal n,
																			int ivoff) { MCC_TRACE("enter\n");
	int et;
	uint64_t er;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != '+' ||
			ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!ast_ident_etype(a, n, &et, &er) || (et & VT_BTYPE) != VT_PTR)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1), base;
	if (ast_ivsr_is_iv_ref(a, y, ivoff))
		{ MCC_TRACE("br\n"); base = x; }
	else if (ast_ivsr_is_iv_ref(a, x, ivoff))
		{ MCC_TRACE("br\n"); base = y; }
	else
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (!ast_licm_operands_ok(a, loop, base) || !ast_expr_pure(a, base, 16))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	if (ast_ivsr_ptr_base_varies(a, loop, base, ivoff))
		{ MCC_TRACE("br\n"); return AST_NONE; }
	return n;
}

static void ast_ivsr_ptr_subst(AstArena *a, AstLocal n, AstLocal e,
															 AstLocal ref) { MCC_TRACE("enter\n");
	if (n == AST_NONE || n == ref)
		{ MCC_TRACE("br\n"); return; }
	if (ast_ident_same(a, e, n)) { MCC_TRACE("br\n");
		ast_cse_setref(a, n, ref);
		ast_licm_folds++;
		return;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_ivsr_ptr_subst(a, c, e, ref); }
}

static void ast_ivsr_ptr_scan(AstArena *a, AstLocal loop, AstLocal n,
															int ivoff) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_ivsr_ptr_target != AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	if (ast_ivsr_ptr_cofactor(a, loop, n, ivoff) != AST_NONE) { MCC_TRACE("br\n");
		ast_ivsr_ptr_target = n;
		return;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_ivsr_ptr_scan(a, loop, c, ivoff); }
}

static int ast_ivsr_ptr_run(AstArena *a) { MCC_TRACE("enter\n");
	int did = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) != AST_If)
			{ MCC_TRACE("br\n"); continue; }
		int op = ast_op(a, n);
		if (op != 3 && op != 8)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal parent = ast_parent(a, n);
		if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal incrbb = ast_child(a, n, op == 3 ? 1 : 0);
		AstLocal body = ast_child(a, n, op == 3 ? 2 : 1);
		if (incrbb == AST_NONE || body == AST_NONE ||
				ast_kind(a, incrbb) != AST_BasicBlock ||
				ast_kind(a, body) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_sccp_has_label(a, body))
			{ MCC_TRACE("br\n"); continue; }
		int ivoff = 0, ivtt = 0, found = 0;
		int64_t stride = 0;
		for (AstLocal s = ast_first_child(a, incrbb); s != AST_NONE;
				 s = ast_next_sib(a, s))
			{ MCC_TRACE("br\n"); if (ast_ivsr_incr_of(a, s, &ivoff, &ivtt, &stride)) { MCC_TRACE("br\n");
				found = 1;
				break;
			} }
		if (!found)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_cprop_escapes(a, ivoff))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_ivsr_count_writes(a, n, ivoff) != 1)
			{ MCC_TRACE("br\n"); continue; }
		ast_ivsr_ptr_target = AST_NONE;
		ast_ivsr_ptr_scan(a, n, body, ivoff);
		if (ast_ivsr_ptr_target == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_ltemp_n >= AST_LTEMP_MAX)
			{ MCC_TRACE("br\n"); break; }
		AstLocal padd = ast_ivsr_ptr_target;
		int et;
		uint64_t er;
		ast_ident_etype(a, padd, &et, &er);
		int off = (ast_ltemp_cur - 8) & -8;
		AstLocal lref = ast_node(a, AST_Ref);
		ast_set_op(a, lref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, lref, (uint64_t)off);
		ast_set_type(a, lref, et, er);
		AstLocal cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, et, er);
		ast_add_child(a, cvt, ast_dup_sub(a, padd));
		AstLocal st = ast_node(a, AST_Store);
		ast_add_child(a, st, lref);
		ast_add_child(a, st, cvt);
		if (!ast_ltemp_insert_before(a, parent, n, st))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal iref = ast_node(a, AST_Ref);
		ast_set_op(a, iref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, iref, (uint64_t)off);
		ast_set_type(a, iref, et, er);
		AstLocal lit = ast_node(a, AST_Literal);
		ast_set_op(a, lit, VT_CONST);
		ast_set_type(a, lit, ivtt, 0);
		ast_set_ival(a, lit, (uint64_t)stride);
		AstLocal add = ast_node(a, AST_Binary);
		ast_set_op(a, add, '+');
		ast_set_type(a, add, et, er);
		ast_add_child(a, add, iref);
		ast_add_child(a, add, lit);
		AstLocal cvt2 = ast_node(a, AST_Convert);
		ast_set_type(a, cvt2, et, er);
		ast_add_child(a, cvt2, add);
		AstLocal iwref = ast_node(a, AST_Ref);
		ast_set_op(a, iwref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, iwref, (uint64_t)off);
		ast_set_type(a, iwref, et, er);
		AstLocal ist = ast_node(a, AST_Store);
		ast_add_child(a, ist, iwref);
		ast_add_child(a, ist, cvt2);
		ast_add_child(a, incrbb, ist);
		AstLocal uref = ast_node(a, AST_Ref);
		ast_set_op(a, uref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, uref, (uint64_t)off);
		ast_set_type(a, uref, et, er);
		ast_ivsr_ptr_subst(a, body, padd, uref);
		ast_licm_folds++;
		ast_ltemp_cur = off;
		ast_ltemp_add(off, 8);
		did++;
	}
	return did;
}

#define AST_LOOPNEST_CAP 256

enum {
	AST_LOOP_BOUND_NONE = 0,
	AST_LOOP_BOUND_CONST = 1,
	AST_LOOP_BOUND_SYMBOLIC = 2
};

typedef struct AstLoopInfo {
	AstLocal header;
	AstLocal parent;
	AstLocal body;
	AstLocal incr;
	AstLocal cond;
	int op;
	int depth;
	int unanalyzable;
	int has_iv;
	int iv_off;
	int iv_tt;
	int64_t iv_stride;
	int bound_kind;
	int64_t bound;
	int bound_is_lower;
} AstLoopInfo;

static AstLoopInfo ast_loopnest[AST_LOOPNEST_CAP];
static int ast_loopnest_n;
static int ast_loopnest_overflow;
static AstArena *ast_loopnest_arena;
static unsigned ast_loopnest_epoch;
static int ast_loopnest_state;

static AstLocal ast_loop_cond_node(AstArena *a, AstLocal loop, int op) { MCC_TRACE("enter\n");
	if (op == 2 || op == 3)
		{ MCC_TRACE("br\n"); return ast_child(a, loop, 0); }
	if (op == 4)
		{ MCC_TRACE("br\n"); return ast_child(a, loop, 1); }
	return AST_NONE;
}

static void ast_loop_parts(AstArena *a, AstLocal loop, int op, AstLocal *body,
													 AstLocal *incr) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case 2:
		*body = ast_child(a, loop, 1);
		*incr = AST_NONE;
		break;
	case 3:
		*incr = ast_child(a, loop, 1);
		*body = ast_child(a, loop, 2);
		break;
	case 4:
		*body = ast_child(a, loop, 0);
		*incr = AST_NONE;
		break;
	case 8:
		*incr = ast_child(a, loop, 0);
		*body = ast_child(a, loop, 1);
		break;
	default:
		*body = AST_NONE;
		*incr = AST_NONE;
	}
}

static int ast_loop_has_unstructured(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Jump &&
			(ast_op(a, n) == 4 || ast_op(a, n) == 5))
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_loop_has_unstructured(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static void ast_loop_ancestry(AstArena *a, AstLocal loop, AstLocal *parent,
															int *depth) { MCC_TRACE("enter\n");
	int d = 0;
	AstLocal par = AST_NONE;
	for (AstLocal p = ast_parent(a, loop); p != AST_NONE; p = ast_parent(a, p))
		{ MCC_TRACE("br\n"); if (ast_licm_is_loop(a, p)) { MCC_TRACE("br\n");
			if (par == AST_NONE)
				{ MCC_TRACE("br\n"); par = p; }
			d++;
		} }
	*parent = par;
	*depth = d;
}

static int ast_lcen_slot(int off) { MCC_TRACE("enter\n");
	if (!mcc_state->loop_census)
		{ MCC_TRACE("br\n"); return 0; }
	for (int i = 0; i < lcen_fn_n; i++)
		{ MCC_TRACE("br\n"); if (lcen_fn_slots[i] == off)
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_loop_find_iv(AstArena *a, AstLocal loop, AstLocal src, int *off,
														int *tt, int64_t *stride) { MCC_TRACE("enter\n");
	if (src == AST_NONE || ast_kind(a, src) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal s = ast_first_child(a, src); s != AST_NONE;
			 s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		int o, t;
		int64_t st;
		if (!ast_ivsr_incr_of(a, s, &o, &t, &st))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_lcen_slot(o))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_cprop_escapes(a, o))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_ivsr_count_writes(a, loop, o) != 1)
			{ MCC_TRACE("br\n"); continue; }
		*off = o;
		*tt = t;
		*stride = st;
		return 1;
	}
	return 0;
}

static int ast_loop_part_uses_iv(AstArena *a, AstLocal n, int iv_off) { MCC_TRACE("enter\n");
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	int op = ast_op(a, n);
	if (op != TOK_LE && op != TOK_GE && op != TOK_LT && op != TOK_GT &&
			op != TOK_NE && op != TOK_EQ)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_ref_is_local_off(a, ast_child(a, n, 0), iv_off) ||
				 ast_ref_is_local_off(a, ast_child(a, n, 1), iv_off);
}

static int ast_loop_classify_bound(AstArena *a, AstLocal cond, int iv_off,
																	 int64_t *bound, int *is_lower) { MCC_TRACE("enter\n");
	if (cond == AST_NONE)
		{ MCC_TRACE("br\n"); return AST_LOOP_BOUND_NONE; }
	AstLocal parts[2];
	int nparts = 1;
	parts[0] = cond;
	if (ast_kind(a, cond) == AST_Binary && ast_op(a, cond) == TOK_LAND &&
			ast_nchild(a, cond) == 2) { MCC_TRACE("br\n");
		parts[0] = ast_child(a, cond, 0);
		parts[1] = ast_child(a, cond, 1);
		nparts = 2;
	}
	for (int i = 0; i < nparts; i++) { MCC_TRACE("br\n");
		AstLocal key;
		int64_t b;
		int il;
		if (!ast_range_bound(a, parts[i], &key, &b, &il))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_kind(a, key) != AST_Ref)
			{ MCC_TRACE("br\n"); continue; }
		int r = ast_op(a, key);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			{ MCC_TRACE("br\n"); continue; }
		if ((int)(int64_t)ast_ival(a, key) != iv_off)
			{ MCC_TRACE("br\n"); continue; }
		*bound = b;
		*is_lower = il;
		return AST_LOOP_BOUND_CONST;
	}
	for (int i = 0; i < nparts; i++)
		{ MCC_TRACE("br\n"); if (ast_loop_part_uses_iv(a, parts[i], iv_off))
			{ MCC_TRACE("br\n"); return AST_LOOP_BOUND_SYMBOLIC; } }
	return AST_LOOP_BOUND_NONE;
}

static void ast_loopnest_compute(AstArena *a) { MCC_TRACE("enter\n");
	ast_loopnest_n = 0;
	ast_loopnest_overflow = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (!ast_licm_is_loop(a, n))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_loopnest_n >= AST_LOOPNEST_CAP) { MCC_TRACE("br\n");
			ast_loopnest_overflow = 1;
			break;
		}
		AstLoopInfo *li = &ast_loopnest[ast_loopnest_n++];
		memset(li, 0, sizeof *li);
		li->header = n;
		li->parent = AST_NONE;
		li->op = ast_op(a, n);
		li->bound_kind = AST_LOOP_BOUND_NONE;
		ast_loop_ancestry(a, n, &li->parent, &li->depth);
		ast_loop_parts(a, n, li->op, &li->body, &li->incr);
		li->cond = ast_loop_cond_node(a, n, li->op);
		if (ast_loop_has_unstructured(a, n)) { MCC_TRACE("br\n");
			li->unanalyzable = 1;
			continue;
		}
		AstLocal ivsrc = (li->op == 3 || li->op == 8) ? li->incr : li->body;
		if (ast_loop_find_iv(a, n, ivsrc, &li->iv_off, &li->iv_tt, &li->iv_stride))
			{ MCC_TRACE("br\n"); li->has_iv = 1; }
		if (li->has_iv && li->cond != AST_NONE)
			{ MCC_TRACE("br\n"); li->bound_kind = ast_loop_classify_bound(a, li->cond, li->iv_off,
																							 &li->bound, &li->bound_is_lower); }
	}
}

static void ast_loopnest_invalidate(const AstArena *a) { MCC_TRACE("enter\n");
	if (ast_loopnest_arena == a) { MCC_TRACE("br\n");
		ast_loopnest_state = 0;
		ast_loopnest_arena = NULL;
	}
}

static void ast_loopnest_sync(AstArena *a) { MCC_TRACE("enter\n");
	if (ast_loopnest_state && ast_loopnest_arena == a &&
			ast_loopnest_epoch == a->epoch)
		{ MCC_TRACE("br\n"); return; }
	ast_loopnest_arena = a;
	ast_loopnest_epoch = a->epoch;
	ast_loopnest_state = 1;
	ast_loopnest_compute(a);
}

static AstLoopInfo *ast_loop_find(AstArena *a, AstLocal loop) { MCC_TRACE("enter\n");
	ast_loopnest_sync(a);
	for (int i = 0; i < ast_loopnest_n; i++)
		{ MCC_TRACE("br\n"); if (ast_loopnest[i].header == loop)
			{ MCC_TRACE("br\n"); return &ast_loopnest[i]; } }
	return NULL;
}

int ast_loopnest_build(AstArena *a) { MCC_TRACE("enter\n");
	ast_loopnest_sync(a);
	return ast_loopnest_overflow ? -1 : ast_loopnest_n;
}

int ast_loop_depth(AstArena *a, AstLocal loop) { MCC_TRACE("enter\n");
	AstLoopInfo *li = ast_loop_find(a, loop);
	return li ? li->depth : -1;
}

AstLocal ast_loop_parent(AstArena *a, AstLocal loop) { MCC_TRACE("enter\n");
	AstLoopInfo *li = ast_loop_find(a, loop);
	return li ? li->parent : AST_NONE;
}

int ast_loop_iv(AstArena *a, AstLocal loop, int *off, int *tt, int64_t *stride) { MCC_TRACE("enter\n");
	AstLoopInfo *li = ast_loop_find(a, loop);
	if (!li || !li->has_iv)
		{ MCC_TRACE("br\n"); return 0; }
	if (off)
		{ MCC_TRACE("br\n"); *off = li->iv_off; }
	if (tt)
		{ MCC_TRACE("br\n"); *tt = li->iv_tt; }
	if (stride)
		{ MCC_TRACE("br\n"); *stride = li->iv_stride; }
	return 1;
}

int ast_loop_bounds(AstArena *a, AstLocal loop, int64_t *bound, int *is_lower) { MCC_TRACE("enter\n");
	AstLoopInfo *li = ast_loop_find(a, loop);
	if (!li || li->bound_kind != AST_LOOP_BOUND_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	if (bound)
		{ MCC_TRACE("br\n"); *bound = li->bound; }
	if (is_lower)
		{ MCC_TRACE("br\n"); *is_lower = li->bound_is_lower; }
	return 1;
}

int ast_loop_analyzable(AstArena *a, AstLocal loop) { MCC_TRACE("enter\n");
	AstLoopInfo *li = ast_loop_find(a, loop);
	return li && !li->unanalyzable && li->has_iv;
}

static const char *ast_loop_kind_name(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case 2:
		return "while";
	case 3:
		return "for";
	case 4:
		return "do-while";
	case 8:
		return "for-noinc";
	}
	return "?";
}

void ast_loopnest_dump(AstArena *a, const char *fname) { MCC_TRACE("enter\n");
	ast_loopnest_sync(a);
	fprintf(stderr, "[LOOPNEST] %s: %d loop(s)%s\n", fname ? fname : "?",
					ast_loopnest_n, ast_loopnest_overflow ? " OVERFLOW" : "");
	for (int i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
		AstLoopInfo *li = &ast_loopnest[i];
		fprintf(stderr, "  loop#%u kind=%s depth=%d parent=",
						(unsigned)li->header, ast_loop_kind_name(li->op), li->depth);
		if (li->parent == AST_NONE)
			{ MCC_TRACE("br\n"); fprintf(stderr, "-"); }
		else
			{ MCC_TRACE("br\n"); fprintf(stderr, "#%u", (unsigned)li->parent); }
		if (li->unanalyzable) { MCC_TRACE("br\n");
			fprintf(stderr, " UNANALYZABLE(label/goto)\n");
			continue;
		}
		int lo = (int)li->body, hi = (int)li->body;
		ast_subtree_span(a, li->body, &lo, &hi);
		fprintf(stderr, " body=[%d,%d]", lo, hi);
		if (li->has_iv)
			{ MCC_TRACE("br\n"); fprintf(stderr, " iv=@%d stride=%lld", li->iv_off,
							(long long)li->iv_stride); }
		else
			{ MCC_TRACE("br\n"); fprintf(stderr, " iv=none"); }
		if (li->bound_kind == AST_LOOP_BOUND_CONST)
			{ MCC_TRACE("br\n"); fprintf(stderr, " bound%s=%lld", li->bound_is_lower ? ">=" : "<=",
							(long long)li->bound); }
		else if (li->bound_kind == AST_LOOP_BOUND_SYMBOLIC)
			{ MCC_TRACE("br\n"); fprintf(stderr, " bound=symbolic"); }
		else
			{ MCC_TRACE("br\n"); fprintf(stderr, " bound=none"); }
		fprintf(stderr, " analyzable=%d\n", li->has_iv ? 1 : 0);
	}
}

#define AST_DEP_MAXIV 8
#define AST_DEP_MAXDIM 4
#define AST_DEP_MAXREF 64

enum { AST_DEP_INDEP = 0, AST_DEP_YES = 1, AST_DEP_CONSERV = 2 };

typedef struct AstDepSub {
	int64_t coeff[AST_DEP_MAXIV];
	int64_t cst;
} AstDepSub;

typedef struct AstDepRef {
	AstLocal node;
	int is_store;
	int base_kind;
	uint64_t base_sym;
	int64_t base_off;
	int ndim;
	int ok;
	int indirect;
	AstDepSub sub[AST_DEP_MAXDIM];
} AstDepRef;

static int ast_dep_decay(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int t = ast_type_t(a, n);
	return (t & (VT_ARRAY | VT_VLA)) == VT_ARRAY;
}

static AstLocal ast_dep_strip(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	while (n != AST_NONE && ast_kind(a, n) == AST_Convert &&
				 ast_nchild(a, n) == 1)
		{ MCC_TRACE("br\n"); n = ast_first_child(a, n); }
	return n;
}

static int ast_dep_affine_acc(AstArena *a, AstLocal E, const int *ivs, int niv,
															int64_t *coeff, int64_t *cst, int64_t mul) { MCC_TRACE("enter\n");
	E = ast_dep_strip(a, E);
	if (E == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	uint16_t k = ast_kind(a, E);
	if (k == AST_Literal) { MCC_TRACE("br\n");
		*cst += mul * (int64_t)ast_ival(a, E);
		return 1;
	}
	if (k == AST_Ref) { MCC_TRACE("br\n");
		int r = ast_op(a, E);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			{ MCC_TRACE("br\n"); return 0; }
		int off = (int)(int64_t)ast_ival(a, E);
		for (int i = 0; i < niv; i++)
			{ MCC_TRACE("br\n"); if (ivs[i] == off) { MCC_TRACE("br\n");
				coeff[i] += mul;
				return 1;
			} }
		return 0;
	}
	if (k == AST_Binary && ast_nchild(a, E) == 2) { MCC_TRACE("br\n");
		int op = ast_op(a, E);
		AstLocal L = ast_child(a, E, 0), R = ast_child(a, E, 1);
		if (op == '+')
			{ MCC_TRACE("br\n"); return ast_dep_affine_acc(a, L, ivs, niv, coeff, cst, mul) &&
						 ast_dep_affine_acc(a, R, ivs, niv, coeff, cst, mul); }
		if (op == '-')
			{ MCC_TRACE("br\n"); return ast_dep_affine_acc(a, L, ivs, niv, coeff, cst, mul) &&
						 ast_dep_affine_acc(a, R, ivs, niv, coeff, cst, -mul); }
		if (op == '*') { MCC_TRACE("br\n");
			AstLocal Ls = ast_dep_strip(a, L), Rs = ast_dep_strip(a, R);
			if (ast_kind(a, Ls) == AST_Literal)
				{ MCC_TRACE("br\n"); return ast_dep_affine_acc(a, R, ivs, niv, coeff, cst,
																	mul * (int64_t)ast_ival(a, Ls)); }
			if (ast_kind(a, Rs) == AST_Literal)
				{ MCC_TRACE("br\n"); return ast_dep_affine_acc(a, L, ivs, niv, coeff, cst,
																	mul * (int64_t)ast_ival(a, Rs)); }
			return 0;
		}
		return 0;
	}
	return 0;
}

static int ast_dep_affine(AstArena *a, AstLocal E, const int *ivs, int niv,
													int64_t *coeff, int64_t *cst) { MCC_TRACE("enter\n");
	for (int i = 0; i < niv; i++)
		{ MCC_TRACE("br\n"); coeff[i] = 0; }
	*cst = 0;
	return ast_dep_affine_acc(a, E, ivs, niv, coeff, cst, 1);
}

static void ast_dep_decode(AstArena *a, AstLocal E, const int *ivs, int niv,
													 AstDepRef *r, unsigned char *consumed, int nn) { MCC_TRACE("enter\n");
	memset(r, 0, sizeof *r);
	AstDepSub tmp[AST_DEP_MAXDIM];
	int nd = 0;
	for (;;) { MCC_TRACE("br\n");
		E = ast_dep_strip(a, E);
		if (E == AST_NONE)
			{ MCC_TRACE("br\n"); return; }
		uint16_t k = ast_kind(a, E);
		if (k == AST_Binary && ast_nchild(a, E) == 2 &&
				(ast_op(a, E) == '+' || ast_op(a, E) == '-')) { MCC_TRACE("br\n");
			int op = ast_op(a, E);
			AstLocal L = ast_child(a, E, 0), R = ast_child(a, E, 1);
			int64_t cR[AST_DEP_MAXIV], kR, cL[AST_DEP_MAXIV], kL;
			int okR = ast_dep_affine(a, R, ivs, niv, cR, &kR);
			int okL = ast_dep_affine(a, L, ivs, niv, cL, &kL);
			int64_t *co;
			int64_t cst;
			AstLocal chain;
			int sign;
			if (okR && !okL) { MCC_TRACE("br\n");
				co = cR;
				cst = kR;
				chain = L;
				sign = op == '-' ? -1 : 1;
			} else if (okL && !okR && op == '+') { MCC_TRACE("br\n");
				co = cL;
				cst = kL;
				chain = R;
				sign = 1;
			} else { MCC_TRACE("br\n");
				return;
			}
			if (nd >= AST_DEP_MAXDIM)
				{ MCC_TRACE("br\n"); return; }
			for (int i = 0; i < niv; i++)
				{ MCC_TRACE("br\n"); tmp[nd].coeff[i] = sign * co[i]; }
			tmp[nd].cst = sign * cst;
			nd++;
			E = chain;
			continue;
		}
		if (k == AST_Load && ast_nchild(a, E) >= 1) { MCC_TRACE("br\n");
			if (consumed && (uint32_t)E < (uint32_t)nn)
				{ MCC_TRACE("br\n"); consumed[E] = 1; }
			if (!ast_dep_decay(a, E))
				{ MCC_TRACE("br\n"); r->indirect = 1; }
			E = ast_first_child(a, E);
			continue;
		}
		if (k == AST_Ref) { MCC_TRACE("br\n");
			int rr = ast_op(a, E);
			if ((rr & VT_LVAL) && !(ast_type_t(a, E) & VT_ARRAY))
				{ MCC_TRACE("br\n"); r->indirect = 1; }
			if ((rr & VT_VALMASK) == VT_CONST && (rr & VT_SYM) &&
					ast_sym(a, E) != 0) { MCC_TRACE("br\n");
				r->base_kind = 1;
				r->base_sym = ast_sym(a, E);
			} else if ((rr & VT_VALMASK) == VT_LOCAL) { MCC_TRACE("br\n");
				r->base_kind = 2;
				r->base_off = (int64_t)(int)ast_ival(a, E);
			} else { MCC_TRACE("br\n");
				return;
			}
			for (int i = 0; i < nd; i++)
				{ MCC_TRACE("br\n"); r->sub[i] = tmp[nd - 1 - i]; }
			r->ndim = nd;
			r->ok = 1;
			return;
		}
		return;
	}
}

static void ast_dep_collect_rec(AstArena *a, AstLocal n, const int *ivs,
																int niv, AstDepRef *refs, int *nref,
																int *overflow, unsigned char *consumed,
																int nn) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	if ((uint32_t)n < (uint32_t)nn && consumed[n])
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	if (k == AST_Store && ast_nchild(a, n) >= 2) { MCC_TRACE("br\n");
		AstLocal lv = ast_child(a, n, 0);
		if (lv != AST_NONE && ast_kind(a, lv) == AST_Load &&
				ast_nchild(a, lv) >= 1) { MCC_TRACE("br\n");
			if ((uint32_t)lv < (uint32_t)nn)
				{ MCC_TRACE("br\n"); consumed[lv] = 1; }
			if (*nref >= AST_DEP_MAXREF)
				{ MCC_TRACE("br\n"); *overflow = 1; }
			else { MCC_TRACE("br\n");
				AstDepRef *r = &refs[*nref];
				ast_dep_decode(a, ast_first_child(a, lv), ivs, niv, r, consumed, nn);
				r->node = n;
				r->is_store = 1;
				(*nref)++;
			}
		}
	} else if (k == AST_Load && ast_nchild(a, n) >= 1) { MCC_TRACE("br\n");
		if (*nref >= AST_DEP_MAXREF)
			{ MCC_TRACE("br\n"); *overflow = 1; }
		else { MCC_TRACE("br\n");
			AstDepRef *r = &refs[*nref];
			ast_dep_decode(a, ast_first_child(a, n), ivs, niv, r, consumed, nn);
			r->node = n;
			r->is_store = 0;
			(*nref)++;
		}
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_dep_collect_rec(a, c, ivs, niv, refs, nref, overflow, consumed, nn); }
}

static AstDepRef *ast_dep_collect(AstArena *a, AstLocal body, const int *ivs,
																	int niv, int *nref, int *overflow) { MCC_TRACE("enter\n");
	int nn = (int)ast_count(a);
	unsigned char *consumed = mcc_mallocz((unsigned long)(nn > 0 ? nn : 1));
	AstDepRef *refs = mcc_malloc(sizeof(AstDepRef) * AST_DEP_MAXREF);
	*nref = 0;
	*overflow = 0;
	ast_dep_collect_rec(a, body, ivs, niv, refs, nref, overflow, consumed, nn);
	mcc_free(consumed);
	return refs;
}

static int ast_dep_base_same(const AstDepRef *r1, const AstDepRef *r2) { MCC_TRACE("enter\n");
	if (!r1->ok || !r2->ok || r1->base_kind != r2->base_kind)
		{ MCC_TRACE("br\n"); return 0; }
	if (r1->base_kind == 1)
		{ MCC_TRACE("br\n"); return r1->base_sym == r2->base_sym; }
	if (r1->base_kind == 2)
		{ MCC_TRACE("br\n"); return r1->base_off == r2->base_off; }
	return 0;
}

static int ast_dep_base_distinct(const AstDepRef *r1, const AstDepRef *r2,
																 int allow_indirect) { MCC_TRACE("enter\n");
	if (!r1->ok || !r2->ok)
		{ MCC_TRACE("br\n"); return 0; }
	if ((r1->indirect || r2->indirect) && !allow_indirect)
		{ MCC_TRACE("br\n"); return 0; }
	if (r1->base_kind == 1 && r2->base_kind == 1)
		{ MCC_TRACE("br\n"); return r1->base_sym != r2->base_sym; }
	return 0;
}

static int ast_dep_direction(const AstDepRef *r1, const AstDepRef *r2, int niv,
														 char *dir, const int64_t *stride) { MCC_TRACE("enter\n");
	for (int k = 0; k < niv; k++)
		{ MCC_TRACE("br\n"); dir[k] = '*'; }
	if (!r1->ok || !r2->ok || r1->ndim != r2->ndim)
		{ MCC_TRACE("br\n"); return AST_DEP_CONSERV; }
	int64_t D[AST_DEP_MAXIV];
	char Dset[AST_DEP_MAXIV];
	for (int k = 0; k < niv; k++) { MCC_TRACE("br\n");
		D[k] = 0;
		Dset[k] = 0;
	}
	for (int d = 0; d < r1->ndim; d++) { MCC_TRACE("br\n");
		const AstDepSub *s1 = &r1->sub[d], *s2 = &r2->sub[d];
		int c1n = 0, c2n = 0, u1 = -1, u2 = -1;
		for (int k = 0; k < niv; k++) { MCC_TRACE("br\n");
			if (s1->coeff[k]) { MCC_TRACE("br\n");
				c1n++;
				u1 = k;
			}
			if (s2->coeff[k]) { MCC_TRACE("br\n");
				c2n++;
				u2 = k;
			}
		}
		if (c1n == 0 && c2n == 0) { MCC_TRACE("br\n");
			if (s1->cst != s2->cst)
				{ MCC_TRACE("br\n"); return AST_DEP_INDEP; }
			continue;
		}
		if (c1n != 1 || c2n != 1 || u1 != u2)
			{ MCC_TRACE("br\n"); return AST_DEP_CONSERV; }
		int64_t A = s1->coeff[u1];
		if (A == 0 || A != s2->coeff[u2])
			{ MCC_TRACE("br\n"); return AST_DEP_CONSERV; }
		int64_t num = s1->cst - s2->cst;
		if (num % A != 0)
			{ MCC_TRACE("br\n"); return AST_DEP_INDEP; }
		int64_t dk = num / A;
		if (Dset[u1] && D[u1] != dk)
			{ MCC_TRACE("br\n"); return AST_DEP_INDEP; }
		D[u1] = dk;
		Dset[u1] = 1;
	}
	for (int k = 0; k < niv; k++) { MCC_TRACE("br\n");
		int64_t dk = D[k];
		if (stride && stride[k] < 0)
			{ MCC_TRACE("br\n"); dk = -dk; }
		dir[k] = Dset[k] ? (dk > 0 ? '<' : (dk < 0 ? '>' : '=')) : '*';
	}
	return AST_DEP_YES;
}

static int ast_dep_perfect_nest(AstArena *a, AstLocal outer, AstLocal inner) { MCC_TRACE("enter\n");
	AstLocal body, incr;
	ast_loop_parts(a, outer, ast_op(a, outer), &body, &incr);
	if (body == AST_NONE || ast_kind(a, body) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	int found = 0;
	for (AstLocal c = ast_first_child(a, body); c != AST_NONE;
			 c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		if (c == inner) { MCC_TRACE("br\n");
			found = 1;
			continue;
		}
		if (ast_licm_is_loop(a, c))
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_kind(a, c) == AST_Store) { MCC_TRACE("br\n");
			AstLocal lv = ast_child(a, c, 0);
			if (lv != AST_NONE && ast_kind(a, lv) == AST_Load)
				{ MCC_TRACE("br\n"); return 0; }
			continue;
		}
		return 0;
	}
	return found;
}

static int ast_dep_loop_start(AstArena *a, AstLocal loop, int iv_off,
															int64_t *start) { MCC_TRACE("enter\n");
	AstLocal bb = ast_parent(a, loop);
	if (bb == AST_NONE || ast_kind(a, bb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	int found = 0;
	int64_t val = 0;
	for (AstLocal c = ast_first_child(a, bb); c != AST_NONE && c != loop;
			 c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		if (ast_kind(a, c) == AST_Store && ast_nchild(a, c) >= 2 &&
				ast_ref_is_local_off(a, ast_child(a, c, 0), iv_off)) { MCC_TRACE("br\n");
			AstLocal v = ast_dep_strip(a, ast_child(a, c, 1));
			if (v != AST_NONE && ast_kind(a, v) == AST_Literal) { MCC_TRACE("br\n");
				val = (int64_t)ast_ival(a, v);
				found = 1;
			}
		}
	}
	*start = val;
	return found;
}

static int ast_dep_adjacent(AstArena *a, AstLocal l1, AstLocal l2) { MCC_TRACE("enter\n");
	AstLocal bb = ast_parent(a, l1);
	if (bb == AST_NONE || ast_parent(a, l2) != bb)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal c = ast_first_child(a, bb);
	while (c != AST_NONE && c != l1)
		{ MCC_TRACE("br\n"); c = ast_next_sib(a, c); }
	if (c != l1)
		{ MCC_TRACE("br\n"); return 0; }
	for (c = ast_next_sib(a, c); c != AST_NONE && c != l2; c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		if (ast_licm_is_loop(a, c))
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_kind(a, c) != AST_Store)
			{ MCC_TRACE("br\n"); return 0; }
		AstLocal lv = ast_child(a, c, 0);
		if (lv != AST_NONE && ast_kind(a, lv) == AST_Load)
			{ MCC_TRACE("br\n"); return 0; }
	}
	return c == l2;
}

static int ast_dep_same_trip(AstArena *a, AstLocal l1, AstLocal l2) { MCC_TRACE("enter\n");
	int64_t b1, b2;
	int lo1, lo2;
	if (!ast_loop_bounds(a, l1, &b1, &lo1) || !ast_loop_bounds(a, l2, &b2, &lo2))
		{ MCC_TRACE("br\n"); return 0; }
	if (b1 != b2 || lo1 != lo2)
		{ MCC_TRACE("br\n"); return 0; }
	int o1, o2, t;
	int64_t s1, s2;
	if (!ast_loop_iv(a, l1, &o1, &t, &s1) || !ast_loop_iv(a, l2, &o2, &t, &s2))
		{ MCC_TRACE("br\n"); return 0; }
	if (s1 != s2)
		{ MCC_TRACE("br\n"); return 0; }
	int64_t st1, st2;
	if (!ast_dep_loop_start(a, l1, o1, &st1) ||
			!ast_dep_loop_start(a, l2, o2, &st2))
		{ MCC_TRACE("br\n"); return 0; }
	return st1 == st2;
}

int ast_loop_interchange_legal(AstArena *a, AstLocal outer, AstLocal inner) { MCC_TRACE("enter\n");
	ast_loopnest_sync(a);
	if (!ast_loop_analyzable(a, outer) || !ast_loop_analyzable(a, inner))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_loop_parent(a, inner) != outer)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_dep_perfect_nest(a, outer, inner))
		{ MCC_TRACE("br\n"); return 0; }
	int ivo, ivi, t;
	int64_t sto = 0, sti = 0;
	if (!ast_loop_iv(a, outer, &ivo, &t, &sto) ||
			!ast_loop_iv(a, inner, &ivi, &t, &sti))
		{ MCC_TRACE("br\n"); return 0; }
	if (sto == 0 || sti == 0)
		{ MCC_TRACE("br\n"); return 0; }
	int ivs[2] = {ivo, ivi};
	int64_t ivstride[2] = {sto, sti};
	AstLocal body, incr;
	ast_loop_parts(a, inner, ast_op(a, inner), &body, &incr);
	int nref, overflow;
	AstDepRef *refs = ast_dep_collect(a, body, ivs, 2, &nref, &overflow);
	int legal = 1;
	if (overflow)
		{ MCC_TRACE("br\n"); legal = 0; }
	for (int i = 0; i < nref && legal; i++)
		{ MCC_TRACE("br\n"); for (int j = i; j < nref && legal; j++) { MCC_TRACE("br\n");
			if (!(refs[i].is_store || refs[j].is_store))
				{ MCC_TRACE("br\n"); continue; }
			if (ast_dep_base_distinct(&refs[i], &refs[j], 0))
				{ MCC_TRACE("br\n"); continue; }
			if (!ast_dep_base_same(&refs[i], &refs[j])) { MCC_TRACE("br\n");
				legal = 0;
				break;
			}
			char dir[2];
			int dep = ast_dep_direction(&refs[i], &refs[j], 2, dir, ivstride);
			if (dep == AST_DEP_INDEP)
				{ MCC_TRACE("br\n"); continue; }
			if (dep == AST_DEP_CONSERV) { MCC_TRACE("br\n");
				legal = 0;
				break;
			}
			int lead = -1;
			for (int k = 0; k < 2; k++)
				{ MCC_TRACE("br\n"); if (dir[k] != '=') { MCC_TRACE("br\n");
					lead = k;
					break;
				} }
			if (lead < 0)
				{ MCC_TRACE("br\n"); continue; }
			if (dir[lead] == '*') { MCC_TRACE("br\n");
				legal = 0;
				break;
			}
			if (dir[lead] == '>')
				{ MCC_TRACE("br\n"); for (int k = 0; k < 2; k++) { MCC_TRACE("br\n");
					if (dir[k] == '<')
						{ MCC_TRACE("br\n"); dir[k] = '>'; }
					else if (dir[k] == '>')
						{ MCC_TRACE("br\n"); dir[k] = '<'; }
				} }
			if (dir[0] == '*' || dir[1] == '*') { MCC_TRACE("br\n");
				legal = 0;
				break;
			}
			if (dir[0] == '<' && dir[1] == '>') { MCC_TRACE("br\n");
				legal = 0;
				break;
			}
		} }
	mcc_free(refs);
	return legal;
}

static int ast_dep_fusion_pair_illegal(const AstDepRef *r1,
																			 const AstDepRef *r2,
																			 int64_t stride) { MCC_TRACE("enter\n");
	if (!r1->ok || !r2->ok)
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_dep_base_distinct(r1, r2, 0))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_dep_base_same(r1, r2))
		{ MCC_TRACE("br\n"); return 1; }
	if (r1->ndim != r2->ndim)
		{ MCC_TRACE("br\n"); return 1; }
	int64_t D = 0;
	int Dset = 0;
	for (int d = 0; d < r1->ndim; d++) { MCC_TRACE("br\n");
		const AstDepSub *s1 = &r1->sub[d], *s2 = &r2->sub[d];
		int c1 = s1->coeff[0] != 0, c2 = s2->coeff[0] != 0;
		if (!c1 && !c2) { MCC_TRACE("br\n");
			if (s1->cst != s2->cst)
				{ MCC_TRACE("br\n"); return 0; }
			continue;
		}
		if (!c1 || !c2)
			{ MCC_TRACE("br\n"); return 1; }
		int64_t A = s1->coeff[0];
		if (A == 0 || A != s2->coeff[0])
			{ MCC_TRACE("br\n"); return 1; }
		int64_t num = s2->cst - s1->cst;
		if (num % A != 0)
			{ MCC_TRACE("br\n"); return 0; }
		int64_t dk = num / A;
		if (Dset && D != dk)
			{ MCC_TRACE("br\n"); return 0; }
		D = dk;
		Dset = 1;
	}
	if (!Dset)
		{ MCC_TRACE("br\n"); return 1; }
	if (stride < 0)
		{ MCC_TRACE("br\n"); D = -D; }
	return D > 0 ? 1 : 0;
}

int ast_loop_fusion_legal(AstArena *a, AstLocal loop1, AstLocal loop2) { MCC_TRACE("enter\n");
	ast_loopnest_sync(a);
	if (!ast_loop_analyzable(a, loop1) || !ast_loop_analyzable(a, loop2))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_dep_adjacent(a, loop1, loop2))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_dep_same_trip(a, loop1, loop2))
		{ MCC_TRACE("br\n"); return 0; }
	int o1, o2, t;
	int64_t s1 = 0, s2 = 0;
	if (!ast_loop_iv(a, loop1, &o1, &t, &s1) ||
			!ast_loop_iv(a, loop2, &o2, &t, &s2))
		{ MCC_TRACE("br\n"); return 0; }
	if (s1 == 0 || s1 != s2)
		{ MCC_TRACE("br\n"); return 0; }
	int iv1[1] = {o1}, iv2[1] = {o2};
	AstLocal b1, b2, incr;
	ast_loop_parts(a, loop1, ast_op(a, loop1), &b1, &incr);
	ast_loop_parts(a, loop2, ast_op(a, loop2), &b2, &incr);
	int n1, n2, ov1, ov2;
	AstDepRef *r1 = ast_dep_collect(a, b1, iv1, 1, &n1, &ov1);
	AstDepRef *r2 = ast_dep_collect(a, b2, iv2, 1, &n2, &ov2);
	int legal = 1;
	if (ov1 || ov2)
		{ MCC_TRACE("br\n"); legal = 0; }
	for (int i = 0; i < n1 && legal; i++)
		{ MCC_TRACE("br\n"); for (int j = 0; j < n2 && legal; j++) { MCC_TRACE("br\n");
			if (!(r1[i].is_store || r2[j].is_store))
				{ MCC_TRACE("br\n"); continue; }
			if (ast_dep_fusion_pair_illegal(&r1[i], &r2[j], s1)) { MCC_TRACE("br\n");
				legal = 0;
				break;
			}
		} }
	mcc_free(r1);
	mcc_free(r2);
	return legal;
}

static void ast_dep_nest_ivs(AstArena *a, AstLocal loop, int *ivs, int *niv) { MCC_TRACE("enter\n");
	int tmp[AST_DEP_MAXIV];
	int n = 0;
	for (AstLocal l = loop; l != AST_NONE; l = ast_loop_parent(a, l)) { MCC_TRACE("br\n");
		int off, t;
		int64_t st;
		if (n < AST_DEP_MAXIV && ast_loop_iv(a, l, &off, &t, &st))
			{ MCC_TRACE("br\n"); tmp[n++] = off; }
	}
	for (int i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); ivs[i] = tmp[n - 1 - i]; }
	*niv = n;
}

#define AST_PAR_MAXSCAL 64

typedef struct AstParScal {
	int off[AST_PAR_MAXSCAL];
	unsigned char killed[AST_PAR_MAXSCAL];
	unsigned char wrote[AST_PAR_MAXSCAL];
	unsigned char upread[AST_PAR_MAXSCAL];
	unsigned char condw[AST_PAR_MAXSCAL];
	int n;
	int bad;
	int carried;
} AstParScal;

static int ast_par_slot(AstParScal *s, int off) { MCC_TRACE("enter\n");
	for (int i = 0; i < s->n; i++)
		{ MCC_TRACE("br\n"); if (s->off[i] == off)
			{ MCC_TRACE("br\n"); return i; } }
	if (s->n >= AST_PAR_MAXSCAL) { MCC_TRACE("br\n");
		s->bad = 1;
		return -1;
	}
	int i = s->n++;
	s->off[i] = off;
	s->killed[i] = 0;
	s->wrote[i] = 0;
	s->upread[i] = 0;
	s->condw[i] = 0;
	return i;
}

static int ast_par_is_iv(const int *ivs, int niv, int off) { MCC_TRACE("enter\n");
	for (int i = 0; i < niv; i++)
		{ MCC_TRACE("br\n"); if (ivs[i] == off)
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_par_probe_call(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (!mcc_state->loop_census)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal f = ast_first_child(a, n);
	if (f == AST_NONE || ast_kind(a, f) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	Sym *s = (Sym *)(uintptr_t)ast_sym(a, f);
	if (!s)
		{ MCC_TRACE("br\n"); return 0; }
	return s->v == lcen_tok_enter || s->v == lcen_tok_exit;
}

static int ast_par_local_off(AstArena *a, AstLocal n, int *off) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	int r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
		{ MCC_TRACE("br\n"); return 0; }
	*off = (int)(int64_t)ast_ival(a, n);
	return 1;
}

static int ast_par_fixed_ref(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	int r = ast_op(a, n);
	return (r & VT_VALMASK) == VT_CONST && (r & VT_SYM) && ast_sym(a, n) != 0;
}

static void ast_par_read(AstArena *a, const int *ivs, int niv, AstParScal *s,
												 int off) { MCC_TRACE("enter\n");
	if (ast_par_is_iv(ivs, niv, off) || ast_lcen_slot(off))
		{ MCC_TRACE("br\n"); return; }
	if (ast_cprop_escapes(a, off)) { MCC_TRACE("br\n");
		s->bad = 1;
		return;
	}
	int i = ast_par_slot(s, off);
	if (i < 0)
		{ MCC_TRACE("br\n"); return; }
	if (!s->killed[i])
		{ MCC_TRACE("br\n"); s->upread[i] = 1; }
}

static void ast_par_write(AstArena *a, const int *ivs, int niv, AstParScal *s,
													int off, int cond) { MCC_TRACE("enter\n");
	if (ast_lcen_slot(off))
		{ MCC_TRACE("br\n"); return; }
	if (!ast_par_is_iv(ivs, niv, off) && ast_cprop_escapes(a, off)) { MCC_TRACE("br\n");
		s->bad = 1;
		return;
	}
	int i = ast_par_slot(s, off);
	if (i < 0)
		{ MCC_TRACE("br\n"); return; }
	s->wrote[i] = 1;
	if (cond)
		{ MCC_TRACE("br\n"); s->condw[i] = 1; }
	else
		{ MCC_TRACE("br\n"); s->killed[i] = 1; }
}

static int ast_par_op_safe(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case AST_OP_ADDR:
	case AST_OP_MEMBER:
	case AST_OP_MEMBER_ARROW:
	case AST_OP_IMAG:
	case AST_OP_MULHU:
	case AST_OP_MULHS:
	case AST_OP_FABS:
	case AST_OP_SQRT:
	case AST_OP_FLOOR:
	case AST_OP_CEIL:
	case AST_OP_TRUNC:
	case AST_OP_COPYSIGN:
	case AST_OP_ROUND:
	case AST_OP_FMIN:
	case AST_OP_FMAX:
	case AST_OP_RINT:
	case AST_OP_NEARBYINT:
	case AST_OP_FMA:
	case AST_OP_FNEG:
	case AST_OP_BSWAP:
	case AST_OP_SIGNBIT:
	case AST_OP_FFS:
	case AST_OP_BITSCAN:
	case AST_OP_CPLXBUILD:
		return 1;
	}
	return (op & 0xffff0000) != 0x40000;
}

static void ast_par_scan(AstArena *a, AstLocal n, const int *ivs, int niv,
												 int cond, AstParScal *s) { MCC_TRACE("enter\n");
	if (n == AST_NONE || s->bad)
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	int op = ast_op(a, n);
	if (k == AST_Invoke && ast_par_probe_call(a, n))
		{ MCC_TRACE("br\n"); return; }
	if (k == AST_Invoke || k == AST_Poison || k == AST_Return ||
			k == AST_Jump || k == AST_StoreVal || ast_op_is_asm(op)) { MCC_TRACE("br\n");
		s->bad = 1;
		return;
	}
	if ((k == AST_Unary || k == AST_Binary) && !ast_par_op_safe(op)) { MCC_TRACE("br\n");
		s->bad = 1;
		return;
	}
	if (ast_type_t(a, n) & VT_VOLATILE) { MCC_TRACE("br\n");
		s->bad = 1;
		return;
	}
	if (k == AST_Store && ast_nchild(a, n) >= 2 &&
			ast_kind(a, ast_child(a, n, 0)) == AST_Ref) { MCC_TRACE("br\n");
		int off;
		for (AstLocal c = ast_next_sib(a, ast_child(a, n, 0)); c != AST_NONE;
				 c = ast_next_sib(a, c))
			{ MCC_TRACE("br\n"); ast_par_scan(a, c, ivs, niv, cond, s); }
		if (ast_par_local_off(a, ast_child(a, n, 0), &off))
			{ MCC_TRACE("br\n"); ast_par_write(a, ivs, niv, s, off, cond); }
		else if (ast_par_fixed_ref(a, ast_child(a, n, 0)))
			{ MCC_TRACE("br\n"); s->carried = 1; }
		else
			{ MCC_TRACE("br\n"); s->bad = 1; }
		return;
	}
	if (k == AST_Unary && (op == TOK_INC || op == TOK_DEC)) { MCC_TRACE("br\n");
		int off;
		if (ast_par_local_off(a, ast_first_child(a, n), &off)) { MCC_TRACE("br\n");
			ast_par_read(a, ivs, niv, s, off);
			ast_par_write(a, ivs, niv, s, off, cond);
		} else if (ast_par_fixed_ref(a, ast_first_child(a, n)))
			{ MCC_TRACE("br\n"); s->carried = 1; }
		else
			{ MCC_TRACE("br\n"); s->bad = 1; }
		return;
	}
	if (k == AST_Ref) { MCC_TRACE("br\n");
		int off;
		if (ast_par_local_off(a, n, &off))
			{ MCC_TRACE("br\n"); ast_par_read(a, ivs, niv, s, off); }
		return;
	}
	int sub = cond || k == AST_If;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_par_scan(a, c, ivs, niv, sub, s); }
}

static int ast_par_encloses(AstArena *a, AstLocal outer, AstLocal loop) { MCC_TRACE("enter\n");
	for (AstLocal p = ast_loop_parent(a, loop); p != AST_NONE;
			 p = ast_loop_parent(a, p))
		{ MCC_TRACE("br\n"); if (p == outer)
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_par_ivs(AstArena *a, AstLocal loop, int *ivs, int *niv,
											 int *self) { MCC_TRACE("enter\n");
	int myoff, t;
	int64_t st;
	ast_dep_nest_ivs(a, loop, ivs, niv);
	if (!ast_loop_iv(a, loop, &myoff, &t, &st))
		{ MCC_TRACE("br\n"); return 0; }
	for (int i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
		AstLoopInfo *li = &ast_loopnest[i];
		if (!ast_par_encloses(a, loop, li->header))
			{ MCC_TRACE("br\n"); continue; }
		if (li->unanalyzable || !li->has_iv)
			{ MCC_TRACE("br\n"); return 0; }
		if (*niv >= AST_DEP_MAXIV)
			{ MCC_TRACE("br\n"); return 0; }
		ivs[(*niv)++] = li->iv_off;
	}
	*self = -1;
	for (int i = 0; i < *niv; i++) { MCC_TRACE("br\n");
		for (int j = 0; j < i; j++)
			{ MCC_TRACE("br\n"); if (ivs[i] == ivs[j])
				{ MCC_TRACE("br\n"); return 0; } }
		if (ivs[i] == myoff)
			{ MCC_TRACE("br\n"); *self = i; }
	}
	return *self >= 0;
}

static int ast_par_cond_pure(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Load)
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_par_cond_pure(a, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static const char *ast_par_why_s = "?";

const char *ast_loop_parallel_why(void) { MCC_TRACE("enter\n");
	return ast_par_why_s;
}

int ast_loop_parallel_legal(AstArena *a, AstLocal loop) { MCC_TRACE("enter\n");
	ast_loopnest_sync(a);
	ast_par_why_s = "ok";
	if (ast_loopnest_overflow)
		{ MCC_TRACE("br\n"); ast_par_why_s = "nest-overflow"; return -1; }
	if (!ast_loop_analyzable(a, loop))
		{ MCC_TRACE("br\n"); ast_par_why_s = "not-analyzable"; return -1; }
	int ivs[AST_DEP_MAXIV], niv = 0, self = -1;
	if (!ast_par_ivs(a, loop, ivs, &niv, &self))
		{ MCC_TRACE("br\n"); ast_par_why_s = "no-iv-nest"; return -1; }
	AstLocal body, incr;
	ast_loop_parts(a, loop, ast_op(a, loop), &body, &incr);
	if (body == AST_NONE)
		{ MCC_TRACE("br\n"); ast_par_why_s = "no-body"; return -1; }
	AstLoopInfo *li = ast_loop_find(a, loop);
	if (!li || li->bound_kind == AST_LOOP_BOUND_NONE)
		{ MCC_TRACE("br\n"); ast_par_why_s = "no-bound"; return -1; }
	if (!ast_par_cond_pure(a, li->cond))
		{ MCC_TRACE("br\n"); ast_par_why_s = "cond-loads"; return -1; }
	AstParScal sc;
	memset(&sc, 0, sizeof sc);
	for (AstLocal c = ast_first_child(a, loop); c != AST_NONE;
			 c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_par_scan(a, c, ivs, niv, 0, &sc); }
	if (sc.bad)
		{ MCC_TRACE("br\n"); ast_par_why_s = "body-unsafe"; return -1; }
	if (sc.carried)
		{ MCC_TRACE("br\n"); ast_par_why_s = "global-scalar-carried"; return 0; }
	for (int i = 0; i < sc.n; i++)
		{ MCC_TRACE("br\n"); if (sc.wrote[i] && sc.upread[i])
			{ MCC_TRACE("br\n"); ast_par_why_s = "scalar-carried"; return 0; } }
	for (int i = 0; i < sc.n; i++)
		{ MCC_TRACE("br\n"); if (sc.condw[i] && !sc.killed[i])
			{ MCC_TRACE("br\n"); ast_par_why_s = "cond-written-scalar"; return -1; } }
	int nref, overflow;
	AstDepRef *refs = ast_dep_collect(a, loop, ivs, niv, &nref, &overflow);
	int verdict = overflow ? -1 : 1;
	if (overflow)
		{ MCC_TRACE("br\n"); ast_par_why_s = "too-many-refs"; }
	for (int i = 0; i < nref && verdict == 1; i++) { MCC_TRACE("br\n");
		if (!refs[i].ok) { MCC_TRACE("br\n");
			ast_par_why_s = "ref-not-affine";
			verdict = -1;
			break;
		}
		if (refs[i].base_kind != 2)
			{ MCC_TRACE("br\n"); continue; }
		for (int j = 0; j < sc.n; j++)
			{ MCC_TRACE("br\n"); if (sc.off[j] == (int)refs[i].base_off && sc.wrote[j])
				{ MCC_TRACE("br\n"); ast_par_why_s = "base-is-written-scalar";
					verdict = -1; } }
	}
	for (int i = 0; i < nref && verdict == 1; i++)
		{ MCC_TRACE("br\n"); for (int j = i; j < nref && verdict == 1; j++) { MCC_TRACE("br\n");
			if (!(refs[i].is_store || refs[j].is_store))
				{ MCC_TRACE("br\n"); continue; }
			if (ast_dep_base_distinct(&refs[i], &refs[j],
																ast_dep_alias_oracle_env))
				{ MCC_TRACE("br\n"); continue; }
			if (!ast_dep_base_same(&refs[i], &refs[j])) { MCC_TRACE("br\n");
				ast_par_why_s = refs[i].indirect || refs[j].indirect
						? "bases-may-alias-indirect" : "bases-may-alias";
				verdict = -1;
				break;
			}
			if (refs[i].ndim == 0 && refs[j].ndim == 0) { MCC_TRACE("br\n");
				ast_par_why_s = "same-scalar-object";
				verdict = 0;
				break;
			}
			char dir[AST_DEP_MAXIV];
			int dep = ast_dep_direction(&refs[i], &refs[j], niv, dir, NULL);
			if (dep == AST_DEP_INDEP)
				{ MCC_TRACE("br\n"); continue; }
			if (dep == AST_DEP_CONSERV) { MCC_TRACE("br\n");
				ast_par_why_s = "subscript-not-comparable";
				verdict = -1;
				break;
			}
			if (dir[self] == '=')
				{ MCC_TRACE("br\n"); continue; }
			ast_par_why_s = dir[self] == '<' || dir[self] == '>'
					? "dep-carried" : "dep-direction-unknown";
			verdict = dir[self] == '<' || dir[self] == '>' ? 0 : -1;
			break;
		} }
	mcc_free(refs);
	return verdict;
}


#define AST_RGN_MAXBASE 48

typedef struct AstRgnBase {
	int kind;
	uint64_t sym;
	int64_t off;
	int store;
	int indirect;
	int ok;
} AstRgnBase;

typedef struct AstRgnSet {
	int n;
	int ovf;
	int opaque;
	AstRgnBase b[AST_RGN_MAXBASE];
} AstRgnSet;

static const char *ast_rgn_why_s = "";

const char *ast_region_disjoint_why(void) { MCC_TRACE("enter\n");
	return ast_rgn_why_s;
}

static int ast_rgn_kind_modelled(uint16_t k) { MCC_TRACE("enter\n");
	return k == AST_BasicBlock || k == AST_If || k == AST_Return ||
				 k == AST_Ref || k == AST_Literal || k == AST_Load ||
				 k == AST_Store || k == AST_Unary || k == AST_Binary ||
				 k == AST_Convert;
}

static int ast_region_opaque(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	uint16_t k = ast_kind(a, n);
	int op = ast_op(a, n);
	if (!ast_rgn_kind_modelled(k))
		{ MCC_TRACE("br\n"); return 1; }
	if ((k == AST_Unary || k == AST_Binary) &&
			(ast_op_is_asm(op) || op == AST_OP_VLA || op == AST_OP_VLA_RESTORE))
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_type_t(a, n) & VT_VOLATILE)
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_region_opaque(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static void ast_rgn_add(AstRgnSet *s, int kind, uint64_t sym, int64_t off,
												int store, int indirect, int ok) { MCC_TRACE("enter\n");
	for (int i = 0; i < s->n; i++)
		{ MCC_TRACE("br\n"); if (s->b[i].ok == ok && s->b[i].kind == kind &&
				s->b[i].sym == sym && s->b[i].off == off &&
				s->b[i].indirect == indirect) { MCC_TRACE("br\n");
			s->b[i].store |= store;
			return;
		} }
	if (s->n >= AST_RGN_MAXBASE) { MCC_TRACE("br\n");
		s->ovf = 1;
		return;
	}
	s->b[s->n].kind = kind;
	s->b[s->n].sym = sym;
	s->b[s->n].off = off;
	s->b[s->n].store = store;
	s->b[s->n].indirect = indirect;
	s->b[s->n].ok = ok;
	s->n++;
}

static int ast_rgn_ref_base(AstArena *a, AstLocal n, int *kind, uint64_t *sym,
														int64_t *off) { MCC_TRACE("enter\n");
	int r;
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	r = ast_op(a, n);
	if ((r & VT_VALMASK) == VT_CONST && (r & VT_SYM) && ast_sym(a, n)) { MCC_TRACE("br\n");
		*kind = 1;
		*sym = ast_sym(a, n);
		*off = 0;
		return 1;
	}
	if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) { MCC_TRACE("br\n");
		*kind = 2;
		*sym = 0;
		*off = (int64_t)ast_ival(a, n);
		return 1;
	}
	return 0;
}

static void ast_rgn_touch(AstArena *a, AstLocal n, AstRgnSet *s, int store) { MCC_TRACE("enter\n");
	int kind = 0;
	uint64_t sym = 0;
	int64_t off = 0;
	if (ast_rgn_ref_base(a, n, &kind, &sym, &off))
		{ MCC_TRACE("br\n"); ast_rgn_add(s, kind, sym, off, store, 0, 1); }
	else
		{ MCC_TRACE("br\n"); ast_rgn_add(s, 0, 0, 0, store, 0, 0); }
}

static void ast_rgn_scalars(AstArena *a, AstLocal n, AstRgnSet *s) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	uint16_t k = ast_kind(a, n);
	if (k == AST_Store && ast_nchild(a, n) >= 2) { MCC_TRACE("br\n");
		AstLocal d = ast_child(a, n, 0);
		if (ast_kind(a, d) == AST_Ref)
			{ MCC_TRACE("br\n"); ast_rgn_touch(a, d, s, 1); }
		else if (ast_kind(a, d) != AST_Load)
			{ MCC_TRACE("br\n"); ast_rgn_add(s, 0, 0, 0, 1, 0, 0); }
		for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
				 c = ast_next_sib(a, c))
			{ MCC_TRACE("br\n"); if (c != d || ast_kind(a, d) != AST_Ref)
				{ MCC_TRACE("br\n"); ast_rgn_scalars(a, c, s); } }
		return;
	}
	if (k == AST_Unary && (ast_op(a, n) == TOK_INC || ast_op(a, n) == TOK_DEC) &&
			ast_nchild(a, n) >= 1) { MCC_TRACE("br\n");
		AstLocal d = ast_first_child(a, n);
		if (ast_kind(a, d) == AST_Ref)
			{ MCC_TRACE("br\n"); ast_rgn_touch(a, d, s, 1); }
		else
			{ MCC_TRACE("br\n"); ast_rgn_add(s, 0, 0, 0, 1, 0, 0);
				ast_rgn_scalars(a, d, s); }
		return;
	}
	if (k == AST_Ref) { MCC_TRACE("br\n");
		ast_rgn_touch(a, n, s, 0);
		return;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_rgn_scalars(a, c, s); }
}

static void ast_rgn_collect(AstArena *a, AstLocal root, AstRgnSet *s) { MCC_TRACE("enter\n");
	int ivs[AST_DEP_MAXIV], nref = 0, ovf = 0;
	AstDepRef *refs;
	for (int i = 0; i < AST_DEP_MAXIV; i++)
		{ MCC_TRACE("br\n"); ivs[i] = -1; }
	if (root == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	if (ast_region_opaque(a, root))
		{ MCC_TRACE("br\n"); s->opaque = 1; }
	refs = ast_dep_collect(a, root, ivs, 0, &nref, &ovf);
	if (ovf)
		{ MCC_TRACE("br\n"); s->ovf = 1; }
	for (int i = 0; i < nref; i++)
		{ MCC_TRACE("br\n"); ast_rgn_add(s, refs[i].base_kind, refs[i].base_sym,
											refs[i].base_off, refs[i].is_store,
											refs[i].indirect, refs[i].ok); }
	mcc_free(refs);
	ast_rgn_scalars(a, root, s);
}

static void ast_rgn_as_dep(const AstRgnBase *b, AstDepRef *r) { MCC_TRACE("enter\n");
	memset(r, 0, sizeof *r);
	r->ok = b->ok;
	r->base_kind = b->kind;
	r->base_sym = b->sym;
	r->base_off = b->off;
	r->indirect = b->indirect;
	r->is_store = b->store;
}

static int ast_rgn_pair(const AstRgnSet *x, const AstRgnSet *y,
												const char **why) { MCC_TRACE("enter\n");
	*why = "disjoint";
	if (x->opaque || y->opaque) { MCC_TRACE("br\n");
		*why = "opaque-effect";
		return -1;
	}
	if (x->ovf || y->ovf) { MCC_TRACE("br\n");
		*why = "too-many-bases";
		return -1;
	}
	for (int i = 0; i < x->n; i++)
		{ MCC_TRACE("br\n"); for (int j = 0; j < y->n; j++) { MCC_TRACE("br\n");
			AstDepRef r1, r2;
			if (!(x->b[i].store || y->b[j].store))
				{ MCC_TRACE("br\n"); continue; }
			if (!x->b[i].ok || !y->b[j].ok) { MCC_TRACE("br\n");
				*why = "ref-not-resolved";
				return -1;
			}
			ast_rgn_as_dep(&x->b[i], &r1);
			ast_rgn_as_dep(&y->b[j], &r2);
			if (ast_dep_base_distinct(&r1, &r2, 0))
				{ MCC_TRACE("br\n"); continue; }
			if (ast_dep_base_same(&r1, &r2)) { MCC_TRACE("br\n");
				*why = "same-object";
				return 0;
			}
			*why = r1.indirect || r2.indirect ? "bases-may-alias-indirect"
																			 : "bases-may-alias";
			return -1;
		} }
	return 1;
}

int ast_region_disjoint(AstArena *a, AstLocal r1, AstLocal r2) { MCC_TRACE("enter\n");
	static AstRgnSet s1, s2;
	ast_rgn_why_s = "disjoint";
	if (!a || r1 == AST_NONE || r2 == AST_NONE) { MCC_TRACE("br\n");
		ast_rgn_why_s = "no-region";
		return -1;
	}
	if (r1 == r2) { MCC_TRACE("br\n");
		ast_rgn_why_s = "same-region";
		return 0;
	}
	memset(&s1, 0, sizeof s1);
	memset(&s2, 0, sizeof s2);
	ast_rgn_collect(a, r1, &s1);
	ast_rgn_collect(a, r2, &s2);
	return ast_rgn_pair(&s1, &s2, &ast_rgn_why_s);
}

#define AST_THR_MAXSEC 256

typedef struct AstThrSec {
	uint64_t lock;
	int id;
	AstRgnSet set;
} AstThrSec;

static int ast_slc_callee_sym(const AstArena *a, AstLocal inv, void **out);
static int ast_slc_invclass(const AstArena *a, AstLocal inv);

static FILE *ast_thr_fp;
static int ast_thr_on, ast_thr_tried;
static AstThrSec *ast_thr_sec;
static int ast_thr_nsec;

static void ast_thr_open(void) { MCC_TRACE("enter\n");
	const char *p;
	if (ast_thr_on || ast_thr_tried)
		{ MCC_TRACE("br\n"); return; }
	ast_thr_tried = 1;
	p = getenv("MCC_THREAD_CENSUS");
	if (!p || !p[0])
		{ MCC_TRACE("br\n"); return; }
	ast_thr_fp = (p[0] == '-' && !p[1]) ? stderr : fopen(p, "a");
	if (!ast_thr_fp)
		{ MCC_TRACE("br\n"); return; }
	ast_thr_sec = mcc_mallocz(sizeof(AstThrSec) * AST_THR_MAXSEC);
	if (!ast_thr_sec)
		{ MCC_TRACE("br\n"); return; }
	setvbuf(ast_thr_fp, NULL, _IOLBF, BUFSIZ);
	ast_thr_on = 1;
}

static const char *ast_thr_callee(const AstArena *a, AstLocal inv) { MCC_TRACE("enter\n");
	void *cs;
	if (!ast_slc_callee_sym(a, inv, &cs))
		{ MCC_TRACE("br\n"); return NULL; }
	return get_tok_str(((Sym *)cs)->v, NULL);
}

static int ast_thr_op_of(const AstArena *a, AstLocal inv) { MCC_TRACE("enter\n");
	const char *nm;
	if (ast_kind(a, inv) != AST_Invoke)
		{ MCC_TRACE("br\n"); return MCC_THR_NONE; }
	if (ast_slc_invclass(a, inv) != 1)
		{ MCC_TRACE("br\n"); return MCC_THR_NONE; }
	nm = ast_thr_callee(a, inv);
	return mcc_thread_classify(nm);
}

static uint64_t ast_thr_lock_key(AstArena *a, AstLocal inv) { MCC_TRACE("enter\n");
	AstLocal arg = ast_child(a, inv, 1);
	arg = ast_dep_strip(a, arg);
	while (arg != AST_NONE && ast_kind(a, arg) == AST_Unary &&
				 ast_op(a, arg) == AST_OP_ADDR && ast_nchild(a, arg) == 1)
		{ MCC_TRACE("br\n"); arg = ast_dep_strip(a, ast_first_child(a, arg)); }
	if (arg == AST_NONE || ast_kind(a, arg) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_sym(a, arg);
}

static int ast_thr_find_op(AstArena *a, AstLocal n, int want, AstLocal *at) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Invoke && ast_thr_op_of(a, n) == want) { MCC_TRACE("br\n");
		*at = n;
		return 1;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_thr_find_op(a, c, want, at))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static void ast_thr_close(AstArena *a, const char *fn, uint64_t lock,
													AstLocal from, AstLocal to) { MCC_TRACE("enter\n");
	AstThrSec *sec;
	int nstmt = 0;
	if (ast_thr_nsec >= AST_THR_MAXSEC)
		{ MCC_TRACE("br\n"); return; }
	sec = &ast_thr_sec[ast_thr_nsec];
	memset(sec, 0, sizeof *sec);
	sec->lock = lock;
	sec->id = ast_thr_nsec;
	for (AstLocal s = from; s != AST_NONE && s != to; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		ast_rgn_collect(a, s, &sec->set);
		nstmt++;
	}
	fprintf(ast_thr_fp,
					"[thrsec] fn=%s lock=%llu id=%d stmts=%d bases=%d opaque=%d ovf=%d\n",
					fn ? fn : "?", (unsigned long long)lock, sec->id, nstmt, sec->set.n,
					sec->set.opaque, sec->set.ovf);
	for (int i = 0; i < ast_thr_nsec; i++) { MCC_TRACE("br\n");
		const char *why;
		int v;
		if (ast_thr_sec[i].lock != lock)
			{ MCC_TRACE("br\n"); continue; }
		v = ast_rgn_pair(&ast_thr_sec[i].set, &sec->set, &why);
		fprintf(ast_thr_fp, "[thrpair] lock=%llu a=%d b=%d verdict=%s why=%s\n",
						(unsigned long long)lock, ast_thr_sec[i].id, sec->id,
						v > 0 ? "independent" : v == 0 ? "conflict" : "unknown", why);
	}
	ast_thr_nsec++;
}

static void ast_thr_block(AstArena *a, const char *fn, AstLocal blk) { MCC_TRACE("enter\n");
	for (AstLocal s = ast_first_child(a, blk); s != AST_NONE;
			 s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		AstLocal lk = AST_NONE;
		uint64_t key;
		if (!ast_thr_find_op(a, s, MCC_THR_LOCK, &lk))
			{ MCC_TRACE("br\n"); continue; }
		key = ast_thr_lock_key(a, lk);
		if (!key)
			{ MCC_TRACE("br\n"); continue; }
		for (AstLocal e = ast_next_sib(a, s); e != AST_NONE; e = ast_next_sib(a, e)) { MCC_TRACE("br\n");
			AstLocal ul = AST_NONE;
			if (!ast_thr_find_op(a, e, MCC_THR_UNLOCK, &ul))
				{ MCC_TRACE("br\n"); continue; }
			if (ast_thr_lock_key(a, ul) != key)
				{ MCC_TRACE("br\n"); continue; }
			ast_thr_close(a, fn, key, ast_next_sib(a, s), e);
			s = e;
			break;
		}
	}
}

static void ast_thread_census(AstArena *a, const char *fn) { MCC_TRACE("enter\n");
	AstLocal nn;
	ast_thr_open();
	if (!ast_thr_on || !a)
		{ MCC_TRACE("br\n"); return; }
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int op;
		if (ast_kind(a, n) != AST_Invoke)
			{ MCC_TRACE("br\n"); continue; }
		op = ast_thr_op_of(a, n);
		if (op == MCC_THR_NONE)
			{ MCC_TRACE("br\n"); continue; }
		fprintf(ast_thr_fp, "[throp] fn=%s op=%s supported=%d\n", fn ? fn : "?",
						mcc_thread_op_name(op), mcc_thread_op_supported(op));
	}
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_BasicBlock)
			{ MCC_TRACE("br\n"); ast_thr_block(a, fn, n); } }
}

static void ast_dep_dump_refs(AstArena *a, AstLocal loop) { MCC_TRACE("enter\n");
	if (!ast_loop_analyzable(a, loop))
		{ MCC_TRACE("br\n"); return; }
	int ivs[AST_DEP_MAXIV], niv;
	ast_dep_nest_ivs(a, loop, ivs, &niv);
	AstLocal body, incr;
	ast_loop_parts(a, loop, ast_op(a, loop), &body, &incr);
	int nref, overflow;
	AstDepRef *refs = ast_dep_collect(a, body, ivs, niv, &nref, &overflow);
	for (int i = 0; i < nref; i++) { MCC_TRACE("br\n");
		AstDepRef *r = &refs[i];
		fprintf(stderr, "    %s ", r->is_store ? "store" : "load ");
		if (!r->ok) { MCC_TRACE("br\n");
			fprintf(stderr, "base=? (non-affine/indirect)\n");
			continue;
		}
		if (r->base_kind == 1)
			{ MCC_TRACE("br\n"); fprintf(stderr, "base=sym#%llu", (unsigned long long)r->base_sym); }
		else
			{ MCC_TRACE("br\n"); fprintf(stderr, "base=@%lld", (long long)r->base_off); }
		if (r->indirect)
			{ MCC_TRACE("br\n"); fprintf(stderr, " INDIRECT"); }
		for (int d = 0; d < r->ndim; d++) { MCC_TRACE("br\n");
			fprintf(stderr, "[");
			int first = 1;
			for (int k = 0; k < niv; k++)
				{ MCC_TRACE("br\n"); if (r->sub[d].coeff[k]) { MCC_TRACE("br\n");
					fprintf(stderr, "%s%lld*@%d", first ? "" : "+",
									(long long)r->sub[d].coeff[k], ivs[k]);
					first = 0;
				} }
			if (r->sub[d].cst || first)
				{ MCC_TRACE("br\n"); fprintf(stderr, "%s%lld", first ? "" : "+", (long long)r->sub[d].cst); }
			fprintf(stderr, "]");
		}
		fprintf(stderr, "\n");
	}
	mcc_free(refs);
}

static void ast_loop_par_census(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal hdr[AST_LOOPNEST_CAP];
	int n;
	if (!lcen_on || lcen_fn_ovf || !lcen_fn_n)
		{ MCC_TRACE("br\n"); return; }
	ast_loopnest_sync(a);
	if (ast_loopnest_overflow || ast_loopnest_n != lcen_fn_n)
		{ MCC_TRACE("br\n"); return; }
	n = ast_loopnest_n;
	for (int i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); hdr[i] = ast_loopnest[i].header; }
	for (int i = 0; i < n; i++) { MCC_TRACE("br\n");
		int p = ast_loop_parallel_legal(a, hdr[i]);
		fprintf(lcen_fp, "[loopar] id=%d par=%s why=%s\n", lcen_fn_ids[i],
						p > 0 ? "1" : p == 0 ? "0" : "?", ast_loop_parallel_why());
	}
}

/* `when` names the point in the pipeline this dump was taken at, and it is not
 * decoration. The dump used to run only at the top of the driver while
 * ast_interchange_run() and ast_fusion_run() run near the bottom, after the tree
 * has been rewritten -- so a verdict here did not predict what the transforms
 * would do, and read exactly as if it did. On the two-loop reducer it printed
 * `fusion(#7,#30): ILLEGAL` and fusion then fired on that same pair in the same
 * compile, because ast_dep_same_trip() bails early at dump time and succeeds by
 * the time the transform asks. That cost ~40 minutes chasing a phantom bug.
 *
 * It is now called twice, and every line says which run it came from. Compare
 * the two: a pair that is ILLEGAL at pre-opt and legal at transform is the tree
 * having been rewritten in between, not a legality bug. */
void ast_loopdep_dump(AstArena *a, const char *fname, const char *when) { MCC_TRACE("enter\n");
	ast_loopnest_sync(a);
	fprintf(stderr, "[LOOPDEP@%s] %s: %d loop(s)\n", when, fname ? fname : "?",
					ast_loopnest_n);
	for (int i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
		AstLoopInfo *li = &ast_loopnest[i];
		if (li->unanalyzable || !li->has_iv)
			{ MCC_TRACE("br\n"); continue; }
		fprintf(stderr, "  [%s] loop#%u refs:\n", when, (unsigned)li->header);
		ast_dep_dump_refs(a, li->header);
	}
	for (int i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
		AstLocal h = ast_loopnest[i].header;
		int p = ast_loop_parallel_legal(a, h);
		fprintf(stderr, "  [%s] parallel(#%u): %s\n", when, (unsigned)h,
						p > 0 ? "legal" : p == 0 ? "CARRIED" : "unknown");
	}
	for (int i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
		AstLoopInfo *li = &ast_loopnest[i];
		if (li->parent == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		int lg = ast_loop_interchange_legal(a, li->parent, li->header);
		fprintf(stderr, "  [%s] interchange(outer#%u,inner#%u): %s\n", when,
						(unsigned)li->parent, (unsigned)li->header,
						lg ? "legal" : "ILLEGAL");
	}
	for (int i = 0; i < ast_loopnest_n; i++)
		{ MCC_TRACE("br\n"); for (int j = 0; j < ast_loopnest_n; j++) { MCC_TRACE("br\n");
			if (i == j)
				{ MCC_TRACE("br\n"); continue; }
			if (ast_dep_adjacent(a, ast_loopnest[i].header, ast_loopnest[j].header)) { MCC_TRACE("br\n");
				int lg = ast_loop_fusion_legal(a, ast_loopnest[i].header,
																			 ast_loopnest[j].header);
				fprintf(stderr, "  [%s] fusion(#%u,#%u): %s\n", when,
								(unsigned)ast_loopnest[i].header,
								(unsigned)ast_loopnest[j].header, lg ? "legal" : "ILLEGAL");
			}
		} }
}

#define AST_SLC_RUNMAX 512

#define AST_SLC_K_STORE (1u << 0)
#define AST_SLC_K_LOAD (1u << 1)
#define AST_SLC_K_BINARY (1u << 2)
#define AST_SLC_K_UNARY (1u << 3)
#define AST_SLC_K_CONVERT (1u << 4)
#define AST_SLC_K_LITERAL (1u << 5)
#define AST_SLC_K_REF (1u << 6)
#define AST_SLC_K_JUMP (1u << 7)
#define AST_SLC_K_RETURN (1u << 8)
#define AST_SLC_K_STOREVAL (1u << 9)
#define AST_SLC_K_POISON (1u << 10)
#define AST_SLC_K_BB (1u << 11)
#define AST_SLC_K_IF (1u << 12)
#define AST_SLC_K_WHILE (1u << 13)
#define AST_SLC_K_FOR (1u << 14)
#define AST_SLC_K_DO (1u << 15)
#define AST_SLC_K_SWITCH (1u << 16)
#define AST_SLC_K_TERNARY (1u << 17)
#define AST_SLC_K_INVOKE (1u << 18)
#define AST_SLC_K_ASM (1u << 19)
#define AST_SLC_K_FLOAT (1u << 20)

static FILE *ast_adump_fp;
static int ast_adump_on;

static int ast_adump_tried;
static unsigned ast_adump_icap_max;

static void ast_adump_open(void) { MCC_TRACE("enter\n");
	const char *p;
	if (ast_adump_on || ast_adump_tried)
		{ MCC_TRACE("br\n"); return; }
	ast_adump_tried = 1;
	p = getenv("MCC_ARENA_DUMP");
	if (!p || !p[0])
		{ MCC_TRACE("br\n"); return; }
	ast_adump_fp = (p[0] == '-' && !p[1]) ? stderr : fopen(p, "a");
	if (!ast_adump_fp)
		{ MCC_TRACE("br\n"); return; }
	setvbuf(ast_adump_fp, NULL, _IOLBF, BUFSIZ);
	p = getenv("MCC_ARENA_DUMP_ICAP");
	if (p && p[0]) { MCC_TRACE("br\n");
		unsigned v = (unsigned)strtoul(p, NULL, 10), q = 8;
		while (q * 2 <= v && q < 0x40000000u)
			q *= 2;
		ast_adump_icap_max = v ? q : 0;
	}
	ast_adump_on = 1;
}

/* `sym` and `type_ref` hold Sym and CType pointers. Emitting them raw made
 * the dump vary run to run under ASLR, which is N7: it broke the byte-identity
 * the H4' bank rested on, and it made the two columns useless to any
 * consumer,
 * because an address is not an identity anyone downstream can match on.
 * Interning by first-encounter order fixes both -- the order is deterministic
 * for a deterministic compile, so the ids are stable across runs, and they are
 * dense small integers a rebuilt arena can actually use. */
#define AST_ADUMP_ICAP 8192
static uintptr_t *ast_adump_ikey;
static unsigned *ast_adump_ival_;
static unsigned ast_adump_icap;
static unsigned ast_adump_in;
static int ast_adump_ifull;

static void ast_adump_ifail(void) { MCC_TRACE("enter\n");
	if (ast_adump_ifull)
		{ MCC_TRACE("br\n"); return; }
	ast_adump_ifull = 1;
	fprintf(stderr,
					"mcc: MCC_ARENA_DUMP: the identity intern table could not grow "
					"past %u entries after %u distinct symbols. Column 11 is an "
					"identity a consumer names objects by, so a reused id would merge "
					"two distinct globals into one. Truncating the dump rather than "
					"degrading it.\n",
					ast_adump_icap, ast_adump_in);
	if (ast_adump_fp) { MCC_TRACE("br\n");
		fprintf(ast_adump_fp, "[intern-overflow] n=%u cap=%u\n", ast_adump_in,
						ast_adump_icap);
		fflush(ast_adump_fp);
	}
	ast_adump_on = 0;
}

static int ast_adump_igrow(void) { MCC_TRACE("enter\n");
	unsigned ncap = ast_adump_icap ? ast_adump_icap * 2 : AST_ADUMP_ICAP, i;
	uintptr_t *nk;
	unsigned *nv;
	if (!ast_adump_icap && ast_adump_icap_max && ast_adump_icap_max < ncap)
		{ MCC_TRACE("br\n"); ncap = ast_adump_icap_max; }
	if (ncap <= ast_adump_icap ||
			(ast_adump_icap_max && ncap > ast_adump_icap_max))
		{ MCC_TRACE("br\n"); return 0; }
	nk = (uintptr_t *)mcc_mallocz((unsigned long)ncap * sizeof *nk);
	nv = (unsigned *)mcc_mallocz((unsigned long)ncap * sizeof *nv);
	if (!nk || !nv) { MCC_TRACE("br\n");
		mcc_free(nk);
		mcc_free(nv);
		return 0;
	}
	for (i = 0; i < ast_adump_icap; i++) { MCC_TRACE("br\n");
		unsigned h, j;
		if (!ast_adump_ikey[i])
			continue;
		h = (unsigned)((ast_adump_ikey[i] * 0x9e3779b1u) >> 13) & (ncap - 1);
		for (j = 0; j < ncap; j++) { MCC_TRACE("br\n");
			unsigned k = (h + j) & (ncap - 1);
			if (!nk[k]) { MCC_TRACE("br\n");
				nk[k] = ast_adump_ikey[i];
				nv[k] = ast_adump_ival_[i];
				break;
			}
		}
	}
	mcc_free(ast_adump_ikey);
	mcc_free(ast_adump_ival_);
	ast_adump_ikey = nk;
	ast_adump_ival_ = nv;
	ast_adump_icap = ncap;
	return 1;
}

static unsigned ast_adump_intern(uintptr_t p) { MCC_TRACE("enter\n");
	unsigned h, i;
	if (!p || ast_adump_ifull)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_adump_in + 1 >= ast_adump_icap / 2 && !ast_adump_igrow()) { MCC_TRACE("br\n");
		unsigned k;
		for (k = 0; ast_adump_icap && k < ast_adump_icap; k++) { MCC_TRACE("br\n");
			unsigned q = (unsigned)(((p * 0x9e3779b1u) >> 13) + k) &
									 (ast_adump_icap - 1);
			if (ast_adump_ikey[q] == p)
				{ MCC_TRACE("br\n"); return ast_adump_ival_[q]; }
			if (!ast_adump_ikey[q])
				break;
		}
		ast_adump_ifail();
		return 0;
	}
	h = (unsigned)((p * 0x9e3779b1u) >> 13) & (ast_adump_icap - 1);
	for (i = 0; i < ast_adump_icap; i++) { MCC_TRACE("br\n");
		unsigned k = (h + i) & (ast_adump_icap - 1);
		if (ast_adump_ikey[k] == p)
			{ MCC_TRACE("br\n"); return ast_adump_ival_[k]; }
		if (!ast_adump_ikey[k]) { MCC_TRACE("br\n");
			ast_adump_ikey[k] = p;
			ast_adump_ival_[k] = ++ast_adump_in;
			return ast_adump_ival_[k];
		}
	}
	ast_adump_ifail();
	return 0;
}

/* Byte size of the object a node denotes, or 0 when it is not known. A device
 * address space needs this and nothing else in the dump carries it: to bound a
 * runtime index you must know the extent of the object being indexed, and
 * without it the only choices are masking against the whole lane region (a
 * legitimate arr[3] then silently reads the wrong word) or poisoning every
 * index that is not provably in range (which poisons the legitimate ones too).
 * Computed from the real CType before it is interned, because the dumped
 * type_ref column is a dense id, not a pointer. */
static int ast_adump_size(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	CType ct;
	int align = 0, sz;
	uint64_t ref = ast_type_ref(a, n);
	int t = ast_type_t(a, n);
	if (!t || (t & VT_BTYPE) == VT_FUNC)
		{ MCC_TRACE("br\n"); return 0; }
	memset(&ct, 0, sizeof ct);
	ct.t = t;
	ct.ref = (Sym *)(uintptr_t)ref;
	sz = type_size(&ct, &align);
	return sz > 0 ? sz : 0;
}

/* The element type word of an array or pointer node, 0 otherwise. The extent
 * column above is not on its own enough to place a runtime index: `arr[i]`
 * replays as gen_op('+') on a VT_ARRAY base, which scales i by the ELEMENT
 * size, and neither the Binary nor the Load above it carries a type at all --
 * both are 0 in every real arena. So the extent gives no element count to
 * bound i against and no width to narrow the stored value to, and a consumer
 * with only the extent would have to guess both. This word gives both, and
 * follows the extent's convention: computed from the real CType before
 * interning, 0 meaning unknown so an older dump refuses rather than guesses. */
static int ast_adump_etype(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	CType ct;
	int t = ast_type_t(a, n);
	if (!t || (t & VT_BTYPE) != VT_PTR)
		{ MCC_TRACE("br\n"); return 0; }
	memset(&ct, 0, sizeof ct);
	ct.t = t;
	ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
	if (!ct.ref)
		{ MCC_TRACE("br\n"); return 0; }
	return pointed_type(&ct)->t;
}

static AstArena *ast_slice_leaf_inline(AstArena *a);

static void ast_adump_body(AstArena *a, const char *fname) { MCC_TRACE("enter\n");
	AstLocal nn, n;
	AstArena *g;
	ast_adump_open();
	if (!ast_adump_on || !a)
		{ MCC_TRACE("br\n"); return; }
	nn = ast_count(a);
	if (!nn)
		{ MCC_TRACE("br\n"); return; }
	g = ast_slice_leaf_inline(a);
	if (g) { MCC_TRACE("br\n");
		a = g;
		nn = ast_count(a);
	}
	fprintf(ast_adump_fp, "[arena] fn=%s n=%ld root=%ld\n", fname ? fname : "?",
					(long)nn, (long)ast_root(a));
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		fprintf(ast_adump_fp,
						"%ld %d %d %d %lld %ld %ld %llu %u %u %llu %llu %d %d\n",
						(long)n, (int)ast_kind(a, n), ast_op(a, n), ast_type_t(a, n),
						(long long)ast_ival(a, n), (long)ast_first_child(a, n),
						(long)ast_next_sib(a, n),
						(unsigned long long)ast_adump_intern(
								(uintptr_t)ast_type_ref(a, n)),
						ast_type_bp(a, n), ast_type_bs(a, n),
						(unsigned long long)ast_adump_intern((uintptr_t)ast_sym(a, n)),
						(unsigned long long)ast_fbits(a, n), ast_adump_size(a, n),
						ast_adump_etype(a, n));
	}
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstLocal cref;
		Sym *cs;
		if (ast_kind(a, n) != AST_Invoke)
			continue;
		cref = ast_child(a, n, 0);
		cs = (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
						 ? NULL
						 : (Sym *)(uintptr_t)ast_sym(a, cref);
		fprintf(ast_adump_fp, "[inv] %ld %s\n", (long)n,
						cs ? get_tok_str(cs->v, NULL) : "?");
	}
	if (g)
		{ MCC_TRACE("br\n"); ast_arena_free(g); }
}

static FILE *ast_slc_fp;
static int ast_slc_on;
static int ast_slc_id;
static const char *ast_slc_fn;
static long ast_slc_nslice[2];
static long ast_slc_sbytes[2];
static long ast_slc_snodes[2];
static int ast_slc_ovf;

static int ast_slc_tried;

static void ast_slc_open(void) { MCC_TRACE("enter\n");
	const char *p;
	if (ast_slc_on || ast_slc_tried)
		{ MCC_TRACE("br\n"); return; }
	ast_slc_tried = 1;
	p = getenv("MCC_SLICE_CENSUS");
	if (!p || !p[0])
		{ MCC_TRACE("br\n"); return; }
	ast_slc_fp = (p[0] == '-' && !p[1]) ? stderr : fopen(p, "a");
	if (!ast_slc_fp)
		{ MCC_TRACE("br\n"); return; }
	setvbuf(ast_slc_fp, NULL, _IOLBF, BUFSIZ);
	ast_slc_on = 1;
}

static int ast_slc_callee_sym(const AstArena *a, AstLocal inv, void **out) { MCC_TRACE("enter\n");
	AstLocal c = ast_first_child(a, inv);
	void *cs;
	*out = NULL;
	if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	cs = (void *)(uintptr_t)ast_sym(a, c);
	if (!cs || (((Sym *)cs)->type.t & VT_BTYPE) != VT_FUNC)
		{ MCC_TRACE("br\n"); return 0; }
	*out = cs;
	return 1;
}

static int ast_slc_graftable(const AstArena *a, AstLocal inv) { MCC_TRACE("enter\n");
	void *cs;
	struct AstInlineFn *e;
	if (!ast_slc_callee_sym(a, inv, &cs))
		{ MCC_TRACE("br\n"); return 0; }
	e = ast_inline_find(cs);
	return e && e->graftable ? 1 : 0;
}

static int ast_slc_invclass(const AstArena *a, AstLocal inv) { MCC_TRACE("enter\n");
	void *cs;
	struct AstInlineFn *e;
	if (!ast_slc_callee_sym(a, inv, &cs))
		{ MCC_TRACE("br\n"); return 0; }
	e = ast_inline_find(cs);
	if (!e)
		{ MCC_TRACE("br\n"); return 1; }
	return e->graftable ? 3 : 2;
}

static int ast_slc_hascall(const AstArena *a, AstLocal n, int transparent) { MCC_TRACE("enter\n");
	int r = 0;
	AstLocal c;
	if (n < ast_slc_memo_cap && ast_slc_memo[n] >= 0)
		{ MCC_TRACE("br\n"); return ast_slc_memo[n]; }
	if (ast_kind(a, n) == AST_Invoke &&
			!(transparent && ast_slc_graftable(a, n)))
		{ MCC_TRACE("br\n"); r = 1; }
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_slc_hascall(a, c, transparent))
			{ MCC_TRACE("br\n"); r = 1; } }
	if (n < ast_slc_memo_cap)
		{ MCC_TRACE("br\n"); ast_slc_memo[n] = (signed char)r; }
	return r;
}

static void ast_slc_acc(const AstArena *a, AstLocal n, long *nodes,
												unsigned *kinds, int *loops) { MCC_TRACE("enter\n");
	AstLocal c;
	int t = ast_type_t(a, n);
	*nodes += 1;
	if (!ast_bad_type(t) && is_float(t))
		{ MCC_TRACE("br\n"); *kinds |= AST_SLC_K_FLOAT; }
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_BasicBlock:
		*kinds |= AST_SLC_K_BB;
		break;
	case AST_Store:
		*kinds |= AST_SLC_K_STORE;
		break;
	case AST_StoreVal:
		*kinds |= AST_SLC_K_STOREVAL;
		break;
	case AST_Load:
		*kinds |= AST_SLC_K_LOAD;
		break;
	case AST_Binary:
		*kinds |= AST_SLC_K_BINARY;
		break;
	case AST_Unary:
		*kinds |= AST_SLC_K_UNARY;
		if (ast_op_is_asm(ast_op(a, n)))
			{ MCC_TRACE("br\n"); *kinds |= AST_SLC_K_ASM; }
		break;
	case AST_Convert:
		*kinds |= AST_SLC_K_CONVERT;
		break;
	case AST_Literal:
		*kinds |= AST_SLC_K_LITERAL;
		break;
	case AST_Ref:
		*kinds |= AST_SLC_K_REF;
		break;
	case AST_Jump:
		*kinds |= AST_SLC_K_JUMP;
		break;
	case AST_Return:
		*kinds |= AST_SLC_K_RETURN;
		break;
	case AST_Poison:
		*kinds |= AST_SLC_K_POISON;
		break;
	case AST_Invoke:
		*kinds |= AST_SLC_K_INVOKE;
		break;
	case AST_If:
		switch (ast_op(a, n)) { MCC_TRACE("br\n");
		case 2:
			*kinds |= AST_SLC_K_WHILE;
			*loops += 1;
			break;
		case 3:
		case 8:
			*kinds |= AST_SLC_K_FOR;
			*loops += 1;
			break;
		case 4:
			*kinds |= AST_SLC_K_DO;
			*loops += 1;
			break;
		case 6:
			*kinds |= AST_SLC_K_SWITCH;
			break;
		case 7:
			*kinds |= AST_SLC_K_TERNARY;
			break;
		default:
			*kinds |= AST_SLC_K_IF;
			break;
		}
		break;
	default:
		break;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_slc_acc(a, c, nodes, kinds, loops); }
}

static int ast_slc_depth(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int d = 0;
	AstLocal p;
	for (p = ast_parent(a, n); p != AST_NONE; p = ast_parent(a, p))
		{ MCC_TRACE("br\n"); if (ast_licm_is_loop(a, p))
			{ MCC_TRACE("br\n"); d++; } }
	return d;
}

static long ast_slc_bytes_of(const AstArena *a, AstLocal s) { MCC_TRACE("enter\n");
	if (a != ast_sattr_arena || s >= ast_sattr_cap)
		{ MCC_TRACE("br\n"); return -1; }
	return ast_sattr[s];
}

static void ast_slc_flush(AstArena *a, const AstLocal *run, int nrun, int t) { MCC_TRACE("enter\n");
	long nodes = 0, bytes = 0;
	unsigned kinds = 0;
	int loops = 0, i, depth, unattr = 0;
	if (nrun <= 0)
		{ MCC_TRACE("br\n"); return; }
	for (i = 0; i < nrun; i++) { MCC_TRACE("br\n");
		long b = ast_slc_bytes_of(a, run[i]);
		ast_slc_acc(a, run[i], &nodes, &kinds, &loops);
		if (b < 0)
			{ MCC_TRACE("br\n"); unattr = 1; }
		else
			{ MCC_TRACE("br\n"); bytes += b; }
	}
	depth = ast_slc_depth(a, run[0]);
	ast_slc_nslice[t]++;
	ast_slc_snodes[t] += nodes;
	if (!unattr)
		{ MCC_TRACE("br\n"); ast_slc_sbytes[t] += bytes; }
	fprintf(ast_slc_fp,
					"[slice] fn=%s t=%d id=%d stmts=%d nodes=%ld bytes=%ld depth=%d "
					"loops=%d kinds=%08x\n",
					ast_slc_fn, t, ast_slc_id++, nrun, nodes, unattr ? -1L : bytes,
					depth, loops, kinds);
}

static void ast_slc_walk(AstArena *a, AstLocal bb, int t);

static void ast_slc_descend(AstArena *a, AstLocal n, int t) { MCC_TRACE("enter\n");
	AstLocal c;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		if (ast_kind(a, c) == AST_BasicBlock)
			{ MCC_TRACE("br\n"); ast_slc_walk(a, c, t); }
		else
			{ MCC_TRACE("br\n"); ast_slc_descend(a, c, t); }
	}
}

static void ast_slc_walk(AstArena *a, AstLocal bb, int t) { MCC_TRACE("enter\n");
	AstLocal run[AST_SLC_RUNMAX];
	int nrun = 0;
	AstLocal s;
	for (s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		if (!ast_slc_hascall(a, s, t)) { MCC_TRACE("br\n");
			if (nrun == AST_SLC_RUNMAX) { MCC_TRACE("br\n");
				ast_slc_ovf = 1;
				ast_slc_flush(a, run, nrun, t);
				nrun = 0;
			}
			run[nrun++] = s;
			continue;
		}
		ast_slc_flush(a, run, nrun, t);
		nrun = 0;
		ast_slc_descend(a, s, t);
	}
	ast_slc_flush(a, run, nrun, t);
}

static void ast_slc_dump(AstArena *a, const char *fname, long body_bytes,
												 long replay_bytes, int faithful) { MCC_TRACE("enter\n");
	AstLocal nn, n;
	long nodes = 0, attr = 0;
	unsigned kinds = 0;
	int loops = 0, t;
	int inv[4] = {0, 0, 0, 0};
	int stmts = 0;
	ast_slc_open();
	if (!ast_slc_on || !a)
		{ MCC_TRACE("br\n"); return; }
	nn = ast_count(a);
	if (!nn)
		{ MCC_TRACE("br\n"); return; }
	if (nn > ast_slc_memo_cap) { MCC_TRACE("br\n");
		ast_slc_memo = mcc_realloc(ast_slc_memo, (size_t)nn);
		ast_slc_memo_cap = nn;
	}
	ast_slc_fn = fname ? fname : "?";
	ast_slc_ovf = 0;
	for (n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_Invoke)
			{ MCC_TRACE("br\n"); inv[ast_slc_invclass(a, n)]++; } }
	ast_slc_acc(a, ast_root(a), &nodes, &kinds, &loops);
	for (n = ast_first_child(a, ast_root(a)); n != AST_NONE; n = ast_next_sib(a, n)) { MCC_TRACE("br\n");
		long b = ast_slc_bytes_of(a, n);
		stmts++;
		if (b > 0)
			{ MCC_TRACE("br\n"); attr += b; }
	}
	for (t = 0; t < 2; t++) { MCC_TRACE("br\n");
		ast_slc_nslice[t] = 0;
		ast_slc_sbytes[t] = 0;
		ast_slc_snodes[t] = 0;
		memset(ast_slc_memo, -1, (size_t)nn);
		ast_slc_walk(a, ast_root(a), t);
	}
	fprintf(ast_slc_fp,
					"[slice-fn] fn=%s nodes=%ld bytes=%ld rbytes=%ld attr=%ld stmts=%d "
					"faithful=%d "
					"loops=%d inv_ind=%d inv_ext=%d inv_ret=%d inv_graft=%d "
					"ns0=%ld nb0=%ld nn0=%ld ns1=%ld nb1=%ld nn1=%ld ovf=%d kinds=%08x\n",
					ast_slc_fn, nodes, body_bytes, replay_bytes, attr, stmts, faithful,
					loops, inv[0],
					inv[1], inv[2], inv[3], ast_slc_nslice[0], ast_slc_sbytes[0],
					ast_slc_snodes[0], ast_slc_nslice[1], ast_slc_sbytes[1],
					ast_slc_snodes[1], ast_slc_ovf, kinds);
	ast_sattr_arena = NULL;
}

static AstLocal ast_li_prev_sib(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal p = ast_parent(a, n);
	if (p == AST_NONE)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	AstLocal prev = AST_NONE;
	for (AstLocal c = ast_first_child(a, p); c != AST_NONE; c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
		if (c == n)
			{ MCC_TRACE("br\n"); return prev; }
		prev = c;
	}
	return AST_NONE;
}

static void ast_li_list_remove(AstArena *a, AstLocal parent, AstLocal node) { MCC_TRACE("enter\n");
	a->epoch++;
	AstLocal prev = AST_NONE;
	for (AstLocal c = a->first_child[parent]; c != AST_NONE; c = a->next_sib[c]) { MCC_TRACE("br\n");
		if (c == node) { MCC_TRACE("br\n");
			if (prev == AST_NONE)
				{ MCC_TRACE("br\n"); a->first_child[parent] = a->next_sib[c]; }
			else
				{ MCC_TRACE("br\n"); a->next_sib[prev] = a->next_sib[c]; }
			if (a->last_child[parent] == c)
				{ MCC_TRACE("br\n"); a->last_child[parent] = prev; }
			a->next_sib[c] = AST_NONE;
			a->nchild[parent]--;
			return;
		}
		prev = c;
	}
}

static void ast_li_list_insert_before(AstArena *a, AstLocal parent, AstLocal ref,
																			AstLocal node) { MCC_TRACE("enter\n");
	a->epoch++;
	a->parent[node] = parent;
	if (a->first_child[parent] == ref) { MCC_TRACE("br\n");
		a->next_sib[node] = ref;
		a->first_child[parent] = node;
		a->nchild[parent]++;
		return;
	}
	for (AstLocal c = a->first_child[parent]; c != AST_NONE; c = a->next_sib[c])
		{ MCC_TRACE("br\n"); if (a->next_sib[c] == ref) { MCC_TRACE("br\n");
			a->next_sib[node] = ref;
			a->next_sib[c] = node;
			a->nchild[parent]++;
			return;
		} }
	a->next_sib[node] = AST_NONE;
	if (a->first_child[parent] == AST_NONE)
		{ MCC_TRACE("br\n"); a->first_child[parent] = node; }
	else
		{ MCC_TRACE("br\n"); a->next_sib[a->last_child[parent]] = node; }
	a->last_child[parent] = node;
	a->nchild[parent]++;
}

static int ast_interchange_is_init(AstArena *a, AstLocal store, int iv_off) { MCC_TRACE("enter\n");
	if (store == AST_NONE || ast_kind(a, store) != AST_Store || ast_nchild(a, store) < 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ref_is_local_off(a, ast_child(a, store, 0), iv_off))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal rhs = ast_dep_strip(a, ast_child(a, store, 1));
	return rhs != AST_NONE && ast_kind(a, rhs) == AST_Literal;
}

static int ast_interchange_body_ok(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 1; }
	switch (ast_kind(a, n)) { MCC_TRACE("br\n");
	case AST_BasicBlock:
	case AST_Load:
	case AST_Ref:
	case AST_Literal:
	case AST_Unary:
	case AST_Binary:
	case AST_Convert:
		break;
	case AST_Store:
		if (ast_nchild(a, n) < 2 || ast_kind(a, ast_child(a, n, 0)) != AST_Load)
			{ MCC_TRACE("br\n"); return 0; }
		break;
	default:
		return 0;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (!ast_interchange_body_ok(a, c))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

static int ast_interchange_beneficial(AstArena *a, AstLocal outer, AstLocal inner) { MCC_TRACE("enter\n");
	int ivo, ivi, t;
	int64_t st;
	if (!ast_loop_iv(a, outer, &ivo, &t, &st) ||
			!ast_loop_iv(a, inner, &ivi, &t, &st))
		{ MCC_TRACE("br\n"); return 0; }
	int ivs[2] = {ivo, ivi};
	AstLocal body, incr;
	ast_loop_parts(a, inner, ast_op(a, inner), &body, &incr);
	int nref, overflow;
	AstDepRef *refs = ast_dep_collect(a, body, ivs, 2, &nref, &overflow);
	int64_t inner_w = 0, outer_w = 0;
	int any = 0;
	for (int i = 0; i < nref; i++) { MCC_TRACE("br\n");
		if (!refs[i].ok)
			{ MCC_TRACE("br\n"); continue; }
		for (int d = 0; d < refs[i].ndim; d++) { MCC_TRACE("br\n");
			int64_t co = refs[i].sub[d].coeff[0], ci = refs[i].sub[d].coeff[1];
			int64_t w = refs[i].ndim - d;
			outer_w += w * (co < 0 ? -co : co);
			inner_w += w * (ci < 0 ? -ci : ci);
		}
		any = 1;
	}
	mcc_free(refs);
	return any && outer_w < inner_w;
}

static int ast_interchange_apply(AstArena *a, AstLocal outer, AstLocal inner) { MCC_TRACE("enter\n");
	if (ast_op(a, outer) != 3 || ast_op(a, inner) != 3)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_nchild(a, outer) != 3 || ast_nchild(a, inner) != 3)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal pbb = ast_parent(a, outer);
	if (pbb == AST_NONE || ast_kind(a, pbb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal body_o = ast_parent(a, inner);
	if (body_o == AST_NONE || ast_kind(a, body_o) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal cond_o = ast_child(a, outer, 0), incr_o = ast_child(a, outer, 1),
					 bodyO = ast_child(a, outer, 2);
	AstLocal cond_i = ast_child(a, inner, 0), incr_i = ast_child(a, inner, 1),
					 bodyI = ast_child(a, inner, 2);
	if (bodyO != body_o)
		{ MCC_TRACE("br\n"); return 0; }
	int ivo, ivi, t;
	int64_t st;
	if (!ast_loop_iv(a, outer, &ivo, &t, &st) ||
			!ast_loop_iv(a, inner, &ivi, &t, &st))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal so = ast_li_prev_sib(a, outer);
	AstLocal si = ast_li_prev_sib(a, inner);
	if (!ast_interchange_is_init(a, so, ivo) || !ast_interchange_is_init(a, si, ivi))
		{ MCC_TRACE("br\n"); return 0; }
	ast_clear_children(a, outer);
	ast_add_child(a, outer, cond_i);
	ast_add_child(a, outer, incr_i);
	ast_add_child(a, outer, bodyO);
	ast_clear_children(a, inner);
	ast_add_child(a, inner, cond_o);
	ast_add_child(a, inner, incr_o);
	ast_add_child(a, inner, bodyI);
	ast_li_list_remove(a, pbb, so);
	ast_li_list_remove(a, body_o, si);
	ast_li_list_insert_before(a, body_o, inner, so);
	ast_li_list_insert_before(a, pbb, outer, si);
	MCC_TRACE("interchange outer#%u inner#%u iv@%d<->@%d\n", (unsigned)outer,
						(unsigned)inner, ivo, ivi);
	return 1;
}

#define AST_UNROLL_CAP 8

static int ast_body_has_loop(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_licm_is_loop(a, n))
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_body_has_loop(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_unroll_apply(AstArena *a, AstLoopInfo *li) { MCC_TRACE("enter\n");
	if ((li->op != 3 && li->op != 2 && li->op != 4) || li->unanalyzable ||
			!li->has_iv)
		{ MCC_TRACE("br\n"); return 0; }
	int64_t stride = li->iv_stride;
	if (stride == 0 || stride < -AST_UNROLL_CAP || stride > AST_UNROLL_CAP)
		{ MCC_TRACE("br\n"); return 0; }
	int64_t astride = stride < 0 ? -stride : stride;
	AstLocal loop = li->header;
	AstLocal parent = ast_parent(a, loop);
	if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal cond = li->cond;
	if (cond == AST_NONE || ast_kind(a, cond) != AST_Binary ||
			ast_nchild(a, cond) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	int cop = ast_op(a, cond);
	int ascending = (cop == TOK_LT || cop == TOK_LE);
	int descending = (cop == TOK_GT || cop == TOK_GE);
	int neq = (cop == TOK_NE);
	if (!ascending && !descending && !neq)
		{ MCC_TRACE("br\n"); return 0; }
	if ((ascending && stride < 1) || (descending && stride > -1))
		{ MCC_TRACE("br\n"); return 0; }
	if (!neq && li->bound_kind != AST_LOOP_BOUND_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ref_is_local_off(a, ast_child(a, cond, 0), li->iv_off))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal blit = ast_dep_strip(a, ast_child(a, cond, 1));
	if (blit == AST_NONE || ast_kind(a, blit) != AST_Literal)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal so = ast_li_prev_sib(a, loop);
	if (!ast_interchange_is_init(a, so, li->iv_off))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal ilit = ast_dep_strip(a, ast_child(a, so, 1));
	if (ilit == AST_NONE || ast_kind(a, ilit) != AST_Literal)
		{ MCC_TRACE("br\n"); return 0; }
	int64_t bnd = (int64_t)ast_ival(a, blit);
	int64_t ini = (int64_t)ast_ival(a, ilit);
	int64_t trip;
	if (neq) { MCC_TRACE("br\n");
		int64_t diff = bnd - ini;
		if (diff == 0 || diff % stride != 0)
			{ MCC_TRACE("br\n"); return 0; }
		trip = diff / stride;
	} else { MCC_TRACE("br\n");
		int incl = (cop == TOK_LE || cop == TOK_GE);
		int64_t base = ascending ? (bnd - ini) : (ini - bnd);
		int64_t span = base + (incl ? 1 : 0);
		if (span < 1 || span > AST_UNROLL_CAP * astride)
			{ MCC_TRACE("br\n"); return 0; }
		trip = (span + astride - 1) / astride;
	}
	if (trip < 1 || trip > AST_UNROLL_CAP)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal incr = li->incr;
	AstLocal body = li->body;
	if (body == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_body_has_loop(a, body))
		{ MCC_TRACE("br\n"); return 0; }
	for (int64_t k = 0; k < trip; k++) { MCC_TRACE("br\n");
		ast_li_list_insert_before(a, parent, loop, ast_dup_sub(a, body));
		if (incr != AST_NONE)
			{ MCC_TRACE("br\n"); ast_li_list_insert_before(a, parent, loop, ast_dup_sub(a, incr)); }
	}
	ast_li_list_remove(a, parent, loop);
	return 1;
}

static AstLocal ast_li_off(AstArena *a, AstLocal baseref, AstLocal tmpl, int64_t ini) { MCC_TRACE("enter\n");
	AstLocal d, lit, plus;
	d = ast_dup_sub(a, baseref);
	if (ini == 0)
		{ MCC_TRACE("br\n"); return d; }
	lit = ast_node(a, AST_Literal);
	ast_set_type(a, lit, VT_LLONG, 0);
	ast_set_ival(a, lit, value64((uint64_t)ini, VT_LLONG));
	ast_set_op(a, lit, VT_CONST);
	plus = ast_node(a, AST_Binary);
	ast_set_op(a, plus, '+');
	ast_set_type(a, plus, ast_type_t(a, tmpl), ast_type_ref(a, tmpl));
	ast_add_child(a, plus, d);
	ast_add_child(a, plus, lit);
	return plus;
}
static int ast_loopidiom_apply(AstArena *a, AstLoopInfo *li) { MCC_TRACE("enter\n");
	AstLocal loop, parent, cond, blit, so, ilit, body, store, addr, val, bin, o0, o1, base;
	int64_t ini, nbytes;
	int al, elemsize, bytev = 0, is_cpy = 0;
	CType ct;
	Sym *ms;
	AstLocal call, ref, dst, zero, len, srcbase = AST_NONE;
	if (li->op != 3 || li->unanalyzable || !li->has_iv || li->iv_stride != 1)
		{ MCC_TRACE("br\n"); return 0; }
	loop = li->header;
	parent = ast_parent(a, loop);
	if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	cond = li->cond;
	if (cond == AST_NONE || ast_kind(a, cond) != AST_Binary ||
			ast_op(a, cond) != TOK_LT || ast_nchild(a, cond) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_ref_is_local_off(a, ast_child(a, cond, 0), li->iv_off))
		{ MCC_TRACE("br\n"); return 0; }
	blit = ast_dep_strip(a, ast_child(a, cond, 1));
	if (blit == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	so = ast_li_prev_sib(a, loop);
	if (!ast_interchange_is_init(a, so, li->iv_off))
		{ MCC_TRACE("br\n"); return 0; }
	ilit = ast_dep_strip(a, ast_child(a, so, 1));
	if (ilit == AST_NONE || ast_kind(a, ilit) != AST_Literal)
		{ MCC_TRACE("br\n"); return 0; }
	ini = (int64_t)ast_ival(a, ilit);
	body = ast_child(a, loop, 2);
	if (body == AST_NONE || ast_kind(a, body) != AST_BasicBlock || ast_nchild(a, body) != 1)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_body_has_loop(a, body))
		{ MCC_TRACE("br\n"); return 0; }
	store = ast_first_child(a, body);
	if (store == AST_NONE || ast_kind(a, store) != AST_Store || ast_nchild(a, store) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	addr = ast_child(a, store, 0);
	val = ast_child(a, store, 1);
	if (val == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, val) == AST_Literal) { MCC_TRACE("br\n");
		if ((ast_stype_t(a, val) & VT_BTYPE) == VT_FLOAT ||
				(ast_stype_t(a, val) & VT_BTYPE) == VT_DOUBLE ||
				(ast_stype_t(a, val) & VT_BTYPE) == VT_LDOUBLE)
			{ MCC_TRACE("br\n"); return 0; }
	} else if (ast_kind(a, val) == AST_Load && ast_nchild(a, val) == 1) { MCC_TRACE("br\n");
		AstLocal vbin = ast_first_child(a, val), vo0, vo1;
		if (ast_kind(a, vbin) != AST_Binary || ast_op(a, vbin) != '+' || ast_nchild(a, vbin) != 2)
			{ MCC_TRACE("br\n"); return 0; }
		vo0 = ast_child(a, vbin, 0);
		vo1 = ast_child(a, vbin, 1);
		if (ast_ref_is_local_off(a, vo1, li->iv_off)) srcbase = vo0;
		else if (ast_ref_is_local_off(a, vo0, li->iv_off)) srcbase = vo1;
		else { MCC_TRACE("br\n"); return 0; }
		if (ast_kind(a, srcbase) != AST_Ref)
			{ MCC_TRACE("br\n"); return 0; }
		is_cpy = 1;
	} else {
		MCC_TRACE("br\n");
		return 0;
	}
	if (addr == AST_NONE || ast_kind(a, addr) != AST_Load || ast_nchild(a, addr) != 1)
		{ MCC_TRACE("br\n"); return 0; }
	bin = ast_first_child(a, addr);
	if (bin == AST_NONE || ast_kind(a, bin) != AST_Binary || ast_op(a, bin) != '+' || ast_nchild(a, bin) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	o0 = ast_child(a, bin, 0);
	o1 = ast_child(a, bin, 1);
	if (ast_ref_is_local_off(a, o1, li->iv_off)) base = o0;
	else if (ast_ref_is_local_off(a, o0, li->iv_off)) base = o1;
	else { MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, base) != AST_Ref)
		{ MCC_TRACE("br\n"); return 0; }
	ct.t = ast_type_t(a, base);
	ct.bp = ast_type_bp(a, base);
	ct.bs = ast_type_bs(a, base);
	ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, base);
	if ((ct.t & VT_BTYPE) != VT_PTR || !ct.ref)
		{ MCC_TRACE("br\n"); return 0; }
	if (is_cpy) { MCC_TRACE("br\n");
		if (!(ast_type_t(a, base) & VT_ARRAY) || !(ast_type_t(a, srcbase) & VT_ARRAY))
			{ MCC_TRACE("br\n"); return 0; }
		if (ast_sym(a, base) == ast_sym(a, srcbase) &&
				ast_ival(a, base) == ast_ival(a, srcbase) &&
				ast_op(a, base) == ast_op(a, srcbase))
			{ MCC_TRACE("br\n"); return 0; }
	}
	if (ast_promo_size_unknown(&ct.ref->type))
		{ MCC_TRACE("br\n"); return 0; }
	elemsize = type_size(&ct.ref->type, &al);
	if (elemsize < 1)
		{ MCC_TRACE("br\n"); return 0; }
	if (!is_cpy) { MCC_TRACE("br\n");
		uint64_t uv = (uint64_t)ast_ival(a, val), rep = 0, mask;
		int k;
		bytev = (int)(uv & 0xFF);
		for (k = 0; k < elemsize && k < 8; k++)
			rep |= (uint64_t)(unsigned char)bytev << (k * 8);
		mask = elemsize >= 8 ? ~(uint64_t)0 : (((uint64_t)1 << (elemsize * 8)) - 1);
		if ((uv & mask) != (rep & mask))
			{ MCC_TRACE("br\n"); return 0; }
	}
	if (ast_kind(a, blit) == AST_Literal) { MCC_TRACE("br\n");
		nbytes = ((int64_t)ast_ival(a, blit) - ini) * (int64_t)elemsize;
		if (nbytes < 1 || nbytes > (int64_t)0x40000000)
			{ MCC_TRACE("br\n"); return 0; }
	} else if (!(ast_stype_t(a, blit) & VT_UNSIGNED)) {
		MCC_TRACE("br\n");
		return 0;
	} else if (ini != 0) {
		MCC_TRACE("br\n");
		return 0;
	}
	ms = external_helper_sym(is_cpy ? TOK_memcpy : TOK_memset);
	if (!ms)
		{ MCC_TRACE("br\n"); return 0; }
	call = ast_node(a, AST_Invoke);
	ref = ast_node(a, AST_Ref);
	ast_set_type(a, ref, func_old_type.t, (uint64_t)(uintptr_t)func_old_type.ref);
	ast_set_sym(a, ref, (uint64_t)(uintptr_t)ms);
	ast_set_op(a, ref, VT_CONST | VT_SYM);
	ast_add_child(a, call, ref);
	dst = ast_li_off(a, base, bin, ini);
	ast_add_child(a, call, dst);
	if (is_cpy) { MCC_TRACE("br\n");
		ast_add_child(a, call, ast_li_off(a, srcbase, bin, ini));
	} else { MCC_TRACE("br\n");
		zero = ast_node(a, AST_Literal);
		ast_set_type(a, zero, int_type.t, 0);
		ast_set_ival(a, zero, value64((uint64_t)bytev, int_type.t));
		ast_set_op(a, zero, VT_CONST);
		ast_add_child(a, call, zero);
	}
	if (ast_kind(a, blit) == AST_Literal) { MCC_TRACE("br\n");
		len = ast_node(a, AST_Literal);
		ast_set_type(a, len, VT_LLONG, 0);
		ast_set_ival(a, len, value64((uint64_t)nbytes, VT_LLONG));
		ast_set_op(a, len, VT_CONST);
	} else { MCC_TRACE("br\n");
		AstLocal cvt, esz;
		cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, VT_LLONG | VT_UNSIGNED, 0);
		ast_add_child(a, cvt, ast_dup_sub(a, blit));
		esz = ast_node(a, AST_Literal);
		ast_set_type(a, esz, VT_LLONG | VT_UNSIGNED, 0);
		ast_set_ival(a, esz, value64((uint64_t)elemsize, VT_LLONG | VT_UNSIGNED));
		ast_set_op(a, esz, VT_CONST);
		len = ast_node(a, AST_Binary);
		ast_set_op(a, len, '*');
		ast_set_type(a, len, VT_LLONG | VT_UNSIGNED, 0);
		ast_add_child(a, len, cvt);
		ast_add_child(a, len, esz);
	}
	ast_add_child(a, call, len);
	ast_set_type(a, call, int_type.t, 0);
	ast_li_list_insert_before(a, parent, loop, call);
	ast_li_list_remove(a, parent, loop);
	return 1;
}
static int ast_loopidiom_run(AstArena *a) { MCC_TRACE("enter\n");
	int total = 0, guard;
	if (!ast_loopidiom_env)
		{ MCC_TRACE("br\n"); return 0; }
	for (guard = 0; guard < AST_LOOPNEST_CAP; guard++) { MCC_TRACE("br\n");
		int applied = 0, i;
		ast_loopnest_sync(a);
		for (i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
			if (ast_loopidiom_apply(a, &ast_loopnest[i])) { MCC_TRACE("br\n");
				total++; applied = 1; break;
			}
		}
		if (!applied) break;
	}
	return total;
}

static int ast_unroll_run(AstArena *a) { MCC_TRACE("enter\n");
	if (!ast_unroll_env)
		{ MCC_TRACE("br\n"); return 0; }
	int total = 0;
	for (int guard = 0; guard < AST_LOOPNEST_CAP; guard++) { MCC_TRACE("br\n");
		ast_loopnest_sync(a);
		int applied = 0;
		for (int i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
			if (ast_unroll_apply(a, &ast_loopnest[i])) { MCC_TRACE("br\n");
				total++;
				applied = 1;
				break;
			}
		}
		if (!applied)
			{ MCC_TRACE("br\n"); break; }
	}
	return total;
}

static int ast_interchange_run(AstArena *a) { MCC_TRACE("enter\n");
	if (!ast_interchange_env)
		{ MCC_TRACE("br\n"); return 0; }
	int total = 0;
	for (int guard = 0; guard < AST_LOOPNEST_CAP; guard++) { MCC_TRACE("br\n");
		ast_loopnest_sync(a);
		int applied = 0;
		for (int i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
			AstLocal inner = ast_loopnest[i].header;
			AstLocal outer = ast_loopnest[i].parent;
			if (outer == AST_NONE)
				{ MCC_TRACE("br\n"); continue; }
			if (ast_op(a, outer) != 3 || ast_op(a, inner) != 3)
				{ MCC_TRACE("br\n"); continue; }
			if (!ast_loop_interchange_legal(a, outer, inner))
				{ MCC_TRACE("br\n"); continue; }
			{
				AstLocal ibody, iincr;
				ast_loop_parts(a, inner, ast_op(a, inner), &ibody, &iincr);
				if (!ast_interchange_body_ok(a, ibody))
					{ MCC_TRACE("br\n"); continue; }
			}
			if (!ast_interchange_beneficial(a, outer, inner))
				{ MCC_TRACE("br\n"); continue; }
			if (ast_interchange_apply(a, outer, inner)) { MCC_TRACE("br\n");
				total++;
				applied = 1;
				break;
			}
		}
		if (!applied)
			{ MCC_TRACE("br\n"); break; }
	}
	return total;
}

static int ast_li_refs_off(AstArena *a, AstLocal n, int off) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Ref && ast_ref_is_local_off(a, n, off))
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_li_refs_off(a, c, off))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static void ast_li_append_children(AstArena *a, AstLocal dst, AstLocal src) { MCC_TRACE("enter\n");
	a->epoch++;
	AstLocal c = a->first_child[src];
	while (c != AST_NONE) { MCC_TRACE("br\n");
		AstLocal nx = a->next_sib[c];
		a->parent[c] = dst;
		a->next_sib[c] = AST_NONE;
		if (a->first_child[dst] == AST_NONE)
			{ MCC_TRACE("br\n"); a->first_child[dst] = c; }
		else
			{ MCC_TRACE("br\n"); a->next_sib[a->last_child[dst]] = c; }
		a->last_child[dst] = c;
		a->nchild[dst]++;
		c = nx;
	}
	a->first_child[src] = AST_NONE;
	a->last_child[src] = AST_NONE;
	a->nchild[src] = 0;
}

static int ast_fusion_apply(AstArena *a, AstLocal l1, AstLocal l2) { MCC_TRACE("enter\n");
	if (ast_op(a, l1) != 3 || ast_op(a, l2) != 3)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_nchild(a, l1) != 3 || ast_nchild(a, l2) != 3)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal pbb = ast_parent(a, l1);
	if (pbb == AST_NONE || ast_kind(a, pbb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_parent(a, l2) != pbb)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal incr1 = ast_child(a, l1, 1), body1 = ast_child(a, l1, 2);
	AstLocal incr2 = ast_child(a, l2, 1), body2 = ast_child(a, l2, 2);
	if (ast_kind(a, incr1) != AST_BasicBlock || ast_kind(a, body1) != AST_BasicBlock ||
			ast_kind(a, incr2) != AST_BasicBlock || ast_kind(a, body2) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	int o1, o2, t;
	int64_t s;
	if (!ast_loop_iv(a, l1, &o1, &t, &s) || !ast_loop_iv(a, l2, &o2, &t, &s))
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_interchange_body_ok(a, body1) || !ast_interchange_body_ok(a, body2))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal init2 = ast_li_prev_sib(a, l2);
	if (!ast_interchange_is_init(a, init2, o2))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_li_prev_sib(a, init2) != l1)
		{ MCC_TRACE("br\n"); return 0; }
	if (o1 != o2 &&
			(ast_li_refs_off(a, body1, o2) || ast_li_refs_off(a, incr1, o2)))
		{ MCC_TRACE("br\n"); return 0; }
	ast_li_append_children(a, body1, body2);
	if (o1 != o2) { MCC_TRACE("br\n");
		ast_li_append_children(a, incr1, incr2);
		ast_li_list_remove(a, pbb, init2);
		ast_li_list_insert_before(a, pbb, l1, init2);
	} else { MCC_TRACE("br\n");
		ast_li_list_remove(a, pbb, init2);
	}
	ast_li_list_remove(a, pbb, l2);
	MCC_TRACE("fusion loop#%u += loop#%u iv@%d/@%d\n", (unsigned)l1, (unsigned)l2,
						o1, o2);
	return 1;
}

static int ast_fusion_run(AstArena *a) { MCC_TRACE("enter\n");
	if (!ast_fusion_env)
		{ MCC_TRACE("br\n"); return 0; }
	int total = 0;
	for (int guard = 0; guard < AST_LOOPNEST_CAP; guard++) { MCC_TRACE("br\n");
		ast_loopnest_sync(a);
		int applied = 0;
		for (int i = 0; i < ast_loopnest_n && !applied; i++)
			{ MCC_TRACE("br\n"); for (int j = 0; j < ast_loopnest_n && !applied; j++) { MCC_TRACE("br\n");
				if (i == j)
					{ MCC_TRACE("br\n"); continue; }
				AstLocal l1 = ast_loopnest[i].header, l2 = ast_loopnest[j].header;
				if (ast_op(a, l1) != 3 || ast_op(a, l2) != 3)
					{ MCC_TRACE("br\n"); continue; }
				if (!ast_dep_adjacent(a, l1, l2))
					{ MCC_TRACE("br\n"); continue; }
				if (!ast_loop_fusion_legal(a, l1, l2))
					{ MCC_TRACE("br\n"); continue; }
				if (ast_fusion_apply(a, l1, l2)) { MCC_TRACE("br\n");
					total++;
					applied = 1;
				}
			} }
		if (!applied)
			{ MCC_TRACE("br\n"); break; }
	}
	return total;
}

static AstLocal ast_tile_local_ref(AstArena *a, int off, int op, int tt,
																	 uint64_t tr) { MCC_TRACE("enter\n");
	AstLocal r = ast_node(a, AST_Ref);
	ast_set_op(a, r, op);
	ast_set_ival(a, r, (uint64_t)off);
	ast_set_type(a, r, tt, tr);
	return r;
}

static int ast_tile_apply(AstArena *a, AstLocal outer, AstLocal inner) { MCC_TRACE("enter\n");
	if (ast_op(a, outer) != 3 || ast_op(a, inner) != 3)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_nchild(a, outer) != 3 || ast_nchild(a, inner) != 3)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal pbb = ast_parent(a, outer);
	if (pbb == AST_NONE || ast_kind(a, pbb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal obody = ast_parent(a, inner);
	if (obody == AST_NONE || ast_kind(a, obody) != AST_BasicBlock ||
			obody != ast_child(a, outer, 2))
		{ MCC_TRACE("br\n"); return 0; }
	int ivo, ivj, t;
	int64_t sto, stj;
	if (!ast_loop_iv(a, outer, &ivo, &t, &sto) ||
			!ast_loop_iv(a, inner, &ivj, &t, &stj))
		{ MCC_TRACE("br\n"); return 0; }
	if (stj != 1)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal ibody, iincr;
	ast_loop_parts(a, inner, ast_op(a, inner), &ibody, &iincr);
	if (!ast_interchange_body_ok(a, ibody))
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal cond_j = ast_child(a, inner, 0);
	if (cond_j == AST_NONE || ast_kind(a, cond_j) != AST_Binary ||
			ast_op(a, cond_j) != TOK_LT || ast_nchild(a, cond_j) != 2)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal jref = ast_child(a, cond_j, 0), mexpr = ast_child(a, cond_j, 1);
	if (!ast_ref_is_local_off(a, jref, ivj) || ast_kind(a, mexpr) != AST_Literal)
		{ MCC_TRACE("br\n"); return 0; }
	int T = ast_tile_size;
	if ((int64_t)ast_ival(a, mexpr) <= T)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal i_init = ast_li_prev_sib(a, outer);
	AstLocal j_init = ast_li_prev_sib(a, inner);
	if (!ast_interchange_is_init(a, i_init, ivo) ||
			!ast_interchange_is_init(a, j_init, ivj))
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_li_prev_sib(a, j_init) != AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_ltemp_n >= AST_LTEMP_MAX)
		{ MCC_TRACE("br\n"); return 0; }
	int jop = ast_op(a, jref), jtt = ast_type_t(a, jref);
	uint64_t jtr = ast_type_ref(a, jref);
	int ctt = ast_type_t(a, cond_j);
	AstLocal afterOuter = ast_next_sib(a, outer);
	int64_t jstart = (int64_t)ast_ival(a, ast_dep_strip(a, ast_child(a, j_init, 1)));

	int jjoff = (ast_ltemp_cur - 8) & -8;
	ast_ltemp_cur = jjoff;
	ast_ltemp_add(jjoff, 8);

	AstLocal jj_init = ast_node(a, AST_Store);
	ast_add_child(a, jj_init, ast_tile_local_ref(a, jjoff, jop, jtt, jtr));
	ast_add_child(a, jj_init, ast_bf_lit(a, jtt, (uint64_t)jstart));
	AstLocal cond_jj = ast_bf_bin(a, TOK_LT, ctt,
															 ast_tile_local_ref(a, jjoff, jop, jtt, jtr),
															 ast_dup_sub(a, mexpr));
	AstLocal incr_jj = ast_node(a, AST_BasicBlock);
	AstLocal jj_step = ast_node(a, AST_Store);
	ast_add_child(a, jj_step, ast_tile_local_ref(a, jjoff, jop, jtt, jtr));
	ast_add_child(a, jj_step,
								ast_bf_bin(a, '+', jtt,
													 ast_tile_local_ref(a, jjoff, jop, jtt, jtr),
													 ast_bf_lit(a, jtt, (uint64_t)T)));
	ast_add_child(a, incr_jj, jj_step);
	AstLocal tbody = ast_node(a, AST_BasicBlock);
	AstLocal tile = ast_node(a, AST_If);
	ast_set_op(a, tile, 3);

	AstLocal guard = ast_bf_bin(a, TOK_LT, ctt,
														 ast_tile_local_ref(a, ivj, jop, jtt, jtr),
														 ast_bf_bin(a, '+', jtt,
																				ast_tile_local_ref(a, jjoff, jop, jtt, jtr),
																				ast_bf_lit(a, jtt, (uint64_t)T)));
	AstLocal incr_j = ast_child(a, inner, 1);
	ast_li_list_remove(a, inner, cond_j);
	AstLocal newcond = ast_bf_bin(a, TOK_LAND, ctt, cond_j, guard);
	ast_li_list_insert_before(a, inner, incr_j, newcond);

	AstLocal j_start_val = ast_child(a, j_init, 1);
	AstLocal jj_as_j = ast_node(a, AST_Convert);
	ast_set_type(a, jj_as_j, jtt, jtr);
	ast_add_child(a, jj_as_j, ast_tile_local_ref(a, jjoff, jop, jtt, jtr));
	ast_li_list_insert_before(a, j_init, j_start_val, jj_as_j);
	ast_li_list_remove(a, j_init, j_start_val);

	ast_li_list_remove(a, pbb, i_init);
	ast_li_list_remove(a, pbb, outer);
	ast_add_child(a, tbody, i_init);
	ast_add_child(a, tbody, outer);
	ast_add_child(a, tile, cond_jj);
	ast_add_child(a, tile, incr_jj);
	ast_add_child(a, tile, tbody);
	ast_li_list_insert_before(a, pbb, afterOuter, jj_init);
	ast_li_list_insert_before(a, pbb, afterOuter, tile);
	MCC_TRACE("tile outer#%u inner#%u iv@%d strip@%d T=%d\n", (unsigned)outer,
						(unsigned)inner, ivj, jjoff, T);
	return 1;
}

static int ast_tile_run(AstArena *a) { MCC_TRACE("enter\n");
	if (!ast_tile_env)
		{ MCC_TRACE("br\n"); return 0; }
	ast_loopnest_sync(a);
	for (int i = 0; i < ast_loopnest_n; i++) { MCC_TRACE("br\n");
		AstLocal inner = ast_loopnest[i].header;
		AstLocal outer = ast_loopnest[i].parent;
		if (outer == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_op(a, outer) != 3 || ast_op(a, inner) != 3)
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_loop_interchange_legal(a, outer, inner))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_tile_apply(a, outer, inner))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int ast_pre_arm_store(AstArena *a, AstLocal bb, AstLocal *store) { MCC_TRACE("enter\n");
	if (ast_kind(a, bb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	AstLocal last = ast_last_child(a, bb);
	if (last == AST_NONE || ast_kind(a, last) != AST_Store)
		{ MCC_TRACE("br\n"); return 0; }
	*store = last;
	return 1;
}

static AstLocal ast_pre_binary_of(AstArena *a, AstLocal store) { MCC_TRACE("enter\n");
	AstLocal e = ast_child(a, store, 1);
	while (e != AST_NONE && ast_kind(a, e) == AST_Convert && ast_nchild(a, e) == 1)
		{ MCC_TRACE("br\n"); e = ast_first_child(a, e); }
	if (e == AST_NONE || ast_kind(a, e) != AST_Binary)
		{ MCC_TRACE("br\n"); return AST_NONE; }
	return e;
}

static int ast_pre_occurs(AstArena *a, AstLocal n, AstLocal e) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (n != e && ast_ident_same(a, e, n))
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_pre_occurs(a, c, e))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static int ast_pre_run(AstArena *a) { MCC_TRACE("enter\n");
	int did = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_ltemp_n >= AST_LTEMP_MAX)
			{ MCC_TRACE("br\n"); break; }
		if (ast_kind(a, n) != AST_If || ast_op(a, n) != 0)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_nchild(a, n) < 3)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_sccp_has_label(a, n))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal parent = ast_parent(a, n);
		if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal thenbb = ast_child(a, n, 1), elsebb = ast_child(a, n, 2);
		if (ast_kind(a, thenbb) != AST_BasicBlock ||
				ast_kind(a, elsebb) != AST_BasicBlock)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal ts, es, e = AST_NONE;
		if (ast_pre_arm_store(a, thenbb, &ts))
			{ MCC_TRACE("br\n"); e = ast_pre_binary_of(a, ts); }
		if (e == AST_NONE && ast_pre_arm_store(a, elsebb, &es))
			{ MCC_TRACE("br\n"); e = ast_pre_binary_of(a, es); }
		if (e == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_cse_regpure(a, e))
			{ MCC_TRACE("br\n"); continue; }
		int et;
		uint64_t er;
		if (!ast_ident_etype(a, e, &et, &er) || !ast_cse_wide(et))
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_licm_operands_ok(a, n, e))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal post = AST_NONE, prhs = AST_NONE;
		for (AstLocal s = ast_next_sib(a, n); s != AST_NONE; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
			if (ast_sccp_has_label(a, s))
				{ MCC_TRACE("br\n"); break; }
			if (ast_kind(a, s) == AST_Store) { MCC_TRACE("br\n");
				AstLocal rhs = ast_child(a, s, 1);
				if (ast_pre_occurs(a, rhs, e) && ast_licm_operands_ok(a, s, e)) { MCC_TRACE("br\n");
					post = s;
					prhs = rhs;
					break;
				}
			}
			if (!ast_licm_operands_ok(a, s, e))
				{ MCC_TRACE("br\n"); break; }
		}
		if (post == AST_NONE)
			{ MCC_TRACE("br\n"); continue; }
		int pal, psz = ast_ltemp_size(et, er, &pal);
		int off = ast_ltemp_mint(psz, pal);
		if (!off)
			{ MCC_TRACE("br\n"); continue; }
		AstLocal lref = ast_node(a, AST_Ref);
		ast_set_op(a, lref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, lref, (uint64_t)off);
		ast_set_type(a, lref, et, er);
		AstLocal cvt = ast_node(a, AST_Convert);
		ast_set_type(a, cvt, et, er);
		ast_add_child(a, cvt, ast_dup_sub(a, e));
		AstLocal st = ast_node(a, AST_Store);
		ast_add_child(a, st, lref);
		ast_add_child(a, st, cvt);
		if (!ast_ltemp_insert_before(a, parent, n, st))
			{ MCC_TRACE("br\n"); continue; }
		AstLocal tref = ast_node(a, AST_Ref);
		ast_set_op(a, tref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, tref, (uint64_t)off);
		ast_set_type(a, tref, et, er);
		ast_licm_subst(a, thenbb, e, tref, 0);
		ast_licm_subst(a, elsebb, e, tref, 0);
		ast_licm_subst(a, prhs, e, tref, 0);
		ast_cse_setref(a, e, tref);
		ast_licm_folds++;
		did++;
	}
	return did;
}

static void ast_cse_block(AstArena *a, AstLocal bb) { MCC_TRACE("enter\n");
	ast_cse_n = 0;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, s);
		if (k == AST_Store) { MCC_TRACE("br\n");
			AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
			ast_cse_subst(a, val, 0);
			ast_cse_subst(a, lval, 1);
			if (!ast_cprop_safe(a, lval) || !ast_cprop_safe(a, val)) { MCC_TRACE("br\n");
				ast_cse_n = 0;
				continue;
			}
			int off, tt;
			if (ast_cse_is_local(a, lval, &off, &tt)) { MCC_TRACE("br\n");
				ast_cse_kill(a, off);
				int et;
				uint64_t er;
				if (!ast_cprop_escapes(a, off) && ast_cse_n < ast_cse_window &&
						ast_cse_regpure(a, val) && !ast_ident_leaf(a, val) &&
						!ast_tco_reads_off(a, val, off) &&
						ast_ident_etype(a, val, &et, &er) && ast_cse_wide(et) &&
						(et & (VT_BTYPE | VT_UNSIGNED)) == (tt & (VT_BTYPE | VT_UNSIGNED)) &&
						(er == ast_type_ref(a, lval) ||
						 (et & VT_BTYPE) == VT_PTR)) { MCC_TRACE("br\n");
					ast_cse_expr[ast_cse_n] = val;
					ast_cse_ref[ast_cse_n] = lval;
					ast_cse_off[ast_cse_n] = off;
					ast_cse_n++;
				}
			} else { MCC_TRACE("br\n");
				ast_cse_n = 0;
			}
		} else if (k == AST_Return) { MCC_TRACE("br\n");
			{
				AstLocal rv = ast_first_child(a, s);
				if (rv != AST_NONE)
					{ MCC_TRACE("br\n"); ast_cse_subst(a, rv, 0); }
			}
			ast_cse_n = 0;
		} else if (k == AST_If && ast_op(a, s) == 0) { MCC_TRACE("br\n");
			ast_cse_subst(a, ast_child(a, s, 0), 0);
			ast_cse_n = 0;
		} else if (k == AST_Invoke) { MCC_TRACE("br\n");
			for (AstLocal c = ast_first_child(a, s); c != AST_NONE; c = ast_next_sib(a, c))
				{ MCC_TRACE("br\n"); ast_cse_subst(a, c, 0); }
			ast_cse_n = 0;
		} else if (ast_licm_is_loop(a, s)) { MCC_TRACE("br\n");
			ast_licm_at_loop(a, s);
			ast_cse_n = 0;
		} else { MCC_TRACE("br\n");
			ast_cse_n = 0;
		}
	}
}

typedef struct {
	AstLocal expr[AST_CSE_MAX];
	AstLocal ref[AST_CSE_MAX];
	int off[AST_CSE_MAX];
	int n;
} AstCseState;

static void ast_cse_state_save(AstCseState *st) { MCC_TRACE("enter\n");
	st->n = ast_cse_n;
	memcpy(st->expr, ast_cse_expr, (size_t)ast_cse_n * sizeof(AstLocal));
	memcpy(st->ref, ast_cse_ref, (size_t)ast_cse_n * sizeof(AstLocal));
	memcpy(st->off, ast_cse_off, (size_t)ast_cse_n * sizeof(int));
}

static void ast_cse_state_load(const AstCseState *st) { MCC_TRACE("enter\n");
	ast_cse_n = st->n;
	memcpy(ast_cse_expr, st->expr, (size_t)st->n * sizeof(AstLocal));
	memcpy(ast_cse_ref, st->ref, (size_t)st->n * sizeof(AstLocal));
	memcpy(ast_cse_off, st->off, (size_t)st->n * sizeof(int));
}

static void ast_cse_state_meet(const AstCseState *st) { MCC_TRACE("enter\n");
	int i = 0;
	while (i < ast_cse_n) { MCC_TRACE("br\n");
		int j, keep = 0;
		for (j = 0; j < st->n; j++)
			{ MCC_TRACE("br\n"); if (st->expr[j] == ast_cse_expr[i] && st->ref[j] == ast_cse_ref[i] &&
					st->off[j] == ast_cse_off[i]) { MCC_TRACE("br\n");
				keep = 1;
				break;
			} }
		if (keep) { MCC_TRACE("br\n");
			i++;
		} else { MCC_TRACE("br\n");
			ast_cse_n--;
			ast_cse_expr[i] = ast_cse_expr[ast_cse_n];
			ast_cse_ref[i] = ast_cse_ref[ast_cse_n];
			ast_cse_off[i] = ast_cse_off[ast_cse_n];
		}
	}
}

static unsigned char *ast_cse_vis;

static void ast_cse_stmts(AstArena *a, AstLocal bb) { MCC_TRACE("enter\n");
	if (bb == AST_NONE || ast_kind(a, bb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return; }
	ast_cse_vis[bb] = 1;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, s);
		if (k == AST_Store) { MCC_TRACE("br\n");
			AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
			ast_cse_subst(a, val, 0);
			ast_cse_subst(a, lval, 1);
			if (!ast_cprop_safe(a, lval) || !ast_cprop_safe(a, val)) { MCC_TRACE("br\n");
				ast_cse_n = 0;
				continue;
			}
			int off, tt;
			if (ast_cse_is_local(a, lval, &off, &tt)) { MCC_TRACE("br\n");
				ast_cse_kill(a, off);
				int et;
				uint64_t er;
				if (!ast_cprop_escapes(a, off) && ast_cse_n < ast_cse_window &&
						ast_cse_regpure(a, val) && !ast_ident_leaf(a, val) &&
						!ast_tco_reads_off(a, val, off) &&
						ast_ident_etype(a, val, &et, &er) && ast_cse_wide(et) &&
						(et & (VT_BTYPE | VT_UNSIGNED)) == (tt & (VT_BTYPE | VT_UNSIGNED)) &&
						(er == ast_type_ref(a, lval) ||
						 (et & VT_BTYPE) == VT_PTR)) { MCC_TRACE("br\n");
					ast_cse_expr[ast_cse_n] = val;
					ast_cse_ref[ast_cse_n] = lval;
					ast_cse_off[ast_cse_n] = off;
					ast_cse_n++;
				}
			} else { MCC_TRACE("br\n");
				ast_cse_n = 0;
			}
		} else if (k == AST_Return) { MCC_TRACE("br\n");
			{
				AstLocal rv = ast_first_child(a, s);
				if (rv != AST_NONE)
					{ MCC_TRACE("br\n"); ast_cse_subst(a, rv, 0); }
			}
			ast_cse_n = 0;
		} else if (k == AST_BasicBlock) { MCC_TRACE("br\n");
			ast_cse_stmts(a, s);
		} else if (k == AST_If && ast_op(a, s) == 0) { MCC_TRACE("br\n");
			ast_cse_subst(a, ast_child(a, s, 0), 0);
			AstLocal tb = ast_child(a, s, 1), eb = ast_child(a, s, 2);
			if (ast_cprop_opaque(a, tb) || ast_cprop_opaque(a, eb)) { MCC_TRACE("br\n");
				ast_cse_n = 0;
				continue;
			}
			AstCseState in, tout;
			ast_cse_state_save(&in);
			ast_cse_stmts(a, tb);
			ast_cse_state_save(&tout);
			ast_cse_state_load(&in);
			ast_cse_stmts(a, eb);
			ast_cse_state_meet(&tout);
		} else if (k == AST_Invoke) { MCC_TRACE("br\n");
			for (AstLocal c = ast_first_child(a, s); c != AST_NONE; c = ast_next_sib(a, c))
				{ MCC_TRACE("br\n"); ast_cse_subst(a, c, 0); }
			if (ast_call_window_env) { MCC_TRACE("br\n");
				for (int i = 0; i < ast_cse_n;)
					{ MCC_TRACE("br\n"); if (ast_cprop_escapes(a, ast_cse_off[i]) ||
							ast_licm_written(a, s, ast_cse_off[i]) ||
							!ast_licm_operands_ok(a, s, ast_cse_expr[i]))
						{ MCC_TRACE("br\n"); ast_cse_kill(a, ast_cse_off[i]); }
					else
						{ MCC_TRACE("br\n"); i++; } }
			} else { MCC_TRACE("br\n");
				ast_cse_n = 0;
			}
		} else if (k == AST_If && ast_op(a, s) >= 2 && ast_op(a, s) <= 4) { MCC_TRACE("br\n");
			ast_licm_at_loop(a, s);
			for (int i = 0; i < ast_cse_n;)
				{ MCC_TRACE("br\n"); if (ast_licm_written(a, s, ast_cse_off[i]) ||
						!ast_licm_operands_ok(a, s, ast_cse_expr[i]))
					{ MCC_TRACE("br\n"); ast_cse_kill(a, ast_cse_off[i]); }
				else
					{ MCC_TRACE("br\n"); i++; } }
			if (ast_sccp_has_label(a, s)) { MCC_TRACE("br\n");
				ast_cse_n = 0;
				continue;
			}
			AstCseState in;
			ast_cse_state_save(&in);
			for (AstLocal c = ast_first_child(a, s); c != AST_NONE;
					 c = ast_next_sib(a, c)) { MCC_TRACE("br\n");
				if (ast_kind(a, c) == AST_BasicBlock) { MCC_TRACE("br\n");
					ast_cse_stmts(a, c);
					ast_cse_state_load(&in);
				} else if (ast_cprop_safe(a, c)) { MCC_TRACE("br\n");
					ast_cse_subst(a, c, 0);
				}
			}
		} else if (ast_licm_is_loop(a, s)) { MCC_TRACE("br\n");
			ast_licm_at_loop(a, s);
			ast_cse_n = 0;
		} else { MCC_TRACE("br\n");
			ast_cse_n = 0;
		}
	}
}

static int ast_cse_run(AstArena *a) { MCC_TRACE("enter\n");
	ast_cse_folds = 0;
	ast_licm_folds = 0;
	AstLocal nn = ast_count(a);
	if (ast_cse_join_env && nn) { MCC_TRACE("br\n");
		ast_cse_vis = mcc_mallocz(nn);
		ast_cse_n = 0;
		ast_cse_stmts(a, ast_root(a));
		for (AstLocal n = 0; n < nn; n++)
			{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_BasicBlock && !ast_cse_vis[n])
				{ MCC_TRACE("br\n"); ast_cse_block(a, n); } }
		mcc_free(ast_cse_vis);
		ast_cse_vis = NULL;
		return ast_cse_folds;
	}
	for (AstLocal n = 0; n < nn; n++)
		{ MCC_TRACE("br\n"); if (ast_kind(a, n) == AST_BasicBlock)
			{ MCC_TRACE("br\n"); ast_cse_block(a, n); } }
	return ast_cse_folds;
}

static int ast_replay_ok(AstArena *a) { MCC_TRACE("enter\n");
	return ast_first_child(a, ast_root(a)) != AST_NONE;
}


static int ast_reloc_sym_equiv(unsigned s1, unsigned s2) { MCC_TRACE("enter\n");
	ElfSym *e1, *e2;
	const char *n1, *n2;
	unsigned long n;
	if (s1 == s2)
		{ MCC_TRACE("br\n"); return 1; }
	if (!s1 || !s2 || !symtab_section || !symtab_section->link)
		{ MCC_TRACE("br\n"); return 0; }
	n = symtab_section->data_offset / sizeof(ElfSym);
	if (s1 >= n || s2 >= n)
		{ MCC_TRACE("br\n"); return 0; }
	e1 = &((ElfSym *)symtab_section->data)[s1];
	e2 = &((ElfSym *)symtab_section->data)[s2];
	if (ELFW(ST_BIND)(e1->st_info) != STB_LOCAL ||
			ELFW(ST_BIND)(e2->st_info) != STB_LOCAL)
		{ MCC_TRACE("br\n"); return 0; }
	if (e1->st_info != e2->st_info || e1->st_other != e2->st_other ||
			e1->st_shndx != e2->st_shndx || e1->st_value != e2->st_value ||
			e1->st_size != e2->st_size)
		{ MCC_TRACE("br\n"); return 0; }
	if (e1->st_name >= symtab_section->link->data_offset ||
			e2->st_name >= symtab_section->link->data_offset)
		{ MCC_TRACE("br\n"); return 0; }
	n1 = (const char *)symtab_section->link->data + e1->st_name;
	n2 = (const char *)symtab_section->link->data + e2->st_name;
	return strcmp(n1, n2) == 0;
}

static int ast_reloc_range_equiv(const unsigned char *ra, const unsigned char *rb,
																 int len) { MCC_TRACE("enter\n");
	int i, n;
	if (len >= 0 && memcmp(ra, rb, (size_t)len) == 0)
		{ MCC_TRACE("br\n"); return 1; }
	if (!ast_reloc_equiv_env)
		{ MCC_TRACE("br\n"); return 0; }
	if (len < 0 || (size_t)len % sizeof(ElfW_Rel))
		{ MCC_TRACE("br\n"); return 0; }
	n = (int)((size_t)len / sizeof(ElfW_Rel));
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		const ElfW_Rel *r1 = &((const ElfW_Rel *)ra)[i];
		const ElfW_Rel *r2 = &((const ElfW_Rel *)rb)[i];
		if (r1->r_offset != r2->r_offset)
			{ MCC_TRACE("br\n"); return 0; }
		if (ELFW(R_TYPE)(r1->r_info) != ELFW(R_TYPE)(r2->r_info))
			{ MCC_TRACE("br\n"); return 0; }
		if (ELFW_R_ADDEND(r1) != ELFW_R_ADDEND(r2))
			{ MCC_TRACE("br\n"); return 0; }
		if (!ast_reloc_sym_equiv((unsigned)ELFW(R_SYM)(r1->r_info),
														 (unsigned)ELFW(R_SYM)(r2->r_info)))
			{ MCC_TRACE("br\n"); return 0; }
	}
	return 1;
}

static int ast_verify_diff_match(const char *fn) { MCC_TRACE("enter\n");
	char sel[96];
	const char *comma = strchr(ast_verify_diff, ',');
	size_t n = comma ? (size_t)(comma - ast_verify_diff) : strlen(ast_verify_diff);
	if (n >= sizeof sel)
		{ MCC_TRACE("br\n"); n = sizeof sel - 1; }
	memcpy(sel, ast_verify_diff, n);
	sel[n] = 0;
	if (!strcmp(sel, "1") || !strcmp(sel, "all") || !strcmp(sel, "full"))
		{ MCC_TRACE("br\n"); return 1; }
	return strstr(fn, sel) != NULL;
}

static void ast_verify_dump_diff(const char *fn, const unsigned char *base,
																 int blen, const unsigned char *repl,
																 int rlen) { MCC_TRACE("enter\n");
	int lim = blen < rlen ? blen : rlen;
	int d = 0;
	while (d < lim && base[d] == repl[d])
		{ MCC_TRACE("br\n"); d++; }
	fprintf(stderr, "[ast-diff] %s: baseline %d B, replay %d B", fn, blen, rlen);
	if (d == lim && blen == rlen)
		{ MCC_TRACE("br\n"); fprintf(stderr, " (code identical — relocation/length divergence)\n"); }
	else
		{ MCC_TRACE("br\n"); fprintf(stderr, ", first diff @ +%d\n", d); }
	int full = ast_verify_diff && strstr(ast_verify_diff, "full") != NULL;
	int lo = full ? 0 : (d - 8 < 0 ? 0 : d - 8);
	int win = full ? (blen > rlen ? blen : rlen) : 96;
	fprintf(stderr, "  base @+%d:", lo);
	for (int i = lo; i < lo + win && i < blen; i++)
		{ MCC_TRACE("br\n"); fprintf(stderr, " %02x", base[i]); }
	fprintf(stderr, "\n  repl @+%d:", lo);
	for (int i = lo; i < lo + win && i < rlen; i++)
		{ MCC_TRACE("br\n"); fprintf(stderr, " %02x", repl[i]); }
	fprintf(stderr, "\n");
}

void ast_func_begin(Sym *sym) { MCC_TRACE("enter\n");
	MCC_TRACE("%s\n", funcname);
	ast_slice_self_sym = (void *)sym;
	lcen_fn_n = 0;
	lcen_fn_ovf = 0;
	ast_fn_switch = 0;
	ast_inline_capture(sym);
	ast_body_ind_sv = ind;
	ast_reloc0_sv =
			cur_text_section->reloc ? cur_text_section->reloc->data_offset : 0;
	ast_locrec_n = 0;
	ast_locrec_min = 0;
	if (rir_try_active) { MCC_TRACE("br\n");
		ast_cur = ast_arena_new();
		ast_cur_bb = ast_node(ast_cur, AST_BasicBlock);
		ast_reemit_poison = 0;
		ast_base_depth = (int)(vtop - vstack + 1);
		ast_fconst_n = 0;
		ast_replaying = 0;
		ast_switch_node = AST_NONE;
		ast_func_has_asm = 0;
		ast_func_has_labeladdr = 0;
		ast_active = 1;
		ast_sym_deferred_n = 0;
		ast_sym_defer_on = 1;
	}
	if (rir_try_active) { MCC_TRACE("br\n");
		ir_cap_reset();
		ir_cap_active = 1;
		rir_reset();
		rir_active = 1;
		rir_started = 1;
	}
}

typedef struct AstStrategy {
	const char *name;
	int (*gate)(void);
	int (*apply)(AstArena *a, Sym *sym);
} AstStrategy;




static int ast_cload_addr(AstArena *a, AstLocal n, Sym **sym, int64_t *off,
													CType *pt, int depth);

static int ast_cload_lval(AstArena *a, AstLocal n, Sym **sym, int64_t *off,
													CType *dt, int depth) { MCC_TRACE("enter\n");
	int k, r;
	if (n == AST_NONE || depth > 24)
		{ MCC_TRACE("br\n"); return 0; }
	k = ast_kind(a, n);
	if (k == AST_Ref) { MCC_TRACE("br\n");
		r = ast_op(a, n);
		if (ast_nchild(a, n) || (r & VT_VALMASK) != VT_CONST || !(r & VT_SYM) ||
				!(r & VT_LVAL) || !ast_sym(a, n))
			{ MCC_TRACE("br\n"); return 0; }
		*sym = (Sym *)(uintptr_t)ast_sym(a, n);
		*off = (int64_t)ast_ival(a, n);
		dt->t = ast_type_t(a, n);
		dt->ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		return 1;
	}
	if (k == AST_Load && ast_nchild(a, n) == 1)
		{ MCC_TRACE("br\n"); return ast_cload_addr(a, ast_child(a, n, 0), sym, off, dt,
																							 depth + 1); }
	if (k == AST_Unary && ast_op(a, n) == AST_OP_MEMBER &&
			ast_nchild(a, n) == 1) { MCC_TRACE("br\n");
		CType base;
		if (!ast_cload_lval(a, ast_child(a, n, 0), sym, off, &base, depth + 1))
			{ MCC_TRACE("br\n"); return 0; }
		*off += (int)ast_ival(a, n);
		dt->t = ast_type_t(a, n);
		dt->ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		return 1;
	}
	return 0;
}

static int ast_cload_addr(AstArena *a, AstLocal n, Sym **sym, int64_t *off,
													CType *pt, int depth) { MCC_TRACE("enter\n");
	CType desig;
	int k, t;
	uint64_t tref;
	if (n == AST_NONE || depth > 24)
		{ MCC_TRACE("br\n"); return 0; }
	k = ast_kind(a, n);
	t = ast_type_t(a, n);
	tref = ast_type_ref(a, n);
	if (k == AST_Convert && ast_nchild(a, n) == 1) { MCC_TRACE("br\n");
		if ((t & (VT_BTYPE | VT_ARRAY)) != VT_PTR || !tref)
			{ MCC_TRACE("br\n"); return 0; }
		if (!ast_cload_addr(a, ast_child(a, n, 0), sym, off, pt, depth + 1))
			{ MCC_TRACE("br\n"); return 0; }
		*pt = ((Sym *)(uintptr_t)tref)->type;
		return 1;
	}
	if (k == AST_Binary && ast_nchild(a, n) == 2) { MCC_TRACE("br\n");
		int bop = ast_op(a, n), align, lt, neg = 0;
		AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1), pn, ln;
		int64_t idx, esz;
		if (bop == '+') { MCC_TRACE("br\n");
			if (ast_kind(a, y) == AST_Literal)
				{ MCC_TRACE("br\n"); pn = x; ln = y; }
			else if (ast_kind(a, x) == AST_Literal)
				{ MCC_TRACE("br\n"); pn = y; ln = x; }
			else
				{ MCC_TRACE("br\n"); return 0; }
		} else if (bop == '-') { MCC_TRACE("br\n");
			if (ast_kind(a, y) != AST_Literal)
				{ MCC_TRACE("br\n"); return 0; }
			pn = x;
			ln = y;
			neg = 1;
		} else { MCC_TRACE("br\n");
			return 0;
		}
		lt = ast_type_t(a, ln);
		if (ast_bad_type(lt) || is_float(lt) || (lt & VT_ARRAY) ||
				ast_nchild(a, ln))
			{ MCC_TRACE("br\n"); return 0; }
		if (!ast_cload_addr(a, pn, sym, off, pt, depth + 1))
			{ MCC_TRACE("br\n"); return 0; }
		esz = type_size(pt, &align);
		if (esz <= 0)
			{ MCC_TRACE("br\n"); return 0; }
		idx = (int64_t)value64(ast_ival(a, ln), lt);
		if (idx > (int64_t)1 << 30 || idx < -((int64_t)1 << 30))
			{ MCC_TRACE("br\n"); return 0; }
		*off += (neg ? -idx : idx) * esz;
		return 1;
	}
	if (k == AST_Ref && !ast_nchild(a, n)) { MCC_TRACE("br\n");
		int r = ast_op(a, n);
		if ((r & (VT_VALMASK | VT_LVAL | VT_SYM)) != (VT_CONST | VT_SYM) ||
				!ast_sym(a, n) || (t & VT_BTYPE) != VT_PTR || !tref)
			{ MCC_TRACE("br\n"); return 0; }
		*sym = (Sym *)(uintptr_t)ast_sym(a, n);
		*off = (int64_t)ast_ival(a, n);
		*pt = ((Sym *)(uintptr_t)tref)->type;
		return 1;
	}
	if (!ast_cload_lval(a, n, sym, off, &desig, depth))
		{ MCC_TRACE("br\n"); return 0; }
	if ((desig.t & (VT_BTYPE | VT_ARRAY)) != (VT_PTR | VT_ARRAY) || !desig.ref)
		{ MCC_TRACE("br\n"); return 0; }
	*pt = desig.ref->type;
	return 1;
}

static int ast_cload_readonly(Sym *sym) { MCC_TRACE("enter\n");
	ElfSym *esym;
	Section *ssec;
	if (!sym || sym->a.weak)
		{ MCC_TRACE("br\n"); return 0; }
	esym = elfsym(sym);
	if (!esym || esym->st_shndx == SHN_UNDEF ||
			esym->st_shndx >= mcc_state->nb_sections)
		{ MCC_TRACE("br\n"); return 0; }
	ssec = mcc_state->sections[esym->st_shndx];
	if (!ssec || (ssec->sh_flags & SHF_WRITE))
		{ MCC_TRACE("br\n"); return 0; }
	return 1;
}

static int ast_cload_rvalue_use(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal p = ast_parent(a, n);
	if (p == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	switch (ast_kind(a, p)) { MCC_TRACE("br\n");
	case AST_Binary:
		return ast_op(a, p) < AST_OP_ADDR;
	case AST_Convert:
	case AST_If:
	case AST_Return:
		return 1;
	case AST_Invoke:
		return ast_child(a, p, 0) != n;
	case AST_Store:
		return ast_op(a, p) != AST_OP_OPASSIGN && ast_child(a, p, 0) != n;
	default:
		return 0;
	}
}

static int ast_cload_run(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal n, nn = ast_count(a);
	int hits = 0;
	if (!ast_cload_env)
		{ MCC_TRACE("br\n"); return 0; }
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		Sym *sym = NULL;
		CType lt;
		SValue v;
		int64_t off = 0;
		int bt, sv_ice, k = ast_kind(a, n);
		if (k == AST_Load && ast_nchild(a, n) == 1) { MCC_TRACE("br\n");
			if (!(ast_fbits(a, n) & AST_FB_LOAD_LVAL) && ast_load_over_member(a, n))
				{ MCC_TRACE("br\n"); continue; }
			if (!ast_cload_rvalue_use(a, n))
				{ MCC_TRACE("br\n"); continue; }
			if (!ast_cload_addr(a, ast_child(a, n, 0), &sym, &off, &lt, 0))
				{ MCC_TRACE("br\n"); continue; }
		} else if (k == AST_Ref ||
							 (k == AST_Unary && ast_op(a, n) == AST_OP_MEMBER)) { MCC_TRACE("br\n");
			if (!ast_cload_rvalue_use(a, n))
				{ MCC_TRACE("br\n"); continue; }
			if (!ast_cload_lval(a, n, &sym, &off, &lt, 0))
				{ MCC_TRACE("br\n"); continue; }
		} else { MCC_TRACE("br\n");
			continue;
		}
		if (!ast_cload_readonly(sym))
			{ MCC_TRACE("br\n"); continue; }
		bt = lt.t & VT_BTYPE;
		if (ast_bad_type(lt.t) || is_float(lt.t) || bt == VT_STRUCT ||
				bt == VT_VOID || bt == VT_FUNC || bt == VT_PTR || (lt.t & VT_ARRAY))
			{ MCC_TRACE("br\n"); continue; }
		memset(&v, 0, sizeof v);
		v.r = VT_CONST | VT_SYM | VT_LVAL;
		v.sym = sym;
		v.c.i = (uint64_t)off;
		v.type = lt;
		sv_ice = ice_nonconst;
		if (!fold_const_lval_at(&v)) { MCC_TRACE("br\n");
			ice_nonconst = sv_ice;
			continue;
		}
		ice_nonconst = sv_ice;
		ast_set_kind(a, n, AST_Literal);
		ast_clear_children(a, n);
		ast_set_op(a, n, VT_CONST);
		ast_set_type_bf(a, n, lt.t, (uint64_t)(uintptr_t)lt.ref, lt.bp, lt.bs);
		ast_set_ival(a, n, v.c.i);
		ast_set_wide(a, n, 0, AST_R2_NONE);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		hits++;
	}
	return hits;
}

#define AST_SRA_MAX 16

typedef struct {
	uint64_t base;
	uint64_t sym;
	int size;
	int ok;
	int nmem;
	int32_t moff[AST_SRA_MAX];
	int mtype[AST_SRA_MAX];
	uint64_t mref[AST_SRA_MAX];
	int slot[AST_SRA_MAX];
} AstSraCand;

static MCC_OPT_TLS int ast_sra_folds;

static int ast_sra_scalar_size(int t) { MCC_TRACE("enter\n");
	if (t & VT_ARRAY)
		{ MCC_TRACE("br\n"); return 0; }
	switch (t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_BOOL:
	case VT_BYTE:
		return 1;
	case VT_SHORT:
		return 2;
	case VT_INT:
	case VT_FLOAT:
		return 4;
	case VT_LLONG:
	case VT_DOUBLE:
		return 8;
	case VT_PTR:
		return MCC_PTR_SIZE;
	}
	return 0;
}

static int ast_sra_ref_local(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	return n != AST_NONE && ast_kind(a, n) == AST_Ref &&
				 (ast_op(a, n) & VT_VALMASK) == VT_LOCAL && !(ast_op(a, n) & VT_SYM);
}

static int ast_sra_struct_ref(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int t;
	if (!ast_sra_ref_local(a, n))
		{ MCC_TRACE("br\n"); return 0; }
	t = ast_type_t(a, n);
	if ((t & VT_BTYPE) != VT_STRUCT || (t & VT_ARRAY) || (t & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	if (IS_UNION(t))
		{ MCC_TRACE("br\n"); return 0; }
	return ast_type_ref(a, n) != 0;
}

static int ast_sra_member_use(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal p = ast_parent(a, n);
	if (p == AST_NONE || ast_kind(a, p) != AST_Unary ||
			ast_op(a, p) != AST_OP_MEMBER)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_first_child(a, p) == n;
}

static int ast_sra_member_ok(AstArena *a, AstLocal m) { MCC_TRACE("enter\n");
	int t = ast_type_t(a, m);
	if (ast_type_bp(a, m) || ast_type_bs(a, m))
		{ MCC_TRACE("br\n"); return 0; }
	if (!t || ast_bad_type(t) || (t & VT_ARRAY) || (t & VT_VOLATILE))
		{ MCC_TRACE("br\n"); return 0; }
	if ((t & VT_BTYPE) == VT_STRUCT)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_sra_scalar_size(t) > 0 && ast_sra_scalar_size(t) <= 8;
}

static AstSraCand *ast_sra_find(AstSraCand *c, int n, uint64_t base) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); if (c[i].base == base) { MCC_TRACE("br\n"); return &c[i]; } }
	return NULL;
}

/* Scalar replacement of aggregates.
 *
 * A struct local whose every appearance is the base of a `.member` access is
 * not really an aggregate: it is a fixed set of independent scalars that happen
 * to share one frame slot. Rewriting each `Unary(MEMBER, Ref base)` into a
 * plain `Ref` at its own slot is a pure renaming, so it needs no initialiser
 * and no copy -- the stores that wrote the members now write the new slots.
 *
 * The legality rule is deliberately one rule rather than a list: a candidate
 * dies unless EVERY `Ref` at its frame offset is child 0 of a member access.
 * That single test subsumes address-taken, pass- and return-by-value, whole
 * struct assignment (which is an `AST_Store` whose child 0 is the struct `Ref`,
 * with the copy generated inside vstore() at replay and no node of its own),
 * and anything else that could observe the object as a unit. Bitfields are
 * refused because their mask/shift is generated at replay from `bp`/`bs` on the
 * member node, which a plain `Ref` cannot carry; unions because two fields can
 * share bytes; nested aggregates and arrays because a slot is not a region. */
static int ast_sra_run(AstArena *a, int separate) { MCC_TRACE("enter\n");
	AstSraCand cand[AST_SRA_MAX];
	int ncand = 0, i, j;
	AstLocal nn = ast_count(a);
	ast_sra_folds = 0;
	if (!a || ast_func_has_asm || ast_func_has_labeladdr)
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstSraCand *c;
		CType ct;
		if (!ast_sra_struct_ref(a, n))
			{ MCC_TRACE("br\n"); continue; }
		c = ast_sra_find(cand, ncand, ast_ival(a, n));
		if (!c) { MCC_TRACE("br\n");
			if (ncand >= AST_SRA_MAX)
				{ MCC_TRACE("br\n"); continue; }
			c = &cand[ncand++];
			c->base = ast_ival(a, n);
			c->sym = ast_sym(a, n);
			c->ok = 1;
			c->nmem = 0;
			for (j = 0; j < AST_SRA_MAX; j++)
				{ MCC_TRACE("br\n"); c->slot[j] = 0; }
			ct.t = ast_type_t(a, n);
			ct.bp = 0;
			ct.bs = 0;
			ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			{ MCC_TRACE("br\n");
				int al = 0;
				c->size = (!ct.ref) ? -1 : type_size(&ct, &al);
			}
			if (c->size <= 0)
				{ MCC_TRACE("br\n"); c->ok = 0; }
		}
		if (c->sym != ast_sym(a, n))
			{ MCC_TRACE("br\n"); c->ok = 0; }
		if (!ast_sra_member_use(a, n))
			{ MCC_TRACE("br\n"); c->ok = 0; }
	}
	if (!ncand)
		{ MCC_TRACE("br\n"); return 0; }
	/* Any reference landing inside a candidate's byte range that is not one of
	 * its own member accesses means the frame slot is observed some other way. */
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (!ast_sra_ref_local(a, n))
			{ MCC_TRACE("br\n"); continue; }
		for (i = 0; i < ncand; i++) { MCC_TRACE("br\n");
			int64_t d;
			if (!cand[i].ok || cand[i].size <= 0)
				{ MCC_TRACE("br\n"); continue; }
			d = (int64_t)ast_ival(a, n) - (int64_t)cand[i].base;
			if (d > 0 && d < cand[i].size)
				{ MCC_TRACE("br\n"); cand[i].ok = 0; }
		}
	}
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstSraCand *c;
		AstLocal base;
		int32_t mo;
		if (ast_kind(a, n) != AST_Unary || ast_op(a, n) != AST_OP_MEMBER)
			{ MCC_TRACE("br\n"); continue; }
		base = ast_first_child(a, n);
		if (!ast_sra_struct_ref(a, base))
			{ MCC_TRACE("br\n"); continue; }
		c = ast_sra_find(cand, ncand, ast_ival(a, base));
		if (!c || !c->ok)
			{ MCC_TRACE("br\n"); continue; }
		if (!ast_sra_member_ok(a, n))
			{ MCC_TRACE("br\n"); c->ok = 0; continue; }
		mo = (int32_t)ast_ival(a, n);
		for (j = 0; j < c->nmem; j++)
			{ MCC_TRACE("br\n"); if (c->moff[j] == mo) { MCC_TRACE("br\n"); break; } }
		if (j == c->nmem) { MCC_TRACE("br\n");
			if (c->nmem >= AST_SRA_MAX)
				{ MCC_TRACE("br\n"); c->ok = 0; continue; }
			c->moff[c->nmem] = mo;
			c->mtype[c->nmem] = ast_type_t(a, n);
			c->mref[c->nmem] = ast_type_ref(a, n);
			c->nmem++;
		} else if (c->mtype[j] != ast_type_t(a, n)) { MCC_TRACE("br\n");
			c->ok = 0;
		}
	}
	for (i = 0; i < ncand; i++)
		{ MCC_TRACE("br\n"); if (cand[i].nmem < 1) { MCC_TRACE("br\n"); cand[i].ok = 0; } }
	/* A member gets its own frame slot only in `separate` mode.  Rewriting in
	 * place to base+k is byte-neutral in this compiler -- Unary(MEMBER, Ref) and
	 * a plain Ref at base+k emit the same code -- so the decomposition only buys
	 * anything once the members stop being one object to every later pass.  Mint
	 * before the rewrite and abandon the whole candidate if the allocator is
	 * full, because a half-slotted struct is a miscompile. */
	if (separate) { MCC_TRACE("br\n");
		for (i = 0; i < ncand; i++) { MCC_TRACE("br\n");
			if (!cand[i].ok)
				{ MCC_TRACE("br\n"); continue; }
			for (j = 0; j < cand[i].nmem; j++) { MCC_TRACE("br\n");
				int msz = ast_sra_scalar_size(cand[i].mtype[j]);
				int off = ast_ltemp_mint(msz, msz > 0 && msz <= 8 ? msz : 8);
				if (!off) { MCC_TRACE("br\n"); cand[i].ok = 0; break; }
				cand[i].slot[j] = off;
			}
		}
	}
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstSraCand *c;
		AstLocal base;
		int32_t mo;
		if (ast_kind(a, n) != AST_Unary || ast_op(a, n) != AST_OP_MEMBER)
			{ MCC_TRACE("br\n"); continue; }
		base = ast_first_child(a, n);
		if (!ast_sra_struct_ref(a, base))
			{ MCC_TRACE("br\n"); continue; }
		c = ast_sra_find(cand, ncand, ast_ival(a, base));
		if (!c || !c->ok)
			{ MCC_TRACE("br\n"); continue; }
		mo = (int32_t)ast_ival(a, n);
		for (j = 0; j < c->nmem; j++)
			{ MCC_TRACE("br\n"); if (c->moff[j] == mo) { MCC_TRACE("br\n"); break; } }
		if (j == c->nmem)
			{ MCC_TRACE("br\n"); continue; }
		ast_clear_children(a, n);
		a->kind[n] = AST_Ref;
		ast_set_op(a, n, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, n, separate
												 ? (uint64_t)(int64_t)c->slot[j]
												 : (uint64_t)((int64_t)c->base + (int64_t)mo));
		ast_set_type(a, n, c->mtype[j], c->mref[j]);
		ast_set_sym(a, n, separate ? 0 : c->sym);
		ast_set_fbits(a, n, 0);
		ast_sra_folds++;
	}
	if (ast_sra_folds)
		{ MCC_TRACE("br\n"); a->epoch++; }
	return ast_sra_folds;
}


#define AST_SROA_CAND 8
#define AST_SROA_MEM 24

typedef struct {
	uint64_t base;
	int size;
	int ok;
	int nmem;
	int nused;
	int32_t moff[AST_SROA_MEM];
	int mtype[AST_SROA_MEM];
	uint64_t mref[AST_SROA_MEM];
	int msz[AST_SROA_MEM];
	int slot[AST_SROA_MEM];
	int used[AST_SROA_MEM];
	int stored[AST_SROA_MEM];
	int movable[AST_SROA_MEM];
	int isparam;
} AstSroaCand;

static MCC_OPT_TLS int ast_sroa_folds;

enum {
	AST_SROA_W_PARAM = 0,
	AST_SROA_W_CPLX,
	AST_SROA_W_MEMBERS,
	AST_SROA_W_M_BITFIELD,
	AST_SROA_W_M_NESTED,
	AST_SROA_W_M_ARRAY,
	AST_SROA_W_M_WIDE,
	AST_SROA_W_M_MANY,
	AST_SROA_W_M_NOREF,
	AST_SROA_W_ADDR,
	AST_SROA_W_WHOLE,
	AST_SROA_W_STRADDLE,
	AST_SROA_W_CLASS,
	AST_SROA_W_NOSTORE,
	AST_SROA_W_SLOTS,
	AST_SROA_W_DEAD,
	AST_SROA_W_OK,
	AST_SROA_W_COUNT
};
static const char *const ast_sroa_why_name[AST_SROA_W_COUNT] = {
		"param", "complex", "members", "m:bitfield", "m:nested", "m:array",
		"m:wide", "m:many", "m:noref", "addr-taken", "whole-struct",
		"straddle", "type-class", "no-store", "slots", "dead", "ok"};
static long ast_sroa_why[AST_SROA_W_COUNT];
static int ast_sroa_why_on = -1;

static void ast_sroa_why_dump(void) { MCC_TRACE("enter\n");
	int i;
	fprintf(stderr, "[sroa]");
	for (i = 0; i < AST_SROA_W_COUNT; i++)
		fprintf(stderr, " %s=%ld", ast_sroa_why_name[i], ast_sroa_why[i]);
	fprintf(stderr, "\n");
}

static void ast_sroa_note(int w) { MCC_TRACE("enter\n");
	if (ast_sroa_why_on < 0) { MCC_TRACE("br\n");
		ast_sroa_why_on = mcc_env_on("MCC_SROA_WHY");
		if (ast_sroa_why_on)
			{ MCC_TRACE("br\n"); atexit(ast_sroa_why_dump); }
	}
	if (ast_sroa_why_on)
		{ MCC_TRACE("br\n"); ast_sroa_why[w]++; }
}

static int ast_sroa_members(AstArena *a, AstLocal n, AstSroaCand *c) { MCC_TRACE("enter\n");
	CType ct;
	Sym *f;
	int al;
	ct.t = ast_type_t(a, n);
	ct.bp = 0;
	ct.bs = 0;
	ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
	if (!ct.ref)
		{ MCC_TRACE("br\n"); ast_sroa_note(AST_SROA_W_M_NOREF); return 0; }
	c->size = type_size(&ct, &al);
	if (c->size <= 0)
		{ MCC_TRACE("br\n"); ast_sroa_note(AST_SROA_W_M_NOREF); return 0; }
	c->nmem = 0;
	c->nused = 0;
	/* A member this pass cannot move is pinned, not fatal.  Refusing the whole
	 * object because one field is a bitfield throws away every other field with
	 * it, and over tests/exec that single rule accounted for 212 of 387 member
	 * refusals.  A pinned member keeps its bytes at base+moff and every access
	 * to it is left exactly as it was; only movable members change address, so
	 * each byte still has exactly one home. */
	for (f = ct.ref->next; f; f = f->next) { MCC_TRACE("br\n");
		int mt = f->type.t, msz, mv = 1, al2;
		CType mct;
		if (c->nmem >= AST_SROA_MEM)
			{ MCC_TRACE("br\n"); ast_sroa_note(AST_SROA_W_M_MANY); return 0; }
		mct.t = mt;
		mct.bp = f->type.bp;
		mct.bs = f->type.bs;
		mct.ref = f->type.ref;
		msz = ast_sra_scalar_size(mt);
		if ((mt & VT_BITFIELD) || f->type.bp || f->type.bs)
			{ MCC_TRACE("br\n"); mv = 0; ast_sroa_note(AST_SROA_W_M_BITFIELD); }
		else if (mt & VT_VOLATILE)
			{ MCC_TRACE("br\n"); mv = 0; ast_sroa_note(AST_SROA_W_M_WIDE); }
		else if ((mt & VT_BTYPE) == VT_STRUCT)
			{ MCC_TRACE("br\n"); mv = 0; ast_sroa_note(AST_SROA_W_M_NESTED); }
		else if (mt & VT_ARRAY)
			{ MCC_TRACE("br\n"); mv = 0; ast_sroa_note(AST_SROA_W_M_ARRAY); }
		else if (msz <= 0 || msz > 8)
			{ MCC_TRACE("br\n"); mv = 0; ast_sroa_note(AST_SROA_W_M_WIDE); }
		if (!mv) { MCC_TRACE("br\n");
			/* A pinned member needs a byte extent so a Ref can be attributed to
			 * it.  For a bitfield that is its storage unit, which several
			 * bitfields share -- member_covering takes the latest start that
			 * covers, so the unit is attributed to the last one declared in it and
			 * every access inside it is left alone either way. */
			mct.bp = 0;
			mct.bs = 0;
			mct.t = mt & ~VT_BITFIELD;
			msz = type_size(&mct, &al2);
			if (msz <= 0)
				{ MCC_TRACE("br\n"); ast_sroa_note(AST_SROA_W_M_NOREF); return 0; }
		}
		c->moff[c->nmem] = (int32_t)f->c;
		c->mtype[c->nmem] = mt;
		c->mref[c->nmem] = (uint64_t)(uintptr_t)f->type.ref;
		c->msz[c->nmem] = msz;
		c->movable[c->nmem] = mv;
		c->slot[c->nmem] = 0;
		c->used[c->nmem] = 0;
		c->stored[c->nmem] = 0;
		c->nmem++;
	}
	return c->nmem > 0;
}

static int ast_sroa_member_at(AstSroaCand *c, int32_t d) { MCC_TRACE("enter\n");
	int j;
	for (j = 0; j < c->nmem; j++)
		{ MCC_TRACE("br\n"); if (c->moff[j] == d) { MCC_TRACE("br\n"); return j; } }
	return -1;
}

/* A bitfield member has no byte extent of its own -- several share a storage
 * unit -- so `covers` treats any offset at or after a pinned bitfield's start,
 * up to the next member, as belonging to it. */
static int ast_sroa_member_covering(AstSroaCand *c, int32_t d, int rsz) { MCC_TRACE("enter\n");
	int j, best = -1;
	for (j = 0; j < c->nmem; j++) { MCC_TRACE("br\n");
		int32_t lo = c->moff[j];
		int32_t hi = lo + (c->msz[j] > 0 ? c->msz[j] : 8);
		if (d >= lo && d + (rsz > 0 ? rsz : 1) <= hi)
			{ MCC_TRACE("br\n"); if (best < 0 || c->moff[j] > c->moff[best]) { MCC_TRACE("br\n"); best = j; } }
	}
	return best;
}

static AstSroaCand *ast_sroa_find(AstSroaCand *c, int n, uint64_t base) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); if (c[i].base == base) { MCC_TRACE("br\n"); return &c[i]; } }
	return NULL;
}

static int ast_sroa_is_store_dest(AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal p = ast_parent(a, n);
	if (p == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, p) != AST_Store && ast_kind(a, p) != AST_StoreVal)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_first_child(a, p) == n;
}

static int ast_sroa_insert_head(AstArena *a, AstLocal bb, AstLocal node) { MCC_TRACE("enter\n");
	a->epoch++;
	a->parent[node] = bb;
	a->next_sib[node] = a->first_child[bb];
	a->first_child[bb] = node;
	a->nchild[bb]++;
	return 1;
}

static int ast_sroa_scatter(AstArena *a, AstSroaCand *c, int j) { MCC_TRACE("enter\n");
	AstLocal bb = ast_root(a), src, dst, st;
	if (bb == AST_NONE || ast_kind(a, bb) != AST_BasicBlock)
		{ MCC_TRACE("br\n"); return 0; }
	src = ast_node(a, AST_Ref);
	ast_set_op(a, src, VT_LOCAL | VT_LVAL);
	ast_set_ival(a, src, (uint64_t)((int64_t)c->base + (int64_t)c->moff[j]));
	ast_set_type(a, src, c->mtype[j], c->mref[j]);
	dst = ast_node(a, AST_Ref);
	ast_set_op(a, dst, VT_LOCAL | VT_LVAL);
	ast_set_ival(a, dst, (uint64_t)(int64_t)c->slot[j]);
	ast_set_type(a, dst, c->mtype[j], c->mref[j]);
	st = ast_node(a, AST_Store);
	ast_add_child(a, st, dst);
	ast_add_child(a, st, src);
	return ast_sroa_insert_head(a, bb, st);
}

static AstSroaCand *ast_sroa_covering(AstSroaCand *c, int n, int64_t off,
																			int32_t *dout) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		int64_t d = off - (int64_t)c[i].base;
		if (d >= 0 && d < c[i].size) { MCC_TRACE("br\n");
			*dout = (int32_t)d;
			return &c[i];
		}
	}
	return NULL;
}

/* Aggressive scalar replacement.
 *
 * The in-place form (ast_sra_run) rewrites Unary(MEMBER, Ref base) to a Ref at
 * base+k and is byte-neutral, because in this compiler those two emit the same
 * code.  The reason it is *also* rare is sharper than that: the front end has
 * already folded most member accesses into a raw Ref at base+k before the arena
 * exists, so the MEMBER node the pass keys on is only present where folding was
 * not possible -- and the in-place check treats every one of those folded refs
 * as an escape and kills the candidate.  `p.z += p.x` alone is enough to lose a
 * whole struct.
 *
 * This pass keys on the frame range instead of on the node shape.  Members come
 * from the type, so the map is complete rather than whatever the function
 * happened to spell; every Ref landing in [base, base+size) must resolve to
 * exactly one member start with a size that fits, and anything else -- a
 * whole-struct read, an interior address, a straddling access -- kills the
 * candidate.  Survivors get one fresh frame slot per member, so the members
 * stop being one object to promotion, to the slice live-in collector and to
 * every later pass.
 */
static int ast_sroa_run(AstArena *a) { MCC_TRACE("enter\n");
	AstSroaCand cand[AST_SROA_CAND];
	AstLocal nn = ast_count(a);
	int ncand = 0, i, j;
	ast_sroa_folds = 0;
	if (!a || ast_func_has_asm || ast_func_has_labeladdr)
		{ MCC_TRACE("br\n"); return 0; }
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (!ast_sra_struct_ref(a, n))
			{ MCC_TRACE("br\n"); continue; }
		if (ast_sroa_find(cand, ncand, ast_ival(a, n)))
			{ MCC_TRACE("br\n"); continue; }
		if (ncand >= AST_SROA_CAND)
			{ MCC_TRACE("br\n"); continue; }
		/* Incoming parameters live above the frame pointer and their bytes are
		 * written by the calling convention, not by any node in this arena.
		 * Moving a member of one to a fresh slot reads whatever the slot happened
		 * to hold -- `take_by_value(Section s) { return s.value; }` returns
		 * garbage.  A compiler-allocated local is below the frame pointer and has
		 * no writer this pass cannot see. */
		/* An incoming parameter's bytes are written by the calling convention
		 * before any statement in this arena runs, so its members cannot simply be
		 * re-addressed -- but they can be *copied out* once, at the head of the
		 * entry block, and every later access redirected to the copies.  That is
		 * the only place the ABI and the decomposition have to agree. */
		if ((int64_t)ast_ival(a, n) >= 0 && !ast_sroa_param_env)
			{ MCC_TRACE("br\n"); ast_sroa_note(AST_SROA_W_PARAM); continue; }
		{
			CType ck;
			ck.t = ast_type_t(a, n);
			ck.bp = 0;
			ck.bs = 0;
			ck.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			if (is_complex_type(&ck))
				{ MCC_TRACE("br\n"); ast_sroa_note(AST_SROA_W_CPLX); continue; }
		}
		cand[ncand].base = ast_ival(a, n);
		cand[ncand].ok = 1;
		cand[ncand].isparam = (int64_t)ast_ival(a, n) >= 0;
		if (!ast_sroa_members(a, n, &cand[ncand]))
			{ MCC_TRACE("br\n"); ast_sroa_note(AST_SROA_W_MEMBERS); continue; }
		ncand++;
	}
	if (!ncand)
		{ MCC_TRACE("br\n"); return 0; }
	/* One pass over every local Ref.  A ref at the base is legal only as the
	 * subject of a member access; a ref inside the range is legal only when it
	 * lands on a member start and fits.  Everything else is an observation of
	 * the object as an object, which a decomposition cannot preserve. */
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstSroaCand *c;
		int32_t d = 0;
		int t, rsz;
		if (!ast_sra_ref_local(a, n))
			{ MCC_TRACE("br\n"); continue; }
		c = ast_sroa_covering(cand, ncand, (int64_t)ast_ival(a, n), &d);
		if (!c || !c->ok)
			{ MCC_TRACE("br\n"); continue; }
		t = ast_type_t(a, n);
		{
			/* &s and &s.m hand the object's address to something this pass cannot
			 * follow, and the node carrying it is a pointer-typed Ref that would
			 * otherwise pass for an access to whatever member sits at that offset:
			 * `testc(&s, 10)` on `struct { double average; int count; }` reads as
			 * an 8-byte access to member 0. */
			AstLocal p = ast_parent(a, n);
			if (p != AST_NONE && ast_kind(a, p) == AST_Unary &&
					(ast_op(a, p) == AST_OP_ADDR || ast_op(a, p) == AST_OP_VLA))
				{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_ADDR); continue; }
			if (!(ast_op(a, n) & VT_LVAL))
				{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_ADDR); continue; }
		}
		if ((t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			AstLocal p = ast_parent(a, n);
			int32_t mo;
			if (d != 0 || p == AST_NONE || ast_kind(a, p) != AST_Unary ||
					ast_op(a, p) != AST_OP_MEMBER || ast_first_child(a, p) != n)
				{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_WHOLE); continue; }
			/* `&s.m` hands the member's address to something this pass cannot
			 * follow (e.g. bar(&f.p) reinterpreting the struct through a pointer);
			 * the ADDR sits above the MEMBER node, so the &s guard above (which
			 * only sees n's direct parent) misses it. Reject the whole candidate,
			 * as for &s. */
			{
				AstLocal pp = ast_parent(a, p);
				if (pp != AST_NONE && ast_kind(a, pp) == AST_Unary &&
						(ast_op(a, pp) == AST_OP_ADDR || ast_op(a, pp) == AST_OP_VLA))
					{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_ADDR); continue; }
			}
			mo = (int32_t)ast_ival(a, p);
			j = ast_sroa_member_covering(c, mo,
																	 ast_sra_scalar_size(ast_type_t(a, p)));
			if (j < 0)
				{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_STRADDLE); continue; }
			if (c->movable[j] &&
					(c->moff[j] != mo || ast_type_bp(a, p) || ast_type_bs(a, p) ||
					 ast_sra_scalar_size(ast_type_t(a, p)) != c->msz[j] ||
					 (ast_type_t(a, p) & VT_VOLATILE)))
				{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_STRADDLE); continue; }
			if (!c->movable[j])
				{ MCC_TRACE("br\n"); continue; }
			c->used[j] = 1;
			if (ast_sroa_is_store_dest(a, p))
				{ MCC_TRACE("br\n"); c->stored[j] = 1; }
			continue;
		}
		rsz = ast_sra_scalar_size(t);
		j = ast_sroa_member_covering(c, d, rsz);
		if (j < 0)
			{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_STRADDLE); continue; }
		if (!c->movable[j])
			{ MCC_TRACE("br\n"); continue; }
		if (c->moff[j] != d || rsz <= 0 || rsz > c->msz[j] || (t & VT_VOLATILE))
			{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_STRADDLE); continue; }
		/* Same size is not the same thing: a pointer-typed access at a double
		 * member, or an integer one at a float member, is not an access to that
		 * member and the two do not even live in the same register file. */
		if (is_float(t) != is_float(c->mtype[j]) ||
				((t & VT_BTYPE) == VT_PTR) != ((c->mtype[j] & VT_BTYPE) == VT_PTR))
			{ MCC_TRACE("br\n"); c->ok = 0; ast_sroa_note(AST_SROA_W_CLASS); continue; }
		c->used[j] = 1;
		if (ast_sroa_is_store_dest(a, n))
			{ MCC_TRACE("br\n"); c->stored[j] = 1; }
	}
	for (i = 0; i < ncand; i++) { MCC_TRACE("br\n");
		if (!cand[i].ok)
			{ MCC_TRACE("br\n"); continue; }
		cand[i].nused = 0;
		for (j = 0; j < cand[i].nmem; j++) { MCC_TRACE("br\n");
			if (!cand[i].used[j])
				{ MCC_TRACE("br\n"); continue; }
			/* A member that is read but never stored in this arena got its value
			 * from something the arena does not show -- the block clear an
			 * `= {0}` or a designated initializer leaves behind for the members it
			 * does not name.  Moving that member reads a slot nobody wrote. */
			if (!cand[i].stored[j] && !cand[i].isparam)
				{ MCC_TRACE("br\n"); cand[i].ok = 0; ast_sroa_note(AST_SROA_W_NOSTORE); break; }
			cand[i].nused++;
		}
		if (!cand[i].nused)
			{ MCC_TRACE("br\n"); if (cand[i].ok) ast_sroa_note(AST_SROA_W_DEAD); cand[i].ok = 0; }
	}
	/* Mint every slot before rewriting anything: a struct that runs the
	 * allocator out half way through would be left with some members moved and
	 * some not, which is a miscompile rather than a missed optimization. */
	for (i = 0; i < ncand; i++) { MCC_TRACE("br\n");
		if (!cand[i].ok)
			{ MCC_TRACE("br\n"); continue; }
		for (j = 0; j < cand[i].nmem; j++) { MCC_TRACE("br\n");
			int off;
			if (!cand[i].used[j] || !cand[i].movable[j])
				{ MCC_TRACE("br\n"); continue; }
			off = ast_ltemp_mint(cand[i].msz[j], cand[i].msz[j]);
			if (!off) { MCC_TRACE("br\n"); cand[i].ok = 0; ast_sroa_note(AST_SROA_W_SLOTS); break; }
			cand[i].slot[j] = off;
		}
	}
	/* Every scatter goes in before any of them, and in declaration order, so the
	 * copies are established before the first statement that can read one. */
	for (i = ncand - 1; i >= 0; i--) { MCC_TRACE("br\n");
		if (!cand[i].ok || !cand[i].isparam)
			{ MCC_TRACE("br\n"); continue; }
		for (j = cand[i].nmem - 1; j >= 0; j--) { MCC_TRACE("br\n");
			if (!cand[i].slot[j])
				{ MCC_TRACE("br\n"); continue; }
			if (!ast_sroa_scatter(a, &cand[i], j))
				{ MCC_TRACE("br\n"); cand[i].ok = 0; break; }
		}
	}
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		AstSroaCand *c;
		int32_t d = 0;
		int t;
		if (!ast_sra_ref_local(a, n))
			{ MCC_TRACE("br\n"); continue; }
		c = ast_sroa_covering(cand, ncand, (int64_t)ast_ival(a, n), &d);
		if (!c || !c->ok)
			{ MCC_TRACE("br\n"); continue; }
		t = ast_type_t(a, n);
		if ((t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			AstLocal p = ast_parent(a, n);
			int32_t mo = (int32_t)ast_ival(a, p);
			j = ast_sroa_member_covering(c, mo,
																	 ast_sra_scalar_size(ast_type_t(a, p)));
			if (j < 0 || !c->slot[j])
				{ MCC_TRACE("br\n"); continue; }
			ast_clear_children(a, p);
			a->kind[p] = AST_Ref;
			ast_set_op(a, p, VT_LOCAL | VT_LVAL);
			ast_set_ival(a, p, (uint64_t)(int64_t)c->slot[j]);
			ast_set_type(a, p, c->mtype[j], c->mref[j]);
			ast_set_sym(a, p, 0);
			ast_set_fbits(a, p, 0);
			ast_sroa_folds++;
			continue;
		}
		j = ast_sroa_member_covering(c, d, ast_sra_scalar_size(t));
		if (j < 0 || !c->slot[j])
			{ MCC_TRACE("br\n"); continue; }
		ast_set_ival(a, n, (uint64_t)(int64_t)c->slot[j]);
		ast_set_sym(a, n, 0);
		ast_sroa_folds++;
	}
	for (i = 0; i < ncand; i++)
		{ MCC_TRACE("br\n"); if (cand[i].ok) { MCC_TRACE("br\n"); ast_sroa_note(AST_SROA_W_OK); } }
	if (ast_sroa_folds)
		{ MCC_TRACE("br\n"); a->epoch++; }
	return ast_sroa_folds;
}

static int sg_templates(void) { MCC_TRACE("enter\n"); return ast_templates_env; }
static int sg_narrow(void) { MCC_TRACE("enter\n"); return ast_narrow_env; }
static int sg_ltemp(void) { MCC_TRACE("enter\n"); return ast_licm_temp_env; }
static int sg_ivsr(void) { MCC_TRACE("enter\n"); return ast_ivsr_env; }
static int sg_pre(void) { MCC_TRACE("enter\n"); return ast_pre_env; }
static int sg_bitflag(void) { MCC_TRACE("enter\n"); return ast_bitflag_env; }
static int sg_range(void) { MCC_TRACE("enter\n"); return ast_range_env; }
static int sg_divmagic(void) { MCC_TRACE("enter\n"); return ast_divmagic_env; }
static int sg_divrem(void) { MCC_TRACE("enter\n"); return ast_divrem_env; }
static int sg_abs(void) { MCC_TRACE("enter\n"); return ast_abs_env; }
static int sg_select(void) { MCC_TRACE("enter\n"); return ast_select_env; }
static int sg_reassoc(void) { MCC_TRACE("enter\n"); return ast_reassoc_env; }
static int sg_sethi(void) { MCC_TRACE("enter\n"); return ast_sethi_env; }
static int sg_inline(void) { MCC_TRACE("enter\n"); return ast_inline_pass_env; }
static int sg_sra(void) { MCC_TRACE("enter\n"); return ast_sra_env; }
static int sg_sroa(void) { MCC_TRACE("enter\n"); return ast_sroa_env; }

static int ast_strat_sra(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_sra_run(a, 0); }
static int ast_strat_sroa(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_sroa_run(a); }
static int ast_strat_bfold(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_bfold_run(a); }
static int ast_strat_ident(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_ident_run(a); }
static int ast_strat_narrow(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_narrow_run(a); }
static int ast_strat_cprop(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_cprop_run(a); }
static int ast_strat_cse(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_cse_run(a); }
static int ast_strat_ltemp(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_ltemp_run(a); }
static int ast_strat_ivsr(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s;
	int d = ast_ivsr_run(a);
	if (ast_ivsr_ptr_env) { MCC_TRACE("br\n"); d += ast_ivsr_ptr_run(a); }
	return d;
}
static int ast_strat_pre(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_pre_run(a); }
static int ast_strat_licm(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)a; (void)s; return ast_licm_folds; }
static int ast_strat_dse(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_dse_run(a); }
static int ast_strat_sccp(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_sccp_run(a); }
static int ast_strat_jt(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_jt_run(a); }
static int ast_strat_bf(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_bf_run(a); }
static int ast_strat_range(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_range_run(a); }
static int ast_strat_divmagic(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_divmagic_run(a); }
static int ast_strat_divrem(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_divrem_run(a); }
static int ast_strat_abs(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_abs_run(a); }
static int ast_strat_select(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_select_run(a); }
static int ast_strat_reassoc(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_reassoc_run(a); }
static int ast_strat_sethi(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_sethi_run(a); }
static int ast_strat_tco(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); return ast_tco_run(a, s); }
static int ast_strat_inline(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_inline_run(a); }
static int ast_strat_cload(AstArena *a, Sym *s) { MCC_TRACE("enter\n"); (void)s; return ast_cload_run(a); }

enum {
	AST_STRAT_BFOLD,
	AST_STRAT_IDENT,
	AST_STRAT_NARROW,
	AST_STRAT_CPROP,
	AST_STRAT_CSE,
	AST_STRAT_LTEMP,
	AST_STRAT_IVSR,
	AST_STRAT_PRE,
	AST_STRAT_LICM,
	AST_STRAT_DSE,
	AST_STRAT_SCCP,
	AST_STRAT_JT,
	AST_STRAT_BF,
	AST_STRAT_RANGE,
	AST_STRAT_DIVMAGIC,
	AST_STRAT_ABS,
	AST_STRAT_SELECT,
	AST_STRAT_REASSOC,
	AST_STRAT_SETHI,
	AST_STRAT_TCO,
	AST_STRAT_INLINE,
	AST_STRAT_CLOAD,
	AST_STRAT_SRA,
	AST_STRAT_SROA,
	AST_STRAT_DIVREM,
	AST_STRAT_COUNT
};
typedef char ast_strat_count_fits[AST_STRAT_COUNT <= AST_STRAT_COUNT_MAX ? 1 : -1];

static const AstStrategy ast_strategies[AST_STRAT_COUNT] = {
	{"bfold", sg_templates, ast_strat_bfold},
	{"ident", sg_templates, ast_strat_ident},
	{"narrow", sg_narrow, ast_strat_narrow},
	{"cprop", sg_templates, ast_strat_cprop},
	{"cse", sg_templates, ast_strat_cse},
	{"ltemp", sg_ltemp, ast_strat_ltemp},
	{"ivsr", sg_ivsr, ast_strat_ivsr},
	{"pre", sg_pre, ast_strat_pre},
	{"licm", sg_templates, ast_strat_licm},
	{"dse", sg_templates, ast_strat_dse},
	{"sccp", sg_templates, ast_strat_sccp},
	{"jt", sg_templates, ast_strat_jt},
	{"bf", sg_bitflag, ast_strat_bf},
	{"range", sg_range, ast_strat_range},
	{"divmagic", sg_divmagic, ast_strat_divmagic},
	{"abs", sg_abs, ast_strat_abs},
	{"select", sg_select, ast_strat_select},
	{"reassoc", sg_reassoc, ast_strat_reassoc},
	{"sethi", sg_sethi, ast_strat_sethi},
	{"tco", sg_templates, ast_strat_tco},
	{"inline", sg_inline, ast_strat_inline},
	{"cload", sg_templates, ast_strat_cload},
	{"sra", sg_sra, ast_strat_sra},
	{"sroa", sg_sroa, ast_strat_sroa},
	{"divrem", sg_divrem, ast_strat_divrem},
};

static uint32_t ast_strat_admit = 0xffffffffu;

#define AST_STRAT_BIT(x) ((uint32_t)1 << (x))

static long ast_run_strat_seq(AstArena *a, Sym *sym, int faithful,
															const int *seq, int k, int *sf) { MCC_TRACE("enter\n");
	long hits = 0;
	int oi;
	for (oi = 0; oi < k; oi++) { MCC_TRACE("br\n");
		int si = seq[oi];
		if (si < 0 || si >= AST_STRAT_COUNT)
			{ MCC_TRACE("br\n"); continue; }
		if (!((ast_strat_admit >> si) & 1))
			{ MCC_TRACE("br\n"); continue; }
		if (faithful && ast_strategies[si].gate()) { MCC_TRACE("br\n");
			int h = ast_strategies[si].apply(a, sym);
			hits += h;
			if (sf)
				{ MCC_TRACE("br\n"); sf[si] += h; }
		}
	}
	return hits;
}

static long ast_run_strat_cycle(AstArena *a, Sym *sym, int faithful,
																const int *seq, int k, int *sf) { MCC_TRACE("enter\n");
	long total = 0, chits;
	int citer = 0;
	do { MCC_TRACE("br\n");
		int stat_before[AST_STRAT_COUNT];
		int need_stats = sf && mcc_stats_mask;
		if (need_stats) { MCC_TRACE("br\n");
			for (int si = 0; si < AST_STRAT_COUNT; si++)
				{ MCC_TRACE("br\n"); stat_before[si] = sf[si]; }
		}
		chits = ast_run_strat_seq(a, sym, faithful, seq, k, sf);
		total += chits;
		citer++;
		if (need_stats) { MCC_TRACE("br\n");
			int stat_delta[AST_STRAT_COUNT];
			for (int si = 0; si < AST_STRAT_COUNT; si++)
				{ MCC_TRACE("br\n"); stat_delta[si] = sf[si] - stat_before[si]; }
			mcc_stats_fold_cycle(stat_delta, AST_STRAT_COUNT, citer);
		}
		MCC_TRACE("cycle iter=%d hits=%ld\n", citer, chits);
	} while (chits > 0 && citer < AST_CYCLE_MAX && ast_cycle_env);
	if (ast_cycle_env && citer >= AST_CYCLE_MAX && chits > 0)
		{ MCC_TRACE("br\n"); MCC_TRACE("cycle CAP hit iter=%d\n", citer); }
	return total;
}

#include "mccgate.h"

typedef struct AstSearchMemo {
	uint64_t hash;
	AstGateMask gates;
	unsigned refcount;
	int64_t score;
	uint64_t tried;
	uint64_t order_packed;
	uint64_t order_n;
} AstSearchMemo;

#define AST_ORDER_BITS 5
#define AST_ORDER_MASK ((1u << AST_ORDER_BITS) - 1u)
#define AST_ORDER_MAXN (64 / AST_ORDER_BITS)
typedef char ast_order_bits_fit[AST_STRAT_COUNT <= (int)AST_ORDER_MASK + 1 ? 1 : -1];

static uint64_t ast_order_pack(const int *seq, int n) { MCC_TRACE("enter\n");
	uint64_t p = 0;
	int i;
	if (n > AST_ORDER_MAXN)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); p |= (uint64_t)(seq[i] & AST_ORDER_MASK) << (i * AST_ORDER_BITS); }
	return p;
}

static void ast_order_unpack(uint64_t p, int n, int *seq) { MCC_TRACE("enter\n");
	int i;
	if (n > AST_ORDER_MAXN)
		{ MCC_TRACE("br\n"); n = AST_ORDER_MAXN; }
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); seq[i] = (int)((p >> (i * AST_ORDER_BITS)) & AST_ORDER_MASK); }
}

#define AST_SEARCH_MEMO_CAP 4096
#define AST_SEARCH_MAX_CAND 128
#define AST_SEARCH_CAND_MAX 64
static AstSearchMemo ast_search_memo[AST_SEARCH_MEMO_CAP];
static int ast_search_memo_n;

#define AST_SEARCH_MEMO_MAGIC 0x4647u
#define AST_GATE_BITS 48
#define AST_GATE_DISK_MASK ((uint64_t)(((uint64_t)1 << AST_GATE_BITS) - 1))
#define AST_SEARCH_DISK_MAX (10ull << 30)

static void ast_isa_key_update(const AstArena *a) { MCC_TRACE("enter\n");
	ast_isa_key_term = 0;
#ifdef MCC_TARGET_X86_64
	{
		AstLocal n;
		int nfn = (int)(sizeof ast_bfold_tab / sizeof *ast_bfold_tab);
		if (!a || !mcc_state || !mcc_isa_has(mcc_state, MCC_ISA_SSE41))
			{ MCC_TRACE("br\n"); return; }
		for (n = 0; n < (AstLocal)a->count; n++) { MCC_TRACE("br\n");
			AstLocal c;
			const char *nm;
			int bi;
			if (ast_kind(a, n) != AST_Invoke)
				{ MCC_TRACE("br\n"); continue; }
			c = ast_first_child(a, n);
			if (c == AST_NONE || ast_kind(a, c) != AST_Ref ||
					!(ast_op(a, c) & VT_SYM) || !a->sym[c])
				{ MCC_TRACE("br\n"); continue; }
			nm = get_tok_str(((Sym *)(uintptr_t)a->sym[c])->v, NULL);
			if (!nm)
				{ MCC_TRACE("br\n"); continue; }
			for (bi = 0; bi < nfn; bi++) { MCC_TRACE("br\n");
				if (strcmp(nm, ast_bfold_tab[bi].name))
					{ MCC_TRACE("br\n"); continue; }
				switch (ast_bfold_tab[bi].id) { MCC_TRACE("br\n");
				case 2: case 3: case 4: case 8: case 9: case 10:
					ast_isa_key_term |= MCC_ISA_SSE41;
					break;
				default:
					break;
				}
				break;
			}
			if (ast_isa_key_term)
				{ MCC_TRACE("br\n"); return; }
		}
	}
#else
	(void)a;
#endif
}

static uint64_t ast_search_key_salt_ex(uint64_t h, int per_triple) { MCC_TRACE("enter\n");
	const char *s;
	(void)s;
#ifdef MCC_VERSION_STR
	for (s = MCC_VERSION_STR; *s; s++)
		{ MCC_TRACE("br\n"); h = (h ^ (unsigned char)*s) * 0x100000001b3ull; }
#endif
#ifdef MCC_CONFIG_TRIPLET
	if (per_triple) { MCC_TRACE("br\n");
		for (s = MCC_CONFIG_TRIPLET; *s; s++)
			{ MCC_TRACE("br\n"); h = (h ^ (unsigned char)*s) * 0x100000001b3ull; }
	}
#endif
	if (ast_isa_key_term) { MCC_TRACE("br\n");
		h = (h ^ (uint64_t)ast_isa_key_term) * 0x100000001b3ull;
	}
	return h;
}

static const char *const ast_search_axis_env[] = {
		"MCC_AST_TEMPLATES",   "MCC_AST_PROMOTE",		 "MCC_AST_INLINE",
		"MCC_AST_NO_CALLFUL",  "MCC_AST_INLINE_LIMIT", "MCC_AST_INLINE_NODES",
		"MCC_AST_GRAFT",			 "MCC_AST_BITFLAG",			 "MCC_AST_CPROP_JOIN",
		"MCC_AST_CSE_JOIN",		 "MCC_AST_PROMOTE_LIMIT", "MCC_AST_OPT_LIMIT",
		"MCC_AST_FN_CONFIG"};

static uint64_t ast_search_axis_salt(uint64_t h) { MCC_TRACE("enter\n");
	unsigned i;
	for (i = 0; i < sizeof ast_search_axis_env / sizeof *ast_search_axis_env; i++) { MCC_TRACE("br\n");
		const char *v = getenv(ast_search_axis_env[i]);
		const char *s;
		h = (h ^ (uint64_t)(i + 1)) * 0x100000001b3ull;
		for (s = v ? v : ""; *s; s++)
			{ MCC_TRACE("br\n"); h = (h ^ (unsigned char)*s) * 0x100000001b3ull; }
	}
	return h;
}

static uint64_t ast_search_key_salt(uint64_t h) { MCC_TRACE("enter\n");
	return ast_search_axis_salt(ast_search_key_salt_ex(h, 1));
}

static uint64_t ast_slice_key_salt(uint64_t h) { MCC_TRACE("enter\n");
	return ast_search_key_salt_ex(h, 0);
}

static int ast_search_cache_dir(char *buf, int cap) { MCC_TRACE("enter\n");
	return host_cache_dir(buf, cap);
}

static int ast_search_disk_path(char *buf, int cap) { MCC_TRACE("enter\n");
	char dir[1024];
	if (ast_search_cache_dir(dir, sizeof dir) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (snprintf(buf, cap, "%s/mcc-search.memo", dir) >= cap)
		{ MCC_TRACE("br\n"); return -1; }
	return 0;
}

#if MCC_HOST_POSIX
#include <dirent.h>
#include <sys/stat.h>
static unsigned long long ast_search_disk_usage(void) { MCC_TRACE("enter\n");
	char dir[1024], path[1152];
	DIR *d;
	struct dirent *e;
	struct stat st;
	unsigned long long total = 0;
	if (ast_search_cache_dir(dir, sizeof dir) != 0)
		{ MCC_TRACE("br\n"); return 0; }
	d = opendir(dir);
	if (!d)
		{ MCC_TRACE("br\n"); return 0; }
	while ((e = readdir(d)) != NULL) { MCC_TRACE("br\n");
		if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path)
			{ MCC_TRACE("br\n"); continue; }
		if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
			{ MCC_TRACE("br\n"); total += (unsigned long long)st.st_size; }
	}
	closedir(d);
	return total;
}
#else
static unsigned long long ast_search_disk_usage(void) { MCC_TRACE("enter\n");
	char path[1152];
	FILE *f;
	long sz;
	if (ast_search_disk_path(path, sizeof path) != 0)
		{ MCC_TRACE("br\n"); return 0; }
	f = fopen(path, "rb");
	if (!f)
		{ MCC_TRACE("br\n"); return 0; }
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fclose(f);
	return sz > 0 ? (unsigned long long)sz : 0;
}
#endif

static int ast_search_memo_add(uint64_t h, AstGateMask gates, unsigned refcount,
															 int64_t score, uint64_t tried,
															 uint64_t order_packed, uint64_t order_n) { MCC_TRACE("enter\n");
	int i, changed = 0;
	for (i = 0; i < ast_search_memo_n; i++)
		{ MCC_TRACE("br\n"); if (ast_search_memo[i].hash == h) { MCC_TRACE("br\n");
			if (ast_search_memo[i].gates != gates) { MCC_TRACE("br\n");
				ast_search_memo[i].gates = gates;
				changed = 1;
			}
			if (refcount > ast_search_memo[i].refcount) { MCC_TRACE("br\n");
				ast_search_memo[i].refcount = refcount;
				changed = 1;
			}
			if (score >= 0 && score != ast_search_memo[i].score) { MCC_TRACE("br\n");
				ast_search_memo[i].score = score;
				changed = 1;
			}
			if ((tried | ast_search_memo[i].tried) != ast_search_memo[i].tried) { MCC_TRACE("br\n");
				ast_search_memo[i].tried |= tried;
				changed = 1;
			}
			if (order_n > 0 && (ast_search_memo[i].order_packed != order_packed ||
													ast_search_memo[i].order_n != order_n)) { MCC_TRACE("br\n");
				ast_search_memo[i].order_packed = order_packed;
				ast_search_memo[i].order_n = order_n;
				changed = 1;
			}
			return changed;
		} }
	if (ast_search_memo_n < AST_SEARCH_MEMO_CAP) { MCC_TRACE("br\n");
		ast_search_memo[ast_search_memo_n].hash = h;
		ast_search_memo[ast_search_memo_n].gates = gates;
		ast_search_memo[ast_search_memo_n].refcount = refcount;
		ast_search_memo[ast_search_memo_n].score = score;
		ast_search_memo[ast_search_memo_n].tried = tried;
		ast_search_memo[ast_search_memo_n].order_packed = order_packed;
		ast_search_memo[ast_search_memo_n].order_n = order_n;
		ast_search_memo_n++;
		changed = 1;
	}
	return changed;
}

#define AST_MEMO_CONT_MAGIC 0x315a534dUL
#define AST_MEMO_RECWORDS 7
#define AST_MEMO_RECBYTES (AST_MEMO_RECWORDS * 8)
#define AST_MEMO_RAWMAX (AST_SEARCH_MEMO_CAP * AST_MEMO_RECBYTES)
static unsigned char ast_memo_raw[AST_MEMO_RAWMAX];
static unsigned char ast_memo_pk[AST_MEMO_RAWMAX * 2 + 64];
static unsigned char ast_memo_try[AST_MEMO_RAWMAX * 2 + 64];

static int ast_memo_pack(long rn, long *plen) { MCC_TRACE("enter\n");
	long best = rn, l;
	int codec = 0;
	memcpy(ast_memo_pk, ast_memo_raw, (size_t)rn);
	l = rle_compress(ast_memo_raw, rn, ast_memo_try, (long)sizeof ast_memo_try);
	if (l >= 0 && l < best) { MCC_TRACE("br\n");
		best = l, codec = 1;
		memcpy(ast_memo_pk, ast_memo_try, (size_t)l);
	}
	l = lzss_compress(ast_memo_raw, rn, ast_memo_try, (long)sizeof ast_memo_try);
	if (l >= 0 && l < best) { MCC_TRACE("br\n");
		best = l, codec = 2;
		memcpy(ast_memo_pk, ast_memo_try, (size_t)l);
	}
	l = lzw_compress(ast_memo_raw, rn, ast_memo_try, (long)sizeof ast_memo_try);
	if (l >= 0 && l < best) { MCC_TRACE("br\n");
		best = l, codec = 3;
		memcpy(ast_memo_pk, ast_memo_try, (size_t)l);
	}
	*plen = best;
	return codec;
}

static long ast_memo_unpack(int codec, long clen) { MCC_TRACE("enter\n");
	switch (codec) { MCC_TRACE("br\n");
	case 0:
		if (clen > AST_MEMO_RAWMAX)
			{ MCC_TRACE("br\n"); return -1; }
		memcpy(ast_memo_raw, ast_memo_pk, (size_t)clen);
		return clen;
	case 1:
		return rle_decompress(ast_memo_pk, clen, ast_memo_raw, AST_MEMO_RAWMAX);
	case 2:
		return lzss_decompress(ast_memo_pk, clen, ast_memo_raw, AST_MEMO_RAWMAX);
	case 3:
		return lzw_decompress(ast_memo_pk, clen, ast_memo_raw, AST_MEMO_RAWMAX);
	}
	return -1;
}

static void ast_search_disk_rewrite(void) { MCC_TRACE("enter\n");
	char path[1152], tmp[1200];
	FILE *f;
	int i;
	long rn = 0, plen = 0;
	unsigned hdr[4];
	int codec;
	if (ast_search_disk_path(path, sizeof path) != 0)
		{ MCC_TRACE("br\n"); return; }
	if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp)
		{ MCC_TRACE("br\n"); return; }
	for (i = 0; i < ast_search_memo_n; i++) { MCC_TRACE("br\n");
		uint64_t rec[AST_MEMO_RECWORDS];
		rec[0] = ast_search_memo[i].hash;
		rec[1] = (ast_search_memo[i].gates & AST_GATE_DISK_MASK) |
						 ((uint64_t)AST_SEARCH_MEMO_MAGIC << AST_GATE_BITS);
		rec[2] = ast_search_memo[i].refcount;
		rec[3] = (uint64_t)ast_search_memo[i].score;
		rec[4] = ast_search_memo[i].tried;
		rec[5] = ast_search_memo[i].order_packed;
		rec[6] = ast_search_memo[i].order_n;
		memcpy(ast_memo_raw + rn, rec, sizeof rec);
		rn += (long)sizeof rec;
	}
	codec = ast_memo_pack(rn, &plen);
	f = fopen(tmp, "wb");
	if (!f)
		{ MCC_TRACE("br\n"); return; }
	hdr[0] = (unsigned)AST_MEMO_CONT_MAGIC;
	hdr[1] = (unsigned)codec;
	hdr[2] = (unsigned)rn;
	hdr[3] = (unsigned)plen;
	if (fwrite(hdr, sizeof hdr, 1, f) == 1 && plen > 0)
		{ MCC_TRACE("br\n"); fwrite(ast_memo_pk, 1, (size_t)plen, f); }
	fclose(f);
	if (rename(tmp, path) != 0)
		{ MCC_TRACE("br\n"); remove(tmp); }
}

static int ast_search_memo_cmp(const void *a, const void *b) { MCC_TRACE("enter\n");
	const AstSearchMemo *x = a, *y = b;
	if (x->refcount < y->refcount)
		{ MCC_TRACE("br\n"); return 1; }
	if (x->refcount > y->refcount)
		{ MCC_TRACE("br\n"); return -1; }
	return 0;
}

static void ast_search_disk_evict(void) { MCC_TRACE("enter\n");
	if (ast_search_memo_n < 4)
		{ MCC_TRACE("br\n"); return; }
	if (ast_search_disk_usage() < AST_SEARCH_DISK_MAX)
		{ MCC_TRACE("br\n"); return; }
	qsort(ast_search_memo, (size_t)ast_search_memo_n, sizeof ast_search_memo[0],
				ast_search_memo_cmp);
	MCC_TRACE("disk evict: usage=%lluMiB >= cap, dropping %d/%d lowest-refcount entries\n",
						ast_search_disk_usage() >> 20, ast_search_memo_n / 4, ast_search_memo_n);
	ast_search_memo_n -= ast_search_memo_n / 4;
	ast_search_disk_rewrite();
}

static void ast_search_disk_load(void) { MCC_TRACE("enter\n");
	char path[1152];
	FILE *f;
	unsigned hdr[4];
	long clen, rl, i;
	if (ast_search_disk_path(path, sizeof path) != 0)
		{ MCC_TRACE("br\n"); return; }
	f = fopen(path, "rb");
	if (!f)
		{ MCC_TRACE("br\n"); return; }
	if (fread(hdr, sizeof hdr, 1, f) != 1 || hdr[0] != (unsigned)AST_MEMO_CONT_MAGIC ||
			hdr[2] > (unsigned)AST_MEMO_RAWMAX || hdr[3] > (unsigned)sizeof ast_memo_pk) { MCC_TRACE("br\n");
		fclose(f);
		return;
	}
	clen = (long)hdr[3];
	if (clen > 0 && fread(ast_memo_pk, 1, (size_t)clen, f) != (size_t)clen) { MCC_TRACE("br\n");
		fclose(f);
		return;
	}
	fclose(f);
	rl = ast_memo_unpack((int)hdr[1], clen);
	if (rl != (long)hdr[2])
		{ MCC_TRACE("br\n"); return; }
	for (i = 0; i + AST_MEMO_RECBYTES <= rl && ast_search_memo_n < AST_SEARCH_MEMO_CAP;
			 i += AST_MEMO_RECBYTES) { MCC_TRACE("br\n");
		uint64_t rec[AST_MEMO_RECWORDS];
		memcpy(rec, ast_memo_raw + i, sizeof rec);
		if ((rec[1] >> AST_GATE_BITS) == AST_SEARCH_MEMO_MAGIC)
			{ MCC_TRACE("br\n"); ast_search_memo_add(rec[0], rec[1] & AST_GATE_DISK_MASK, (unsigned)rec[2],
													(int64_t)rec[3], rec[4], rec[5], rec[6]); }
	}
	MCC_TRACE("disk load: %s codec=%u raw=%ldB -> %d memo entries\n", path, hdr[1], rl,
						ast_search_memo_n);
	ast_search_disk_evict();
}

static void ast_search_disk_store(uint64_t h, AstGateMask gates, unsigned refcount,
																	int64_t score, uint64_t tried,
																	uint64_t order_packed, uint64_t order_n) { MCC_TRACE("enter\n");
	if (ast_search_memo_add(h, gates, refcount, score, tried, order_packed, order_n))
		{ MCC_TRACE("br\n"); ast_search_disk_rewrite(); }
	ast_search_disk_evict();
}

static AstSliceMemo ast_slice_disk[AST_SLICE_MEMO_CAP];
static int ast_slice_disk_n;
static int ast_slice_disk_loaded;
static int ast_slice_flush_armed;
static unsigned char ast_slice_io_raw[AST_SLICE_MEMO_CAP * AST_SLICE_RECBYTES];

static int ast_slice_disk_path(char *buf, int cap) { MCC_TRACE("enter\n");
	char dir[1024];
	uint64_t key = ast_slice_key_salt(0xcbf29ce484222325ULL);
	if (host_cache_dir(dir, sizeof dir) != 0)
		{ MCC_TRACE("br\n"); return -1; }
	if (snprintf(buf, cap, "%s/sl-%016llx.ck", dir, (unsigned long long)key) >= cap)
		{ MCC_TRACE("br\n"); return -1; }
	return 0;
}

static long ast_slice_disk_slurp(const char *path) { MCC_TRACE("enter\n");
	FILE *f = fopen(path, "rb");
	long len;
	if (!f)
		{ MCC_TRACE("br\n"); return 0; }
	fseek(f, 0, SEEK_END);
	len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len < 0)
		{ MCC_TRACE("br\n"); len = 0; }
	if (len > (long)sizeof ast_slice_io_raw)
		{ MCC_TRACE("br\n"); len = (long)sizeof ast_slice_io_raw; }
	if (len > 0 && fread(ast_slice_io_raw, 1, (size_t)len, f) != (size_t)len)
		{ MCC_TRACE("br\n"); len = 0; }
	fclose(f);
	return len;
}

static void ast_slice_disk_load(void) { MCC_TRACE("enter\n");
	char path[1152];
	long len;
	if (ast_slice_disk_loaded)
		{ MCC_TRACE("br\n"); return; }
	ast_slice_disk_loaded = 1;
	if (ast_slice_disk_path(path, sizeof path) != 0)
		{ MCC_TRACE("br\n"); return; }
	len = ast_slice_disk_slurp(path);
	ast_slice_disk_n =
			ast_slice_rec_deserialize(ast_slice_io_raw, len, ast_slice_disk, AST_SLICE_MEMO_CAP);
	MCC_TRACE("slice disk load: %s -> %d records\n", path, ast_slice_disk_n);
}

#if MCC_HOST_POSIX
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
static int ast_slice_lock(const char *path) { MCC_TRACE("enter\n");
	char lockp[1200];
	int fd;
	if (snprintf(lockp, sizeof lockp, "%s.lock", path) >= (int)sizeof lockp)
		{ MCC_TRACE("br\n"); return -1; }
	fd = open(lockp, O_CREAT | O_RDWR, 0644);
	if (fd >= 0)
		{ MCC_TRACE("br\n"); flock(fd, LOCK_EX); }
	return fd;
}
static void ast_slice_unlock(int fd) { MCC_TRACE("enter\n");
	if (fd >= 0) { MCC_TRACE("br\n");
		flock(fd, LOCK_UN);
		close(fd);
	}
}
#else
static int ast_slice_lock(const char *path) { MCC_TRACE("enter\n");
	(void)path;
	return -1;
}
static void ast_slice_unlock(int fd) { MCC_TRACE("enter\n"); (void)fd; }
#endif

static void ast_slice_disk_commit(const AstSliceMemo *recs, int n) { MCC_TRACE("enter\n");
	{ const char *d = getenv("MCC_SLICE_DUMP");
	  if (d && d[0]) { MCC_TRACE("br\n");
	    FILE *f = fopen(d, "a"); int q;
	    if (f) { MCC_TRACE("br\n");
	      for (q = 0; q < n; q++) { MCC_TRACE("br\n");
	               fprintf(f, "%016llx g=%016llx e=%016llx %lld %d\n",
	                       (unsigned long long)recs[q].ident,
	                       (unsigned long long)recs[q].gates,
	                       (unsigned long long)recs[q].eligible,
	                       (long long)recs[q].size, recs[q].proven); }
	      fclose(f); } } }
	char path[1152], tmpp[1200];
	int lockfd, i;
	FILE *f;
	long len, wl;
	static AstSliceMemo merged[AST_SLICE_MEMO_CAP];
	int merged_n = 0;
	if (!recs || n <= 0)
		{ MCC_TRACE("br\n"); return; }
	if (ast_slice_disk_path(path, sizeof path) != 0)
		{ MCC_TRACE("br\n"); return; }
	lockfd = ast_slice_lock(path);
	len = ast_slice_disk_slurp(path);
	merged_n = ast_slice_rec_deserialize(ast_slice_io_raw, len, merged, AST_SLICE_MEMO_CAP);
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); ast_slice_merge_one(merged, &merged_n, AST_SLICE_MEMO_CAP, &recs[i]); }
	wl = ast_slice_rec_serialize(merged, merged_n, ast_slice_io_raw,
															 (long)sizeof ast_slice_io_raw);
	if (wl > 0 && snprintf(tmpp, sizeof tmpp, "%s.tmp", path) < (int)sizeof tmpp &&
			(f = fopen(tmpp, "wb"))) { MCC_TRACE("br\n");
		int fd;
		fwrite(ast_slice_io_raw, 1, (size_t)wl, f);
		fflush(f);
#if MCC_HOST_POSIX
		if ((fd = fileno(f)) >= 0)
			{ MCC_TRACE("br\n"); fsync(fd); }
#else
		(void)fd;
#endif
		fclose(f);
		if (rename(tmpp, path) != 0)
			{ MCC_TRACE("br\n"); remove(tmpp); }
	}
	ast_slice_unlock(lockfd);
	MCC_TRACE("slice disk commit: %s <- %d local -> %d total\n", path, n, merged_n);
}

static void ast_slice_disk_merge_store(void) { MCC_TRACE("enter\n");
	if (ast_slice_memo_n <= 0)
		{ MCC_TRACE("br\n"); return; }
	ast_slice_disk_commit(ast_slice_memo, ast_slice_memo_n);
}

#if MCC_EMBED_JIT

void ast_slice_graduate(uint64_t ident, uint64_t gates, int64_t size) { MCC_TRACE("enter\n");
	AstSliceMemo rec;
	if (!ast_slice_env || !ident)
		{ MCC_TRACE("br\n"); return; }
	rec.ident = ident;
	rec.gates = gates;
	rec.size = size;
	rec.refcount = 1;
	rec.proven = 1;
	ast_slice_disk_commit(&rec, 1);
}

typedef struct AstSliceGradCtx {
	AstSliceMemo *tab;
	int n;
	int cap;
	uint64_t gates;
} AstSliceGradCtx;

static void ast_slice_visit_graduate(uint64_t ident, int size, uint64_t gates, void *ctx) { MCC_TRACE("enter\n");
	AstSliceGradCtx *g = (AstSliceGradCtx *)ctx;
	AstSliceMemo rec;
	(void)gates;
	rec.ident = ident;
	rec.gates = g->gates;
	rec.size = size;
	rec.refcount = 1;
	rec.proven = 1;
	ast_slice_merge_one(g->tab, &g->n, g->cap, &rec);
}

void ast_slice_graduate_arena(const AstArena *a, uint64_t gate_mask) { MCC_TRACE("enter\n");
	static AstSliceMemo grad[AST_SLICE_MEMO_CAP];
	AstSliceGradCtx g;
	if (!ast_slice_env || !a)
		{ MCC_TRACE("br\n"); return; }
	g.tab = grad;
	g.n = 0;
	g.cap = AST_SLICE_MEMO_CAP;
	g.gates = gate_mask;
	ast_slice_enum(a, gate_mask, ast_slice_visit_graduate, &g);
	if (g.n > 0)
		{ MCC_TRACE("br\n"); ast_slice_disk_commit(grad, g.n); }
	MCC_TRACE("slice graduate-arena: %d proven slices gates=%llx\n", g.n,
						(unsigned long long)gate_mask);
}

int ast_slice_enabled(void) { MCC_TRACE("enter\n");
	return ast_slice_env;
}
#endif

static void ast_slice_flush_atexit(void) { MCC_TRACE("enter\n");
	ast_slice_disk_merge_store();
}

#define AST_SEARCH_WIN 10
static int ast_search_started;
static unsigned ast_search_start_ms;
static unsigned ast_search_budget_ms;
static unsigned ast_search_durwin[AST_SEARCH_WIN];
static int ast_search_durwin_n;
static int ast_search_durwin_head;
static volatile int ast_search_abort;

static unsigned ast_now_ms(void) { MCC_TRACE("enter\n");
	return (unsigned)((unsigned long long)clock() * 1000ull / CLOCKS_PER_SEC);
}

static void ast_search_durwin_push(unsigned dt) { MCC_TRACE("enter\n");
	ast_search_durwin[ast_search_durwin_head] = dt;
	ast_search_durwin_head = (ast_search_durwin_head + 1) % AST_SEARCH_WIN;
	if (ast_search_durwin_n < AST_SEARCH_WIN)
		{ MCC_TRACE("br\n"); ast_search_durwin_n++; }
}

static unsigned ast_search_expect_ms(void) { MCC_TRACE("enter\n");
	double y[AST_SEARCH_WIN], pred;
	int n = ast_search_durwin_n, start, i;
	if (n <= 0)
		{ MCC_TRACE("br\n"); return 0; }
	start = (ast_search_durwin_head - n + AST_SEARCH_WIN * 2) % AST_SEARCH_WIN;
	for (i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); y[i] = (double)ast_search_durwin[(start + i) % AST_SEARCH_WIN]; }
	pred = ast_fc_forecast(y, n);
	if (pred < 0)
		{ MCC_TRACE("br\n"); pred = 0; }
	return (unsigned)(pred + 0.5);
}

static unsigned ast_search_remaining_ms(void) { MCC_TRACE("enter\n");
	unsigned el;
	if (!ast_search_budget_ms)
		{ MCC_TRACE("br\n"); return ~0u; }
	el = ast_now_ms() - ast_search_start_ms;
	return el >= ast_search_budget_ms ? 0 : ast_search_budget_ms - el;
}

static int ast_search_cap_fired;
static long ast_search_predict_hits;
static long ast_search_predict_tries;
static unsigned long ast_search_evals;
static unsigned long ast_search_eval_quota;
static int ast_search_quota_hit;

static void ast_search_evals_report(void) { MCC_TRACE("enter\n");
	fprintf(stderr, "[search] %lu candidate evaluations, quota %lu (%s)\n",
					ast_search_evals, ast_search_eval_quota,
					ast_search_quota_hit ? "spent" : "not reached");
	fprintf(stderr, "[search] predicted %ld candidate(s), %ld improved on the "
									"incumbent\n",
					ast_search_predict_tries, ast_search_predict_hits);
}

static int ast_search_should_stop(void) { MCC_TRACE("enter\n");
	if (ast_search_abort)
		{ MCC_TRACE("br\n"); return 1; }
	if (ast_search_eval_quota && ast_search_evals >= ast_search_eval_quota) { MCC_TRACE("br\n");
		if (!ast_search_quota_hit) { MCC_TRACE("br\n");
			ast_search_quota_hit = 1;
			if (ast_search_verbose_env)
				{ MCC_TRACE("br\n"); fprintf(stderr,
					"[search] tick quota spent: %lu candidate evaluations; every function "
					"after this one keeps its default gates. This bound is counted in "
					"work, not in time, so the object stays reproducible -- raise it with "
					"-fopt-search-ticks or MCC_SEARCH_TU_EVALS\n",
					ast_search_eval_quota); }
			MCC_TRACE("search quota %lu candidate evaluations spent\n",
								ast_search_eval_quota);
		}
		return 1;
	}
	if (!ast_search_budget_ms)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_search_remaining_ms())
		{ MCC_TRACE("br\n"); return 0; }
	if (!ast_search_cap_fired) { MCC_TRACE("br\n");
		ast_search_cap_fired = 1;
		mcc_search_cap_notice("ast-search", ast_now_ms() - ast_search_start_ms,
													ast_search_budget_ms);
	}
	return 1;
}

#define MCC_EFFECT_MALLOC mcc_malloc
#define MCC_EFFECT_REALLOC mcc_realloc
#define MCC_EFFECT_FREE mcc_free
#if defined(__has_include)
#if __has_include("ast_eval_slice.h")
#include "ast_eval_slice.h"
#endif
#endif
#ifndef AST_EVAL_SLICE_PROVIDED
static int ast_eval_slice(AstArena *a, AstLocal n, const int32_t *o, const int64_t *v,
													int c, int64_t *out) { MCC_TRACE("enter\n");
	(void)a;
	(void)n;
	(void)o;
	(void)v;
	(void)c;
	(void)out;
	return 1;
}
#endif

#define MCC_GPU_MALLOC mcc_malloc
#define MCC_GPU_REALLOC mcc_realloc
#define MCC_GPU_FREE mcc_free
#define MCC_GPU_ORACLE 1
#include "mccgpu.h"

#ifdef AST_EVAL_SLICE_PROVIDED
#include "slice_inline.h"

static AstArena *ast_slice_leaf_pool(AstArena *a, AstLocal inv, AstLocal *root,
																		 int32_t *poff, int *pnparam) { MCC_TRACE("enter\n");
	AstLocal cref = ast_child(a, inv, 0);
	struct AstInlineFn *e;
	void *cs;
	int i;
	if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
		{ MCC_TRACE("br\n"); return NULL; }
	cs = (void *)(uintptr_t)ast_sym(a, cref);
	if (!cs || (((Sym *)cs)->type.t & VT_BTYPE) != VT_FUNC)
		{ MCC_TRACE("br\n"); return NULL; }
	if (mcc_slice_inl_depth_max > 0 && cs == ast_slice_self_sym && ast_cur &&
			ast_cur != a && !ast_arena_has_hole(ast_cur)) { MCC_TRACE("br\n");
		*pnparam = 0;
		if (ast_inline_cap_ok && ast_inline_cap_np > 0 &&
				ast_inline_cap_np <= MCC_SLICE_INL_MAXPARAM) { MCC_TRACE("br\n");
			for (i = 0; i < ast_inline_cap_np; i++)
				{ MCC_TRACE("br\n"); poff[i] = (int32_t)ast_inline_cap_off[i]; }
			*pnparam = ast_inline_cap_np;
		}
		*root = ast_root(ast_cur);
		return ast_cur;
	}
	e = ast_inline_find(cs);
	if (!e || !e->ast || ast_arena_has_hole(e->ast))
		{ MCC_TRACE("br\n"); return NULL; }
	/* Hand back the incoming-parameter offsets in declaration order. The scanner
	 * cannot derive them: they are the target's frame layout, and mcc supports
	 * targets that put parameters above the frame pointer and targets that put
	 * them below it. */
	if (e->nparams > MCC_SLICE_INL_MAXPARAM)
		{ MCC_TRACE("br\n"); return NULL; }
	for (i = 0; i < e->nparams; i++)
		{ MCC_TRACE("br\n"); poff[i] = (int32_t)e->param_off[i]; }
	*pnparam = e->nparams;
	*root = ast_root(e->ast);
	return e->ast;
}

static void ast_slice_inl_report(void) { MCC_TRACE("enter\n");
	fprintf(stderr,
					"[slice-inline] invoke-seen=%ld invoke-inlined=%ld pool=%d "
					"depth-max=%d rec-grafts=%ld bailouts=%ld expand-nodes=%ld\n",
					mcc_slice_inl_seen, mcc_slice_inl_n, ast_inline_hi,
					mcc_slice_inl_depth_max, mcc_slice_inl_rec, mcc_slice_inl_bail,
					mcc_slice_inl_expand_tot);
}

static int ast_slice_inl_env = -1;
static int ast_slice_inl_cfg_done;

/* Reading the knobs must not install the hook or register the atexit report.
 * atexit handlers run in reverse registration order, and ast_ladder_gpu_report
 * tears the device down from one of them, so registering an extra handler at a
 * different moment reorders that teardown against the driver's own unload and
 * turns mcc_gpu_quiesce into a call through an unmapped page. Keep the
 * side-effecting arm on exactly the path that already reached it. */
static int ast_slice_inl_depth_cfg(void) { MCC_TRACE("enter\n");
	if (!ast_slice_inl_cfg_done) { MCC_TRACE("br\n");
		const char *d;
		ast_slice_inl_cfg_done = 1;
		d = getenv("MCC_SLICE_INL_DEPTH");
		if (d && d[0])
			{ MCC_TRACE("br\n"); mcc_slice_inl_depth_max = (int)strtol(d, NULL, 10); }
		if (mcc_slice_inl_depth_max < 0)
			{ MCC_TRACE("br\n"); mcc_slice_inl_depth_max = 0; }
		if (mcc_slice_inl_depth_max > MCC_SLICE_INL_DEPTH_CAP)
			{ MCC_TRACE("br\n"); mcc_slice_inl_depth_max = MCC_SLICE_INL_DEPTH_CAP; }
		d = getenv("MCC_SLICE_INL_EXPAND");
		if (d && d[0])
			{ MCC_TRACE("br\n"); mcc_slice_inl_expand_max = strtol(d, NULL, 10); }
		if (mcc_slice_inl_expand_max <= 0)
			{ MCC_TRACE("br\n"); mcc_slice_inl_expand_max = MCC_SLICE_INL_EXPAND; }
	}
	return mcc_slice_inl_depth_max;
}

static int ast_slice_inl_on(void) { MCC_TRACE("enter\n");
	if (ast_slice_inl_env < 0) { MCC_TRACE("br\n");
		ast_slice_inl_env = mcc_env_flag("MCC_AST_SLICE_INLINE", 1);
		mcc_slice_inl_dump = mcc_env_on("MCC_SLICE_INL_DUMP");
		ast_slice_inl_depth_cfg();
		if (ast_slice_inl_env) { MCC_TRACE("br\n");
			mcc_slice_leaf_hook = ast_slice_leaf_pool;
			atexit(ast_slice_inl_report);
		}
	}
	return ast_slice_inl_env;
}

static AstArena *ast_slice_leaf_inline(AstArena *a) { MCC_TRACE("enter\n");
	AstArena *g;
	if (!a || (!ast_inline_n && ast_slice_inl_depth_cfg() <= 0) ||
			!ast_slice_inl_on())
		{ MCC_TRACE("br\n"); return NULL; }
	g = ast_arena_clone(a);
	if (!g)
		{ MCC_TRACE("br\n"); return NULL; }
	mcc_slice_inline_arena(g);
	return g;
}
#else
static AstArena *ast_slice_leaf_inline(AstArena *a) { MCC_TRACE("enter\n");
	(void)a;
	return NULL;
}
#endif

#ifndef AST_LADDER_GPU_MAX
#define AST_LADDER_GPU_MAX (1u << 20)
#endif

static long ast_ladder_gpu_budget;
static long ast_ladder_gpu_rungs;

enum {
	AST_LGR_ARITY = 0, AST_LGR_BUDGET, AST_LGR_LIVEIN, AST_LGR_RESULT,
	AST_LGR_EMIT_LHS, AST_LGR_EMIT_RHS, AST_LGR_OOM, AST_LGR_DISPATCH,
	AST_LGR_N
};

static const char *const ast_ladder_gpu_reason[AST_LGR_N] = {
	"arity-or-space", "budget", "livein-type", "result-type",
	"emit-lhs", "emit-rhs", "host-oom", "dispatch"
};

static long ast_ladder_gpu_refused[AST_LGR_N];
static int ast_ladder_gpu_forced;

static int ast_ladder_gpu_refuse(int why) { MCC_TRACE("enter\n");
	ast_ladder_gpu_refused[why]++;
	return -1;
}

void ast_ladder_gpu_force(void) { MCC_TRACE("enter\n");
	ast_ladder_gpu_forced = 1;
	ast_eval_ladder_set(1);
}

static int64_t ast_ladder_gpu_word(const int32_t *p, uint64_t i) { MCC_TRACE("enter\n");
	uint64_t lo = (uint32_t)p[i];
	uint64_t hi = (uint32_t)p[i + 1];
	return (int64_t)(lo | (hi << 32));
}

static int ast_ladder_gpu_run(AstArena *a, AstLocal ar, AstArena *b, AstLocal br,
															const AstEvalLadderIn *in, int n, const int *e,
															const int *sh, int total, uint64_t space,
															AstEvalLadderRes *res) { MCC_TRACE("enter\n");
	int32_t off[AST_EVAL_LADDER_MAXIN];
	MccGpuCode ca = {NULL, 0}, cb = {NULL, 0};
	MccGpuStats gs;
	int i, verdict = 1;
	int rta, rtb;
	int32_t *tin = NULL, *oa = NULL, *ob = NULL;
	uint64_t code;
	int ntuple = (int)space;

	ast_ladder_gpu_rungs++;
	if (n < 0 || n > AST_EVAL_LADDER_MAXIN || space > AST_LADDER_GPU_MAX)
		return ast_ladder_gpu_refuse(AST_LGR_ARITY);
	mcc_gpu_stats(&gs);
	if (ast_ladder_gpu_budget && gs.dispatches >= ast_ladder_gpu_budget)
		return ast_ladder_gpu_refuse(AST_LGR_BUDGET);
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		if (is_float(in[i].type) || !ast_eval_slice_intt(in[i].type) ||
				in[i].bits > 64)
			{ MCC_TRACE("br\n"); return ast_ladder_gpu_refuse(AST_LGR_LIVEIN); }
		off[i] = in[i].off;
	}
	rta = ast_eval_slice_wtype(a, ar);
	rtb = ast_eval_slice_wtype(b, br);
	if (!rta || !rtb || is_float(rta) || is_float(rtb))
		return ast_ladder_gpu_refuse(AST_LGR_RESULT);
	if (!mcc_gpu_emit(a, ar, off, n, &ca))
		return ast_ladder_gpu_refuse(AST_LGR_EMIT_LHS);
	if (!mcc_gpu_emit(b, br, off, n, &cb)) { MCC_TRACE("br\n");
		mcc_gpu_code_free(&ca);
		return ast_ladder_gpu_refuse(AST_LGR_EMIT_RHS);
	}
	tin = mcc_malloc((size_t)ntuple * (n ? n : 1) * MCC_GPU_IN_SLOTS * 4);
	oa = mcc_malloc((size_t)ntuple * MCC_GPU_OUT_SLOTS * 4);
	ob = mcc_malloc((size_t)ntuple * MCC_GPU_OUT_SLOTS * 4);
	if (!tin || !oa || !ob) { MCC_TRACE("br\n");
		ast_ladder_gpu_refused[AST_LGR_OOM]++;
		goto bail;
	}
	for (code = 0; code < space; code++)
		for (i = 0; i < n; i++) { MCC_TRACE("br\n");
			int64_t tv = ast_eval_slice_fit(
					ast_eval_ladder_sx(code >> sh[i], e[i]), in[i].type);
			tin[(code * n + i) * MCC_GPU_IN_SLOTS] = (int32_t)(uint32_t)(uint64_t)tv;
			tin[(code * n + i) * MCC_GPU_IN_SLOTS + 1] =
					(int32_t)(uint32_t)((uint64_t)tv >> 32);
		}
	{
		const char *dd = getenv("MCC_LADDER_GPU_DUMP");
		if (dd) { MCC_TRACE("br\n");
			static int seq;
			char pth[512];
			snprintf(pth, sizeof pth, "%s/lad%04d_a." MCC_GPU_CODE_SUFFIX, dd, seq);
			mcc_gpu_code_dump(&ca, pth);
			snprintf(pth, sizeof pth, "%s/lad%04d_b." MCC_GPU_CODE_SUFFIX, dd, seq);
			mcc_gpu_code_dump(&cb, pth);
			seq++;
		}
	}
	if (!mcc_gpu_run2(&ca, &cb, tin, ntuple, n, oa, ob)) { MCC_TRACE("br\n");
		ast_ladder_gpu_refused[AST_LGR_DISPATCH]++;
		goto bail;
	}

	for (code = 0; code < space; code++) { MCC_TRACE("br\n");
		int adef = oa[code * MCC_GPU_OUT_SLOTS + 2] != 0;
		int bdef = ob[code * MCC_GPU_OUT_SLOTS + 2] != 0;
		int64_t fa, fb;
		res->points++;
		if (!adef) { MCC_TRACE("br\n");
			res->vacuous++;
			continue;
		}
		if (!bdef) { MCC_TRACE("br\n");
			for (i = 0; i < n && i < AST_EVAL_LADDER_MAXIN; i++)
				res->diff_in[i] = ast_ladder_gpu_word(tin, (code * n + i) * MCC_GPU_IN_SLOTS);
			res->diff_a = ast_eval_slice_fit(ast_ladder_gpu_word(oa, code * MCC_GPU_OUT_SLOTS), rta);
			res->diff_b = 0;
			res->diff_b_undef = 1;
			verdict = 0;
			goto out;
		}
		res->informative++;
		fa = ast_eval_slice_fit(ast_ladder_gpu_word(oa, code * MCC_GPU_OUT_SLOTS), rta);
		fb = ast_eval_slice_fit(ast_ladder_gpu_word(ob, code * MCC_GPU_OUT_SLOTS), rtb);
		if (fa != fb) { MCC_TRACE("br\n");
			for (i = 0; i < n && i < AST_EVAL_LADDER_MAXIN; i++)
				res->diff_in[i] = ast_ladder_gpu_word(tin, (code * n + i) * MCC_GPU_IN_SLOTS);
			res->diff_a = fa;
			res->diff_b = fb;
			res->diff_b_undef = 0;
			verdict = 0;
			goto out;
		}
	}
out:
	mcc_gpu_code_free(&ca);
	mcc_gpu_code_free(&cb);
	mcc_free(tin);
	mcc_free(oa);
	mcc_free(ob);
	return verdict;

bail:
	mcc_gpu_code_free(&ca);
	mcc_gpu_code_free(&cb);
	mcc_free(tin);
	mcc_free(oa);
	mcc_free(ob);
	return -1;
}

static int ast_ladder_gpu_run_tuples(AstArena *a, AstLocal ar, AstArena *b,
																		 AstLocal br, const AstEvalLadderIn *in,
																		 int n, const int64_t *vals, int ntuple,
																		 AstEvalLadderRes *res) { MCC_TRACE("enter\n");
	int32_t off[AST_EVAL_LADDER_MAXIN];
	MccGpuCode ca = {NULL, 0}, cb = {NULL, 0};
	MccGpuStats gs;
	int i, verdict = 1, code;
	int rta, rtb;
	int32_t *tin = NULL, *oa = NULL, *ob = NULL;

	ast_ladder_gpu_rungs++;
	if (n < 1 || n > AST_EVAL_LADDER_MAXIN || ntuple < 1 ||
			(uint64_t)ntuple > AST_LADDER_GPU_MAX)
		return ast_ladder_gpu_refuse(AST_LGR_ARITY);
	mcc_gpu_stats(&gs);
	if (ast_ladder_gpu_budget && gs.dispatches >= ast_ladder_gpu_budget)
		return ast_ladder_gpu_refuse(AST_LGR_BUDGET);
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		if (is_float(in[i].type) || !ast_eval_slice_intt(in[i].type) ||
				in[i].bits > 64)
			{ MCC_TRACE("br\n"); return ast_ladder_gpu_refuse(AST_LGR_LIVEIN); }
		off[i] = in[i].off;
	}
	rta = ast_eval_slice_wtype(a, ar);
	rtb = ast_eval_slice_wtype(b, br);
	if (!rta || !rtb || is_float(rta) || is_float(rtb))
		return ast_ladder_gpu_refuse(AST_LGR_RESULT);
	if (!mcc_gpu_emit(a, ar, off, n, &ca))
		return ast_ladder_gpu_refuse(AST_LGR_EMIT_LHS);
	if (!mcc_gpu_emit(b, br, off, n, &cb)) { MCC_TRACE("br\n");
		mcc_gpu_code_free(&ca);
		return ast_ladder_gpu_refuse(AST_LGR_EMIT_RHS);
	}
	tin = mcc_malloc((size_t)ntuple * n * MCC_GPU_IN_SLOTS * 4);
	oa = mcc_malloc((size_t)ntuple * MCC_GPU_OUT_SLOTS * 4);
	ob = mcc_malloc((size_t)ntuple * MCC_GPU_OUT_SLOTS * 4);
	if (!tin || !oa || !ob) { MCC_TRACE("br\n");
		ast_ladder_gpu_refused[AST_LGR_OOM]++;
		goto bail;
	}
	for (code = 0; code < ntuple; code++)
		for (i = 0; i < n; i++) { MCC_TRACE("br\n");
			int64_t tv = ast_eval_slice_fit(vals[code * n + i], in[i].type);
			tin[(code * n + i) * MCC_GPU_IN_SLOTS] = (int32_t)(uint32_t)(uint64_t)tv;
			tin[(code * n + i) * MCC_GPU_IN_SLOTS + 1] =
					(int32_t)(uint32_t)((uint64_t)tv >> 32);
		}
	if (!mcc_gpu_run2(&ca, &cb, tin, ntuple, n, oa, ob)) { MCC_TRACE("br\n");
		ast_ladder_gpu_refused[AST_LGR_DISPATCH]++;
		goto bail;
	}
	for (code = 0; code < ntuple; code++) { MCC_TRACE("br\n");
		int adef = oa[code * MCC_GPU_OUT_SLOTS + 2] != 0;
		int bdef = ob[code * MCC_GPU_OUT_SLOTS + 2] != 0;
		int64_t fa, fb;
		res->points++;
		if (!adef) { MCC_TRACE("br\n"); res->vacuous++; continue; }
		if (!bdef) { MCC_TRACE("br\n");
			for (i = 0; i < n && i < AST_EVAL_LADDER_MAXIN; i++)
				res->diff_in[i] = ast_ladder_gpu_word(tin, (code * n + i) * MCC_GPU_IN_SLOTS);
			res->diff_a = ast_eval_slice_fit(ast_ladder_gpu_word(oa, code * MCC_GPU_OUT_SLOTS), rta);
			res->diff_b = 0;
			res->diff_b_undef = 1;
			verdict = 0;
			goto out;
		}
		res->informative++;
		fa = ast_eval_slice_fit(ast_ladder_gpu_word(oa, code * MCC_GPU_OUT_SLOTS), rta);
		fb = ast_eval_slice_fit(ast_ladder_gpu_word(ob, code * MCC_GPU_OUT_SLOTS), rtb);
		if (fa != fb) { MCC_TRACE("br\n");
			for (i = 0; i < n && i < AST_EVAL_LADDER_MAXIN; i++)
				res->diff_in[i] = ast_ladder_gpu_word(tin, (code * n + i) * MCC_GPU_IN_SLOTS);
			res->diff_a = fa;
			res->diff_b = fb;
			res->diff_b_undef = 0;
			verdict = 0;
			goto out;
		}
	}
out:
	mcc_gpu_code_free(&ca);
	mcc_gpu_code_free(&cb);
	mcc_free(tin);
	mcc_free(oa);
	mcc_free(ob);
	return verdict;

bail:
	mcc_gpu_code_free(&ca);
	mcc_gpu_code_free(&cb);
	mcc_free(tin);
	mcc_free(oa);
	mcc_free(ob);
	return -1;
}

void ast_ladder_gpu_setup(void) { MCC_TRACE("enter\n");
	static int done;
	if (done)
		{ MCC_TRACE("br\n"); return; }
	/* Declining because nobody asked does NOT count as done. main() calls this
	 * through mcc_rt_enter() so the device comes up at the process's entry
	 * rather than in the middle of a compilation (src/mccrt.h); at that point
	 * argv has not been parsed, so a CLI-forced request has not arrived yet.
	 * Marking `done` on the way out of that first call would swallow the forced
	 * boot entirely. Only a request that was actually honoured is `done`. */
	if (!ast_ladder_gpu_forced && !mcc_env_on("MCC_AST_EVAL_LADDER_GPU"))
		{ MCC_TRACE("br\n"); return; }
	done = 1;
	ast_ladder_gpu_budget = mcc_env_num("MCC_AST_EVAL_LADDER_GPU_MAX", 0);
	ast_ladder_gpu_hook = ast_ladder_gpu_run;
	ast_ladder_gpu_tuples_hook = ast_ladder_gpu_run_tuples;
	{ MCC_TRACE("br\n");
		AstArena *pa = ast_arena_new();
		AstLocal r = ast_node(pa, AST_Literal);
		int32_t poff[1];
		MccGpuCode pcode = {NULL, 0};
		ast_set_op(pa, r, VT_CONST);
		ast_set_type(pa, r, VT_INT, 0);
		ast_set_ival(pa, r, 7);
		poff[0] = -8;
		if (mcc_gpu_emit(pa, r, poff, 1, &pcode)) { MCC_TRACE("br\n");
			int32_t pin[64 * MCC_GPU_IN_SLOTS], pout[64 * MCC_GPU_OUT_SLOTS];
			memset(pin, 0, sizeof pin);
			int wrc = mcc_gpu_run(&pcode, pin, 64, 1, pout);
			if (getenv("MCC_AST_EVAL_LADDER_GPU_DIAG")) { MCC_TRACE("br\n");
				fprintf(stderr, "[ladder-gpu] warmup rc=%d (%d units)\n", wrc, pcode.n);
				fflush(stderr);
			}
			mcc_gpu_code_free(&pcode);
		}
		ast_arena_free(pa);
	}
	atexit(ast_ladder_gpu_report);
}

void ast_ladder_gpu_report(void) { MCC_TRACE("enter\n");
	MccGpuStats gs;
	if (!ast_ladder_gpu_hook)
		{ MCC_TRACE("br\n"); return; }
	/* Reports, and no longer tears down. The quiesce that used to sit here ran
	 * from an atexit handler, which is what made its ordering against both the
	 * JIT pool and the driver's own unload accidental -- see the comment above
	 * ast_slice_inl_depth_cfg() for the unmapped-page failure that caused, and
	 * src/mccrt.h for where the teardown lives now. mcc_gpu_stats() reads
	 * mcc_gpu.ok, which quiesce deliberately leaves alone, so this still reports
	 * whether a device came up whichever order the two run in. */
	mcc_gpu_stats(&gs);
	fprintf(stderr,
					"[ladder-gpu] tried=%d available=%d device=%s rungs=%ld dispatches=%ld "
					"lanes=%ld\n",
					gs.tried, gs.ok, gs.name, ast_ladder_gpu_rungs, gs.dispatches,
					gs.lanes);
	{ MCC_TRACE("br\n");
		int i;
		long tot = 0;
		for (i = 0; i < AST_LGR_N; i++)
			{ MCC_TRACE("br\n"); tot += ast_ladder_gpu_refused[i]; }
		fprintf(stderr, "[ladder-gpu] forced=%d refused=%ld", ast_ladder_gpu_forced,
						tot);
		for (i = 0; i < AST_LGR_N; i++)
			{ MCC_TRACE("br\n"); fprintf(stderr, " %s=%ld", ast_ladder_gpu_reason[i],
																	 ast_ladder_gpu_refused[i]); }
		fprintf(stderr, "\n");
	}
#ifdef MCC_GPU_REFUSE_KINDS
	if (mcc_gpu_refuse_total) { MCC_TRACE("br\n");
		int i;
		fprintf(stderr, "[ladder-gpu] emitter-refused=%ld by-node", mcc_gpu_refuse_total);
		for (i = 0; i < MCC_GPU_REFUSE_KINDS; i++)
			if (mcc_gpu_refuse_kind[i])
				{ MCC_TRACE("br\n"); fprintf(stderr, " %s=%ld",
																		 ast_kind_name((uint16_t)i),
																		 mcc_gpu_refuse_kind[i]); }
		fprintf(stderr, "\n");
		fprintf(stderr, "[ladder-gpu] emitter-refused by-op");
		for (i = 0; i < mcc_gpu_refuse_opn; i++) { MCC_TRACE("br\n");
			int ov = mcc_gpu_refuse_opv[i];
			const char *on = NULL;
			if (ov == AST_OP_ADDR)
				{ MCC_TRACE("br\n"); on = "addr-of"; }
			else if (ov == AST_OP_MEMBER)
				{ MCC_TRACE("br\n"); on = "member"; }
			else if (ov == AST_OP_MEMBER_ARROW)
				{ MCC_TRACE("br\n"); on = "member-arrow"; }
			else if (ov == AST_OP_IMAG)
				{ MCC_TRACE("br\n"); on = "imag"; }
			if (on)
				{ MCC_TRACE("br\n"); fprintf(stderr, " %s=%ld", on,
																		 mcc_gpu_refuse_op[i]); }
			else if (ov > 32 && ov < 127)
				{ MCC_TRACE("br\n"); fprintf(stderr, " '%c'=%ld", (char)ov,
																		 mcc_gpu_refuse_op[i]); }
			else
				{ MCC_TRACE("br\n"); fprintf(stderr, " op%d=%ld", ov,
																		 mcc_gpu_refuse_op[i]); }
		}
		fprintf(stderr, "\n");
	}
#endif
}

#if MCC_EMBED_JIT
int ast_slice_certifiable(AstArena *a, AstLocal root) { MCC_TRACE("enter\n");
#ifdef AST_EVAL_SLICE_PROVIDED
	return ast_eval_slice_kind_ok(a, root, 0);
#else
	(void)a;
	(void)root;
	return 0;
#endif
}

int ast_jit_const_fn(AstArena *a, int64_t *out) { MCC_TRACE("enter\n");
#ifdef AST_EVAL_SLICE_PROVIDED
	AstLocal rets[AST_EVAL_SLICE_MAXRET];
	int nr = ast_eval_slice_returns(a, rets, AST_EVAL_SLICE_MAXRET);
	if (nr != 1)
		{ MCC_TRACE("br\n"); return 0; }
	return ast_eval_slice(a, rets[0], NULL, NULL, 0, out);
#else
	(void)a;
	(void)out;
	return 0;
#endif
}

int ast_jit_search_vocab(uint64_t *out, int max) { MCC_TRACE("enter\n");
	const AstGateMask base = AST_SG_TEMPLATES | AST_SG_NARROW | AST_SG_SETHI |
													 AST_SG_BITFLAG | AST_SG_RANGE | AST_SG_DSECALL |
													 AST_SG_TCOPTR | AST_SG_CSECOMM;
	const AstGateMask toggles[] = {
			AST_SG_TEMPLATES, AST_SG_NARROW, AST_SG_SETHI, AST_SG_BITFLAG,
			AST_SG_RANGE, AST_SG_DIVMAGIC, AST_SG_ABS, AST_SG_REASSOC,
			AST_SG_DSECALL, AST_SG_TCOPTR, AST_SG_CSECOMM, AST_SG_REASSOC_ASSOC,
			AST_SG_REASSOC_SHLSHR, AST_SG_REASSOC_SHRSHL, AST_SG_REASSOC_MULDIST,
			AST_SG_NARROWFIX, AST_SG_SETHILEAF, AST_SG_LTEMP, AST_SG_IVSR,
			AST_SG_PRE };
	int nt = (int)(sizeof toggles / sizeof toggles[0]);
	int n = 0, i, j;
	if (n < max)
		{ MCC_TRACE("br\n"); out[n++] = 0; }
	if (n < max)
		{ MCC_TRACE("br\n"); out[n++] = (uint64_t)base; }
	for (i = 0; i < nt && n < max; i++)
		{ MCC_TRACE("br\n"); out[n++] = (uint64_t)(base ^ toggles[i]); }
	for (i = 0; i < nt && n < max; i++)
		for (j = i + 1; j < nt && n < max; j++)
			{ MCC_TRACE("br\n"); out[n++] = (uint64_t)(base ^ toggles[i] ^ toggles[j]); }
	return n;
}

int ast_jit_fold_consts(AstArena *a) { MCC_TRACE("enter\n");
#ifdef AST_EVAL_SLICE_PROVIDED
	AstLocal n, cnt = ast_count(a);
	int folded = 0;
	for (n = 0; n < cnt; n++) { MCC_TRACE("br\n");
		int64_t v;
		uint16_t k = ast_kind(a, n);
		int bt;
		if (k != AST_Binary && k != AST_Unary)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_nchild(a, n) == 0)
			{ MCC_TRACE("br\n"); continue; }
		bt = ast_type_t(a, n) & VT_BTYPE;
		if (bt != VT_BOOL && bt != VT_BYTE && bt != VT_SHORT && bt != VT_INT &&
				bt != VT_LLONG)
			{ MCC_TRACE("br\n"); continue; }
		if (ast_eval_slice(a, n, NULL, NULL, 0, &v)) { MCC_TRACE("br\n");
			ast_clear_children(a, n);
			ast_set_kind(a, n, AST_Literal);
			ast_set_op(a, n, VT_CONST);
			ast_set_ival(a, n, (uint64_t)v);
			ast_set_fbits(a, n, 0);
			ast_set_sym(a, n, 0);
			folded++;
		}
	}
	return folded;
#else
	(void)a;
	return 0;
#endif
}

int ast_slice_equiv(AstArena *a, AstLocal aroot, AstArena *b,
										AstLocal broot) { MCC_TRACE("enter\n");
	ast_ladder_gpu_setup();
#ifdef AST_EVAL_SLICE_PROVIDED
	return ast_eval_slice_equiv(a, aroot, b, broot);
#else
	(void)a;
	(void)aroot;
	(void)b;
	(void)broot;
	return 0;
#endif
}

int ast_slice_live_ins(AstArena *a, AstLocal root, int32_t *offs,
											 int max) { MCC_TRACE("enter\n");
#ifdef AST_EVAL_SLICE_PROVIDED
	int cnt = 0;
	if (!ast_eval_slice_livein(a, root, offs, &cnt, max))
		{ MCC_TRACE("br\n"); return -1; }
	return cnt;
#else
	(void)a;
	(void)root;
	(void)offs;
	(void)max;
	return -1;
#endif
}

static AstLocal ast_slice_copy_into(AstArena *dst, const AstArena *src,
																		AstLocal snode) { MCC_TRACE("enter\n");
	AstLocal c, cc, n = ast_node(dst, ast_kind(src, snode));
	ast_set_op(dst, n, ast_op(src, snode));
	ast_copy_type(dst, n, src, snode);
	ast_set_ival(dst, n, ast_ival(src, snode));
	ast_set_fbits(dst, n, ast_fbits(src, snode));
	ast_set_sym(dst, n, ast_sym(src, snode));
	ast_set_wide(dst, n, ast_wide_hi(src, snode), ast_wide_r2(src, snode));
	for (c = ast_first_child(src, snode); c != AST_NONE; c = ast_next_sib(src, c)) {
		MCC_TRACE("br\n");
		cc = ast_slice_copy_into(dst, src, c);
		ast_add_child(dst, n, cc);
	}
	return n;
}

AstArena *ast_slice_wrap_kernel(const AstArena *a, AstLocal root) { MCC_TRACE("enter\n");
	AstArena *k;
	AstLocal bb, ret, e;
	if (!a || root >= a->count)
		{ MCC_TRACE("br\n"); return NULL; }
	if (ast_subtree_has_storeval(a, root))
		{ MCC_TRACE("br\n"); return NULL; }
	k = ast_arena_new();
	if (!k)
		{ MCC_TRACE("br\n"); return NULL; }
	bb = ast_node(k, AST_BasicBlock);
	ret = ast_node(k, AST_Return);
	e = ast_slice_copy_into(k, a, root);
	ast_add_child(k, ret, e);
	ast_add_child(k, bb, ret);
	return k;
}

static int ast_slice_subtree_size(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	AstLocal c;
	int k = 1;
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); k += ast_slice_subtree_size(a, c); }
	return k;
}

static void ast_slice_maximal_rec(AstArena *a, AstLocal n, AstLocal *roots,
																	int *sizes, int *cnt, int max) { MCC_TRACE("enter\n");
	AstLocal c;
	if (n == AST_NONE || *cnt >= max)
		{ MCC_TRACE("br\n"); return; }
#ifdef AST_EVAL_SLICE_PROVIDED
	if (ast_eval_slice_kind_ok(a, n, 0) && ast_nchild(a, n) > 0) { MCC_TRACE("br\n");
		roots[*cnt] = n;
		sizes[*cnt] = ast_slice_subtree_size(a, n);
		(*cnt)++;
		return;
	}
#endif
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_slice_maximal_rec(a, c, roots, sizes, cnt, max); }
}

typedef struct AstSliceSearchCtx {
	const int *sizes;
} AstSliceSearchCtx;

static long ast_slice_search_score(const int *sel, int k, void *user) { MCC_TRACE("enter\n");
	AstSliceSearchCtx *c = (AstSliceSearchCtx *)user;
	long tot = 0;
	int i;
	for (i = 0; i < k; i++)
		{ MCC_TRACE("br\n"); tot += c->sizes[sel[i]]; }
	return -tot;
}

int ast_slice_search(AstArena *a, AstLocal root, int budget, AstLocal *out,
										 int max) { MCC_TRACE("enter\n");
	AstLocal roots[COMBO_MAX];
	int sizes[COMBO_MAX];
	int cnt = 0, i, sel_n = 0;
	AstSliceSearchCtx ctx;
	ComboSpec spec;
	ComboBest best;
	if (!a || root >= a->count)
		{ MCC_TRACE("br\n"); return 0; }
	ast_slice_maximal_rec(a, root, roots, sizes, &cnt, COMBO_MAX);
	if (cnt == 0)
		{ MCC_TRACE("br\n"); return 0; }
	ctx.sizes = sizes;
	spec.nitems = cnt;
	spec.min_k = 1;
	spec.max_k = budget < 1 ? 1 : (budget > cnt ? cnt : budget);
	spec.ordered = 0;
	spec.walk = COMBO_WALK_LINEAR;
	spec.budget = 0;
	spec.score = ast_slice_search_score;
	spec.visit = NULL;
	spec.user = &ctx;
	if (!combo_run(&spec, &best))
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < best.k && sel_n < max; i++)
		{ MCC_TRACE("br\n"); out[sel_n++] = roots[best.sel[i]]; }
	return sel_n;
}
#endif

#if defined(AST_EVAL_SLICE_PROVIDED) && MCC_EMBED_JIT

#define AST_LADDER_CENSUS_ROOTS 32
#define AST_LADDER_CENSUS_PAIRS 128

typedef struct AstLadderStat {
	unsigned long pairs;
	unsigned long certified;
	unsigned long differ;
	unsigned long refused;
	unsigned long r_struct;
	unsigned long r_op;
	unsigned long r_type;
	unsigned long r_arity;
	unsigned long r_obs;
	unsigned long r_budget;
	unsigned long r_vacuous;
	unsigned long rung_const;
	unsigned long rung_obs;
	unsigned long rung_w[6];
	unsigned long exact;
	unsigned long diff_corners;
	unsigned long diff_w[6];
	unsigned long diff_const;
	unsigned long diff_obs;
	unsigned long points;
	unsigned long inferred;
	unsigned long inferred_pairs;
	unsigned long seed_yes;
	unsigned long seed_yes_ladder_no;
	unsigned long seed_yes_differ;
	unsigned long seed_yes_refused;
	unsigned long seed_yes_diff_w[6];
	unsigned long seed_yes_diff_other;
	unsigned long seed_no_ladder_yes;
	double secs;
} AstLadderStat;

static AstLadderStat ast_ladder_self;
static AstLadderStat ast_ladder_cross;
static int ast_ladder_census_env = -1;

static int ast_ladder_widx(int w) { MCC_TRACE("enter\n");
	switch (w) { MCC_TRACE("br\n");
	case 1: return 0;
	case 2: return 1;
	case 4: return 2;
	case 8: return 3;
	case 16: return 4;
	case 32: return 5;
	default: return -1;
	}
}

static double ast_ladder_now(void) { MCC_TRACE("enter\n");
#if MCC_HOST_WIN32
	LARGE_INTEGER freq, cnt;
	if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0)
		{ MCC_TRACE("br\n"); return 0.0; }
	QueryPerformanceCounter(&cnt);
	return (double)cnt.QuadPart / (double)freq.QuadPart;
#else
	struct timespec ts;
	if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
		{ MCC_TRACE("br\n"); return 0.0; }
	return (double)ts.tv_sec + (double)ts.tv_nsec * 1e-9;
#endif
}

static void ast_ladder_tally(AstLadderStat *st, const AstEvalLadderRes *r,
														 int seed_ok, double dt) { MCC_TRACE("enter\n");
	int wi;
	st->pairs++;
	st->points += r->points;
	st->inferred += r->inferred;
	if (r->inferred)
		{ MCC_TRACE("br\n"); st->inferred_pairs++; }
	st->secs += dt;
	if (seed_ok)
		{ MCC_TRACE("br\n"); st->seed_yes++; }
	if (r->verdict == 1) { MCC_TRACE("br\n");
		st->certified++;
		if (r->type_complete)
			{ MCC_TRACE("br\n"); st->exact++; }
		if (r->rung == AST_LADDER_RUNG_CONST)
			{ MCC_TRACE("br\n"); st->rung_const++; }
		else if (r->rung == AST_LADDER_RUNG_OBSERVED)
			{ MCC_TRACE("br\n"); st->rung_obs++; }
		else { MCC_TRACE("br\n");
			wi = ast_ladder_widx(r->rung);
			if (wi >= 0)
				{ MCC_TRACE("br\n"); st->rung_w[wi]++; }
		}
		if (!seed_ok)
			{ MCC_TRACE("br\n"); st->seed_no_ladder_yes++; }
		return;
	}
	if (r->verdict == 0) { MCC_TRACE("br\n");
		st->differ++;
		if (seed_ok) { MCC_TRACE("br\n");
			st->seed_yes_differ++;
			wi = ast_ladder_widx(r->rung);
			if (wi >= 0)
				{ MCC_TRACE("br\n"); st->seed_yes_diff_w[wi]++; }
			else
				{ MCC_TRACE("br\n"); st->seed_yes_diff_other++; }
		}
		if (r->rung == AST_LADDER_RUNG_CORNERS)
			{ MCC_TRACE("br\n"); st->diff_corners++; }
		else if (r->rung == AST_LADDER_RUNG_CONST)
			{ MCC_TRACE("br\n"); st->diff_const++; }
		else if (r->rung == AST_LADDER_RUNG_OBSERVED)
			{ MCC_TRACE("br\n"); st->diff_obs++; }
		else { MCC_TRACE("br\n");
			wi = ast_ladder_widx(r->rung);
			if (wi >= 0)
				{ MCC_TRACE("br\n"); st->diff_w[wi]++; }
		}
	} else { MCC_TRACE("br\n");
		st->refused++;
		if (seed_ok)
			{ MCC_TRACE("br\n"); st->seed_yes_refused++; }
		switch (r->reason) { MCC_TRACE("br\n");
		case AST_LADDER_R_STRUCT: st->r_struct++; break;
		case AST_LADDER_R_OP: st->r_op++; break;
		case AST_LADDER_R_TYPE: st->r_type++; break;
		case AST_LADDER_R_ARITY: st->r_arity++; break;
		case AST_LADDER_R_OBS: st->r_obs++; break;
		case AST_LADDER_R_BUDGET: st->r_budget++; break;
		case AST_LADDER_R_VACUOUS: st->r_vacuous++; break;
		default: break;
		}
	}
	if (seed_ok)
		{ MCC_TRACE("br\n"); st->seed_yes_ladder_no++; }
}

static const char *ast_ladder_rung_name(int rung, char *buf, size_t cap) { MCC_TRACE("enter\n");
	if (rung == AST_LADDER_RUNG_CONST)
		{ MCC_TRACE("br\n"); return "const"; }
	if (rung == AST_LADDER_RUNG_CORNERS)
		{ MCC_TRACE("br\n"); return "corners"; }
	if (rung == AST_LADDER_RUNG_OBSERVED)
		{ MCC_TRACE("br\n"); return "observed"; }
	if (rung == AST_LADDER_RUNG_NONE)
		{ MCC_TRACE("br\n"); return "none"; }
	snprintf(buf, cap, "w%d", rung);
	return buf;
}

static const char *ast_ladder_reason_name(int reason) { MCC_TRACE("enter\n");
	switch (reason) { MCC_TRACE("br\n");
	case AST_LADDER_R_STRUCT: return "unsupported-node";
	case AST_LADDER_R_OP: return "unsupported-op";
	case AST_LADDER_R_TYPE: return "no-static-type";
	case AST_LADDER_R_ARITY: return "too-many-live-ins";
	case AST_LADDER_R_BUDGET: return "over-budget";
	case AST_LADDER_R_VACUOUS: return "all-points-undefined";
	case AST_LADDER_R_OBS: return "no-observed-tuples";
	default: return "none";
	}
}

void ast_slice_ladder_set(int on) { MCC_TRACE("enter\n");
	ast_eval_ladder_set(on);
}

int ast_slice_ladder_on(void) { MCC_TRACE("enter\n");
	return ast_eval_ladder_on();
}

void ast_slice_ladder_observed_source(int (*fn)(const int32_t *, int, int64_t *,
																								int, void *),
																			void *user) { MCC_TRACE("enter\n");
	ast_eval_ladder_obs_fn = fn;
	ast_eval_ladder_obs_user = user;
}

int ast_slice_ladder_explain(AstArena *a, AstLocal aroot, AstArena *b,
														 AstLocal broot, char *buf, size_t cap) { MCC_TRACE("enter\n");
	AstEvalLadderRes r;
	ast_ladder_gpu_setup();
	char rb[16];
	ast_eval_slice_ladder(a, aroot, b, broot, &r);
	if (buf && cap) { MCC_TRACE("br\n");
		if (r.verdict == 1)
			{ MCC_TRACE("br\n"); snprintf(buf, cap,
											 "equiv rung=%s n=%d exact=%d inferred=%lu points=%lu",
											 ast_ladder_rung_name(r.rung, rb, sizeof rb), r.nin,
											 r.type_complete, r.inferred, r.points); }
		else if (r.verdict == 0)
			{ MCC_TRACE("br\n"); snprintf(buf, cap,
											 "differ rung=%s smallest-width=%d n=%d a=%lld b=%s%lld",
											 ast_ladder_rung_name(r.rung, rb, sizeof rb),
											 r.diff_width, r.nin, (long long)r.diff_a,
											 r.diff_b_undef ? "undef:" : "", (long long)r.diff_b); }
		else
			{ MCC_TRACE("br\n"); snprintf(buf, cap, "refused %s n=%d",
											 ast_ladder_reason_name(r.reason), r.nin); }
	}
	return r.verdict;
}

static void ast_ladder_dump_one(const char *tag, const AstLadderStat *st) { MCC_TRACE("enter\n");
	if (st->pairs == 0)
		{ MCC_TRACE("br\n"); return; }
	fprintf(stderr,
					"[ladder-%s] pairs=%lu certified=%lu differ=%lu refused=%lu "
					"exact=%lu\n",
					tag, st->pairs, st->certified, st->differ, st->refused, st->exact);
	fprintf(stderr,
					"[ladder-%s] rung const=%lu w1=%lu w2=%lu w4=%lu w8=%lu w16=%lu "
					"w32=%lu observed=%lu\n",
					tag, st->rung_const, st->rung_w[0], st->rung_w[1], st->rung_w[2],
					st->rung_w[3], st->rung_w[4], st->rung_w[5], st->rung_obs);
	fprintf(stderr,
					"[ladder-%s] diff const=%lu w1=%lu w2=%lu w4=%lu w8=%lu w16=%lu "
					"w32=%lu corners=%lu observed=%lu\n",
					tag, st->diff_const, st->diff_w[0], st->diff_w[1], st->diff_w[2],
					st->diff_w[3], st->diff_w[4], st->diff_w[5], st->diff_corners,
					st->diff_obs);
	fprintf(stderr,
					"[ladder-%s] refuse no-static-type=%lu unsupported-node=%lu "
					"unsupported-op=%lu too-many-live-ins=%lu no-observed=%lu "
					"over-budget=%lu all-undefined=%lu\n",
					tag, st->r_type, st->r_struct, st->r_op, st->r_arity, st->r_obs,
					st->r_budget, st->r_vacuous);
	fprintf(stderr,
					"[ladder-%s] seed-certified=%lu seed-yes-ladder-no=%lu "
					"seed-no-ladder-yes=%lu\n",
					tag, st->seed_yes, st->seed_yes_ladder_no, st->seed_no_ladder_yes);
	fprintf(stderr,
					"[ladder-%s] seed-yes-refuted=%lu (w1=%lu w2=%lu w4=%lu w8=%lu "
					"w16=%lu w32=%lu other=%lu) seed-yes-refused=%lu\n",
					tag, st->seed_yes_differ, st->seed_yes_diff_w[0],
					st->seed_yes_diff_w[1], st->seed_yes_diff_w[2],
					st->seed_yes_diff_w[3], st->seed_yes_diff_w[4],
					st->seed_yes_diff_w[5], st->seed_yes_diff_other,
					st->seed_yes_refused);
	fprintf(stderr,
					"[ladder-%s] inferred-width-nodes=%lu pairs-with-inferred-width=%lu\n",
					tag, st->inferred, st->inferred_pairs);
	fprintf(stderr, "[ladder-%s] points=%lu\n", tag, st->points);
	fprintf(stderr, "[ladder-%s] secs=%.4f us-per-pair=%.2f\n", tag, st->secs,
					st->pairs ? st->secs * 1e6 / (double)st->pairs : 0.0);
}

void ast_slice_ladder_stats_dump(void) { MCC_TRACE("enter\n");
	ast_ladder_dump_one("self", &ast_ladder_self);
	ast_ladder_dump_one("cross", &ast_ladder_cross);
}

static void ast_ladder_census_rec(AstArena *a, AstLocal n, AstLocal *roots,
																	int *cnt, int max) { MCC_TRACE("enter\n");
	AstLocal c;
	if (n == AST_NONE || *cnt >= max)
		{ MCC_TRACE("br\n"); return; }
	if (ast_eval_slice_kind_ok(a, n, 0) && ast_nchild(a, n) > 0) { MCC_TRACE("br\n");
		roots[(*cnt)++] = n;
		return;
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_ladder_census_rec(a, c, roots, cnt, max); }
}

static void ast_ladder_census(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal roots[AST_LADDER_CENSUS_ROOTS];
	AstArena *g;
	int cnt = 0, i, j, pairs = 0;
	if (ast_ladder_census_env < 0) { MCC_TRACE("br\n");
		ast_ladder_census_env =
				ast_ladder_gpu_forced || mcc_env_on("MCC_AST_EVAL_LADDER_CENSUS");
		if (ast_ladder_census_env)
			{ MCC_TRACE("br\n"); atexit(ast_slice_ladder_stats_dump); }
	}
	if (!ast_ladder_census_env || !a)
		{ MCC_TRACE("br\n"); return; }
	g = ast_slice_leaf_inline(a);
	if (g)
		{ MCC_TRACE("br\n"); a = g; }
	ast_ladder_census_rec(a, ast_root(a), roots, &cnt, AST_LADDER_CENSUS_ROOTS);
	for (i = 0; i < cnt; i++) { MCC_TRACE("br\n");
		AstArena *cp = ast_slice_extract(a, roots[i]);
		AstEvalLadderRes r;
		double t0, t1;
		int seed_ok;
		if (!cp)
			{ MCC_TRACE("br\n"); continue; }
		seed_ok = ast_eval_slice_equiv_seed(a, roots[i], cp, ast_root(cp));
		t0 = ast_ladder_now();
		ast_eval_slice_ladder(a, roots[i], cp, ast_root(cp), &r);
		t1 = ast_ladder_now();
		ast_ladder_tally(&ast_ladder_self, &r, seed_ok, t1 - t0);
		ast_arena_free(cp);
	}
	for (i = 0; i < cnt && pairs < AST_LADDER_CENSUS_PAIRS; i++)
		for (j = i + 1; j < cnt && pairs < AST_LADDER_CENSUS_PAIRS; j++) { MCC_TRACE("br\n");
			AstEvalLadderRes r;
			double t0, t1;
			int seed_ok = ast_eval_slice_equiv_seed(a, roots[i], a, roots[j]);
			t0 = ast_ladder_now();
			ast_eval_slice_ladder(a, roots[i], a, roots[j], &r);
			t1 = ast_ladder_now();
			ast_ladder_tally(&ast_ladder_cross, &r, seed_ok, t1 - t0);
			pairs++;
		}
	if (g)
		{ MCC_TRACE("br\n"); ast_arena_free(g); }
}

#endif

static unsigned long ast_search_gates_now(void) { MCC_TRACE("enter\n");
	return (ast_templates_env ? AST_SG_TEMPLATES : 0) |
				 (ast_narrow_env ? AST_SG_NARROW : 0) |
				 (ast_bitflag_env ? AST_SG_BITFLAG : 0) |
				 (ast_sethi_env ? AST_SG_SETHI : 0) |
				 (ast_narrow_fix_env ? AST_SG_NARROWFIX : 0) |
				 (ast_sethi_leaf_env ? AST_SG_SETHILEAF : 0) |
				 (ast_licm_temp_env ? AST_SG_LTEMP : 0) |
				 (ast_ivsr_env ? AST_SG_IVSR : 0) |
				 (ast_pre_env ? AST_SG_PRE : 0) |
				 (ast_dse_call_env ? AST_SG_DSECALL : 0) |
				 (ast_tco_ptr_env ? AST_SG_TCOPTR : 0) |
				 (ast_cse_comm_env ? AST_SG_CSECOMM : 0) |
				 (ast_range_env ? AST_SG_RANGE : 0) |
				 (ast_divmagic_env ? AST_SG_DIVMAGIC : 0) |
				 (ast_abs_env ? AST_SG_ABS : 0) |
				 (ast_reassoc_env ? AST_SG_REASSOC : 0) |
				 (ast_sccp_fix_env ? AST_SG_SCCPFIX : 0) |
				 (ast_ident_conv_env ? AST_SG_IDENT_CONV : 0) |
				 (ast_ident_shift_env ? AST_SG_IDENT_SHIFT : 0) |
				 (ast_ident_arith_env ? AST_SG_IDENT_ARITH : 0) |
				 (ast_ident_bit_env ? AST_SG_IDENT_BIT : 0) |
				 (ast_ident_rel_env ? AST_SG_IDENT_REL : 0) |
				 (ast_ident_urange_env ? AST_SG_IDENT_URANGE : 0) |
				 (ast_reassoc_assoc_env ? AST_SG_REASSOC_ASSOC : 0) |
				 (ast_reassoc_shlshr_env ? AST_SG_REASSOC_SHLSHR : 0) |
				 (ast_reassoc_shrshl_env ? AST_SG_REASSOC_SHRSHL : 0) |
				 (ast_reassoc_muldist_env ? AST_SG_REASSOC_MULDIST : 0) |
				 (ast_bfold_sqrt_env ? AST_SG_BFOLD_SQRT : 0) |
				 (ast_bfold_sign_env ? AST_SG_BFOLD_SIGN : 0) |
				 (ast_bfold_round_env ? AST_SG_BFOLD_ROUND : 0) |
				 (ast_bfold_minmax_env ? AST_SG_BFOLD_MINMAX : 0) |
				 (ast_narrow_c0_env ? AST_SG_NARROW_C0 : 0) |
				 (ast_narrow_c1_env ? AST_SG_NARROW_C1 : 0) |
				 (ast_narrow_c2_env ? AST_SG_NARROW_C2 : 0) |
				 (ast_narrow_c3_env ? AST_SG_NARROW_C3 : 0) |
				 (ast_vlat_env ? AST_SG_VLAT : 0) |
				 (ast_math_inline_prepass_env ? AST_SG_MATHPRE : 0) |
				 (ast_interchange_env ? AST_SG_INTERCHANGE : 0) |
				 (ast_fusion_env ? AST_SG_FUSION : 0) |
				 (ast_tile_env ? AST_SG_TILE : 0);
}

static void ast_search_gates_set(AstGateMask g) { MCC_TRACE("enter\n");
	g |= ast_search_floor;
	ast_templates_env = (g & AST_SG_TEMPLATES) != 0;
	ast_narrow_env = (g & AST_SG_NARROW) != 0;
	ast_bitflag_env = (g & AST_SG_BITFLAG) != 0;
	ast_sethi_env = (g & AST_SG_SETHI) != 0;
	ast_narrow_fix_env = (g & AST_SG_NARROWFIX) != 0;
	ast_sethi_leaf_env = (g & AST_SG_SETHILEAF) != 0;
	ast_licm_temp_env = (g & AST_SG_LTEMP) != 0;
	ast_ivsr_env = (g & AST_SG_IVSR) != 0;
	ast_pre_env = (g & AST_SG_PRE) != 0;
	ast_dse_call_env = (g & AST_SG_DSECALL) != 0;
	ast_tco_ptr_env = (g & AST_SG_TCOPTR) != 0;
	ast_cse_comm_env = (g & AST_SG_CSECOMM) != 0;
	ast_range_env = (g & AST_SG_RANGE) != 0;
	ast_divmagic_env = (g & AST_SG_DIVMAGIC) != 0;
	ast_abs_env = (g & AST_SG_ABS) != 0;
	ast_reassoc_env = (g & AST_SG_REASSOC) != 0;
	ast_sccp_fix_env = (g & AST_SG_SCCPFIX) != 0;
	ast_ident_conv_env = (g & AST_SG_IDENT_CONV) != 0;
	ast_ident_shift_env = (g & AST_SG_IDENT_SHIFT) != 0;
	ast_ident_arith_env = (g & AST_SG_IDENT_ARITH) != 0;
	ast_ident_bit_env = (g & AST_SG_IDENT_BIT) != 0;
	ast_ident_rel_env = (g & AST_SG_IDENT_REL) != 0;
	ast_ident_urange_env = (g & AST_SG_IDENT_URANGE) != 0;
	ast_reassoc_assoc_env = (g & AST_SG_REASSOC_ASSOC) != 0;
	ast_reassoc_shlshr_env = (g & AST_SG_REASSOC_SHLSHR) != 0;
	ast_reassoc_shrshl_env = (g & AST_SG_REASSOC_SHRSHL) != 0;
	ast_reassoc_muldist_env = (g & AST_SG_REASSOC_MULDIST) != 0;
	ast_bfold_sqrt_env = (g & AST_SG_BFOLD_SQRT) != 0;
	ast_bfold_sign_env = (g & AST_SG_BFOLD_SIGN) != 0;
	ast_bfold_round_env = (g & AST_SG_BFOLD_ROUND) != 0;
	ast_bfold_minmax_env = (g & AST_SG_BFOLD_MINMAX) != 0;
	ast_narrow_c0_env = (g & AST_SG_NARROW_C0) != 0;
	ast_narrow_c1_env = (g & AST_SG_NARROW_C1) != 0;
	ast_narrow_c2_env = (g & AST_SG_NARROW_C2) != 0;
	ast_narrow_c3_env = (g & AST_SG_NARROW_C3) != 0;
	ast_vlat_env = (g & AST_SG_VLAT) != 0;
	ast_math_inline_prepass_env = (g & AST_SG_MATHPRE) != 0;
	ast_interchange_env = (g & AST_SG_INTERCHANGE) != 0;
	ast_fusion_env = (g & AST_SG_FUSION) != 0;
	ast_tile_env = (g & AST_SG_TILE) != 0;
}

#ifndef SHF_PRIVATE
#define SHF_PRIVATE 0x80000000
#endif

typedef struct {
	Section *sec;
	unsigned long sec_doff;
	int ind, rsym, loc, anon_sym;
	SValue *vtop;
	Sym *lsmark;
	uint64_t pinned;
	int promo_n, promo_callful, promo_save_loc;
	int promo_total, graft_total, opt_total;
} AstScratchSave;

static void ast_scratch_init(void) { MCC_TRACE("enter\n");
	Section *sc, *sr;
	char buf[64];
	if (mcc_state->ast_scratch_sec)
		{ MCC_TRACE("br\n"); return; }
	sc = new_section(mcc_state, ".mcc.scratch.text", SHT_PROGBITS, SHF_PRIVATE);
	snprintf(buf, sizeof(buf), REL_SECTION_FMT, ".mcc.scratch.text");
	sr = new_section(mcc_state, buf, SHT_RELX, SHF_PRIVATE);
	sr->sh_entsize = sizeof(ElfW_Rel);
	sr->link = symtab_section;
	sr->sh_info = sc->sh_num;
	sc->reloc = sr;
	mcc_state->ast_scratch_sec = sc;
}

static void ast_scratch_enter(AstScratchSave *sv) { MCC_TRACE("enter\n");
	Section *sc;
	ast_scratch_init();
	sc = mcc_state->ast_scratch_sec;
	sv->sec = cur_text_section;
	sv->sec_doff = cur_text_section->data_offset;
	sv->ind = ind;
	sv->rsym = rsym;
	sv->loc = loc;
	sv->anon_sym = anon_sym;
	sv->vtop = vtop;
	sv->lsmark = local_stack;
	sv->pinned = ast_pinned_regs;
	sv->promo_n = ast_promo_n;
	sv->promo_callful = ast_promo_callful;
	sv->promo_save_loc = ast_promo_save_loc;
	sv->promo_total = ast_promo_total;
	sv->graft_total = ast_graft_total;
	sv->opt_total = ast_opt_total;
	cur_text_section = sc;
	sc->data_offset = 0;
	if (sc->reloc)
		{ MCC_TRACE("br\n"); sc->reloc->data_offset = 0; }
	MCC_TRACE("scratch enter sec=%s ind0=%d\n", sv->sec->name, sv->ind);
}

static int ast_scratch_measure_exit(AstScratchSave *sv) { MCC_TRACE("enter\n");
	Section *sc = mcc_state->ast_scratch_sec;
	int size = ind - ast_body_ind_sv;
	MCC_TRACE("scratch measure size=%d\n", size);
	sc->data_offset = 0;
	if (sc->reloc)
		{ MCC_TRACE("br\n"); sc->reloc->data_offset = 0; }
	sym_pop(&local_stack, sv->lsmark, 0);
	cur_text_section = sv->sec;
	ind = sv->ind;
	rsym = sv->rsym;
	loc = sv->loc;
	anon_sym = sv->anon_sym;
	vtop = sv->vtop;
	ast_pinned_regs = sv->pinned;
	ast_promo_n = sv->promo_n;
	ast_promo_callful = sv->promo_callful;
	ast_promo_save_loc = sv->promo_save_loc;
	ast_promo_total = sv->promo_total;
	ast_graft_total = sv->graft_total;
	ast_opt_total = sv->opt_total;
	if (sv->sec->data_offset != sv->sec_doff) { MCC_TRACE("br\n");
		MCC_TRACE("scratch LEAK doff=%lu/%lu\n", sv->sec->data_offset,
							sv->sec_doff);
		mcc_internal_error("ast scratch isolation leak");
	}
	MCC_TRACE("scratch exit ok\n");
	return size;
}

static int ast_search_emit_size(AstArena *a, int saved_loc, int saved_anon) { MCC_TRACE("enter\n");
	AstScratchSave scr;
	Section *rsec;
	AstArena *save_cur = ast_cur;
	addr_t save_doff, save_roff;
	addr_t ddelta, rodelta;
	int size;
	ast_scratch_enter(&scr);
	rsec = cur_text_section->reloc;
	save_doff = data_section ? data_section->data_offset : 0;
	save_roff = rodata_section ? rodata_section->data_offset : 0;
	ind = ast_body_ind_sv;
	rsym = 0;
	if (rsec)
		{ MCC_TRACE("br\n"); rsec->data_offset = ast_reloc0_sv; }
	nocode_wanted = 0;
	loc = ast_ltemp_n ? ast_ltemp_cur : saved_loc;
	anon_sym = saved_anon;
	ast_fconst_i = ast_fconst_n;
	ast_locrec_i = 0;
	ast_replaying = 1;
	ast_rp_switch = NULL;
	ast_rp_nlabel = 0;
	ast_rp_bsym = ast_rp_csym = NULL;
	ast_pinned_regs = 0;
	ast_inline_active = ast_search_emitiso_env ? ast_search_want_inline : 0;
	ast_graft_budget = ast_graft_budget_max;
	ast_loc_low = loc;
	ast_graft_base = loc;
	ast_temp_frontier = 1;
	ast_cur = a;
	ast_replay_body(a);
	if (ast_loc_low < loc)
		{ MCC_TRACE("br\n"); loc = ast_loc_low; }
	ast_replaying = 0;
	ast_inline_active = 0;
	ddelta = (data_section ? data_section->data_offset : 0) - save_doff;
	rodelta = (rodata_section ? rodata_section->data_offset : 0) - save_roff;
	size = ind - ast_body_ind_sv;
	if (ddelta || rodelta)
		{ MCC_TRACE("br\n"); MCC_TRACE("emit-size data delta text=%d data=%lld rodata=%lld\n", size,
							(long long)ddelta, (long long)rodelta); }
	ast_cur = save_cur;
	if (data_section) { MCC_TRACE("br\n");
		if (data_section->data && data_section->data_offset > save_doff)
			{ MCC_TRACE("br\n"); memset(data_section->data + save_doff, 0,
															 data_section->data_offset - save_doff); }
		data_section->data_offset = save_doff;
	}
	if (rodata_section) { MCC_TRACE("br\n");
		if (rodata_section->data && rodata_section->data_offset > save_roff)
			{ MCC_TRACE("br\n"); memset(rodata_section->data + save_roff, 0,
															 rodata_section->data_offset - save_roff); }
		rodata_section->data_offset = save_roff;
	}
	size = ast_scratch_measure_exit(&scr);
	return size;
}

#define AST_SCORE_HITBITS 12
#define AST_SCORE_HITMAX ((1L << AST_SCORE_HITBITS) - 1)
static long ast_search_pack_score(long primary, long hits) { MCC_TRACE("enter\n");
	long h;
	if (primary < 0)
		{ MCC_TRACE("br\n"); return -1; }
	h = hits < 0 ? 0 : (hits > AST_SCORE_HITMAX ? AST_SCORE_HITMAX : hits);
	return (primary << AST_SCORE_HITBITS) + (AST_SCORE_HITMAX - h);
}

static long ast_search_score_emitsize(AstArena *pristine, Sym *sym, int faithful,
																			AstGateMask gates, int saved_loc,
																			int saved_anon) { MCC_TRACE("enter\n");
	AstArena *saved_cur = ast_cur, *trial = ast_arena_clone(pristine);
	long size, hits;
	if (!trial)
		{ MCC_TRACE("br\n"); return -1; }
	ast_search_gates_set(gates);
	ast_cur = trial;
	AstLtempSave ltsv;
	ast_ltemp_save(&ltsv);
	hits = ast_run_strat_cycle(trial, sym, faithful, ast_strat_order,
														 ast_strat_order_n, NULL);
	size = ast_search_emit_size(trial, saved_loc, saved_anon);
	ast_ltemp_restore(&ltsv);
	ast_cur = saved_cur;
	ast_arena_free(trial);
	return ast_search_pack_score(size, hits);
}

static long ast_search_score_one(AstArena *pristine, Sym *sym, int faithful,
																 AstGateMask gates, int saved_loc, int saved_anon) { MCC_TRACE("enter\n");
	AstArena *saved_cur, *trial;
	long sc, hits;
	ast_search_evals++;
	if (ast_search_emitsize_env)
		{ MCC_TRACE("br\n"); return ast_search_score_emitsize(pristine, sym, faithful, gates, saved_loc,
																		 saved_anon); }
	saved_cur = ast_cur;
	trial = ast_arena_clone(pristine);
	if (!trial)
		{ MCC_TRACE("br\n"); return -1; }
	ast_search_gates_set(gates);
	ast_cur = trial;
	AstLtempSave ltsv;
	ast_ltemp_save(&ltsv);
	hits = ast_run_strat_cycle(trial, sym, faithful, ast_strat_order,
														 ast_strat_order_n, NULL);
	sc = ast_cost_score(trial);
	ast_ltemp_restore(&ltsv);
	ast_cur = saved_cur;
	ast_arena_free(trial);
	return ast_search_pack_score(sc, hits);
}

static long ast_search_score_order(AstArena *pristine, Sym *sym, int faithful,
																	 const int *seq, int k, int saved_loc,
																	 int saved_anon) { MCC_TRACE("enter\n");
	AstArena *saved_cur, *trial;
	long sc, hits;
	trial = ast_arena_clone(pristine);
	if (!trial)
		{ MCC_TRACE("br\n"); return -1; }
	saved_cur = ast_cur;
	ast_cur = trial;
	AstLtempSave ltsv;
	ast_ltemp_save(&ltsv);
	hits = ast_run_strat_cycle(trial, sym, faithful, seq, k, NULL);
	sc = ast_search_emitsize_env ? ast_search_emit_size(trial, saved_loc, saved_anon)
															 : ast_cost_score(trial);
	ast_ltemp_restore(&ltsv);
	ast_cur = saved_cur;
	ast_arena_free(trial);
	return ast_search_pack_score(sc, hits);
}

typedef struct AstOrderCtx {
	AstArena *pristine;
	Sym *sym;
	int faithful;
	int saved_loc;
	int saved_anon;
	const int *rows;
	uint64_t tried;
	int ord;
} AstOrderCtx;

static long ast_search_order_combo_score(const int *sel, int k, void *user) { MCC_TRACE("enter\n");
	AstOrderCtx *cx = (AstOrderCtx *)user;
	int seq[AST_STRAT_COUNT_MAX];
	unsigned t0;
	long sc;
	int i;
	if (ast_search_should_stop())
		{ MCC_TRACE("br\n"); return COMBO_REJECT; }
	if (cx->ord < 64)
		{ MCC_TRACE("br\n"); cx->tried |= (uint64_t)1 << cx->ord; }
	cx->ord++;
	for (i = 0; i < k; i++)
		{ MCC_TRACE("br\n"); seq[i] = cx->rows[sel[i]]; }
	t0 = ast_now_ms();
	sc = ast_search_score_order(cx->pristine, cx->sym, cx->faithful, seq, k, cx->saved_loc,
															cx->saved_anon);
	ast_search_durwin_push(ast_now_ms() - t0);
	if (sc < 0)
		{ MCC_TRACE("br\n"); return COMBO_REJECT; }
	return sc;
}

typedef struct AstComboCtx {
	AstArena *pristine;
	Sym *sym;
	int faithful;
	int saved_loc;
	int saved_anon;
	const AstGateMask *items;
	uint64_t tried;
	uint64_t skip;
	int ord;
	long best_score;
} AstComboCtx;

static long ast_search_combo_score(const int *sel, int k, void *user) { MCC_TRACE("enter\n");
	AstComboCtx *cx = (AstComboCtx *)user;
	AstGateMask gates = 0;
	unsigned t0;
	long sc;
	int i;
	if (ast_search_should_stop())
		{ MCC_TRACE("br\n"); return COMBO_REJECT; }
	if (cx->ord < 64 && (cx->skip & ((uint64_t)1 << cx->ord))) { MCC_TRACE("br\n");
		cx->tried |= (uint64_t)1 << cx->ord;
		cx->ord++;
		return COMBO_REJECT;
	}
	if (cx->ord < 64)
		{ MCC_TRACE("br\n"); cx->tried |= (uint64_t)1 << cx->ord; }
	cx->ord++;
	for (i = 0; i < k; i++)
		{ MCC_TRACE("br\n"); gates |= cx->items[sel[i]]; }
	t0 = ast_now_ms();
	sc = ast_search_score_one(cx->pristine, cx->sym, cx->faithful, gates,
														cx->saved_loc, cx->saved_anon);
	ast_search_durwin_push(ast_now_ms() - t0);
	MCC_TRACE("combo cand gates=%llx k=%d score=%ld\n", (unsigned long long)gates, k, sc);
	if (mcc_stats_mask) { MCC_TRACE("br\n");
		int improved = sc >= 0 && (cx->best_score < 0 || sc < cx->best_score);
		mcc_stats_combo_cand(gates, sel, k, cx->items, sc, cx->ord,
												 ast_now_ms() - ast_search_start_ms, ast_search_budget_ms,
												 ast_search_expect_ms());
		mcc_stats_combo_outcome(improved, sc < 0, ast_search_ordered_env ? 1 : 0);
		if (improved)
			{ MCC_TRACE("br\n"); cx->best_score = sc; }
	}
	if (sc < 0)
		{ MCC_TRACE("br\n"); return COMBO_REJECT; }
	return sc;
}

#if MCC_HOST_POSIX
#include <sys/wait.h>
#include <unistd.h>

typedef struct AstScoreRec {
	int idx;
	long score;
} AstScoreRec;

static int ast_search_pool(AstArena *pristine, Sym *sym, int faithful,
													 const AstGateMask *gatelist, int nc, int saved_loc,
													 int saved_anon, AstGateMask *best_out,
													 long *best_score_out) { MCC_TRACE("enter\n");
	int nw = host_nproc() - 1, pipefd[2], w, i, done = 0;
	pid_t pids[64];
	AstGateMask best = gatelist[0];
	long best_score = -1;
	long *results;
	AstScoreRec rec;
	if (nw < 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (nw > nc)
		{ MCC_TRACE("br\n"); nw = nc; }
	if (nw > 64)
		{ MCC_TRACE("br\n"); nw = 64; }
	results = mcc_malloc((size_t)nc * sizeof *results);
	if (!results)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < nc; i++)
		{ MCC_TRACE("br\n"); results[i] = -1; }
	if (pipe(pipefd) != 0)
		{ MCC_TRACE("br\n"); mcc_free(results); return 0; }
	for (w = 0; w < nw; w++) { MCC_TRACE("br\n");
		pid_t pid = fork();
		if (pid == 0) { MCC_TRACE("br\n");
			close(pipefd[0]);
			for (i = w; i < nc; i += nw) { MCC_TRACE("br\n");
				AstScoreRec r;
				r.idx = i;
				r.score = ast_search_score_one(pristine, sym, faithful, gatelist[i],
																			 saved_loc, saved_anon);
				if (write(pipefd[1], &r, sizeof r) != (ssize_t)sizeof r)
					{ MCC_TRACE("br\n"); break; }
			}
			close(pipefd[1]);
			_exit(0);
		}
		pids[w] = pid;
	}
	close(pipefd[1]);
	while (read(pipefd[0], &rec, sizeof rec) == (ssize_t)sizeof rec) { MCC_TRACE("br\n");
		if (rec.idx < 0 || rec.idx >= nc)
			{ MCC_TRACE("br\n"); continue; }
		done++;
		results[rec.idx] = rec.score;
	}
	close(pipefd[0]);
	for (w = 0; w < nw; w++)
		{ MCC_TRACE("br\n"); if (pids[w] > 0) { MCC_TRACE("br\n");
			int st;
			waitpid(pids[w], &st, 0);
		} }
	if (done != nc) { MCC_TRACE("br\n");
		MCC_TRACE("fork pool: %d of %d candidates reported, refusing a partial "
							"result that would depend on which worker won the race\n",
							done, nc);
		mcc_free(results);
		return 0;
	}
	for (i = 0; i < nc; i++)
		{ MCC_TRACE("br\n"); if (results[i] >= 0 && (best_score < 0 || results[i] < best_score)) { MCC_TRACE("br\n");
			best_score = results[i];
			best = gatelist[i];
		} }
	mcc_free(results);
	if (best_score < 0)
		{ MCC_TRACE("br\n"); return 0; }
	*best_out = best;
	*best_score_out = best_score;
	return 1;
}

#include <pthread.h>
typedef struct AstScoreThreadArg {
	AstArena *pristine;
	Sym *sym;
	int faithful;
	const AstGateMask *gatelist;
	int nc;
	int saved_loc;
	int saved_anon;
	int wid;
	int nw;
	long *results;
} AstScoreThreadArg;

static void *ast_search_thread_fn(void *p) { MCC_TRACE("enter\n");
	AstScoreThreadArg *a = (AstScoreThreadArg *)p;
	int i;
	for (i = a->wid; i < a->nc; i += a->nw)
		{ MCC_TRACE("br\n"); a->results[i] = ast_search_score_one(
				a->pristine, a->sym, a->faithful, a->gatelist[i], a->saved_loc,
				a->saved_anon); }
	return NULL;
}

static int ast_search_pool_pthreads(AstArena *pristine, Sym *sym, int faithful,
																		const AstGateMask *gatelist, int nc,
																		int saved_loc, int saved_anon,
																		AstGateMask *best_out,
																		long *best_score_out) { MCC_TRACE("enter\n");
	int nw = host_nproc() - 1, w, i;
	pthread_t th[64];
	AstScoreThreadArg args[64];
	long *results;
	AstGateMask best = gatelist[0];
	long best_score = -1;
	if (mcc_env_on("MCC_AST_PTHREADS_DIAG"))
		{ MCC_TRACE("br\n"); fprintf(stderr, "[pthreads-pool] entry nw=%d nc=%d\n", nw, nc); }
	if (nw < 2)
		{ MCC_TRACE("br\n"); return 0; }
	if (nw > nc)
		{ MCC_TRACE("br\n"); nw = nc; }
	if (nw > 64)
		{ MCC_TRACE("br\n"); nw = 64; }
	results = mcc_malloc((size_t)nc * sizeof *results);
	if (!results)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; i < nc; i++)
		{ MCC_TRACE("br\n"); results[i] = -1; }
	for (w = 0; w < nw; w++) { MCC_TRACE("br\n");
		args[w].pristine = pristine;
		args[w].sym = sym;
		args[w].faithful = faithful;
		args[w].gatelist = gatelist;
		args[w].nc = nc;
		args[w].saved_loc = saved_loc;
		args[w].saved_anon = saved_anon;
		args[w].wid = w;
		args[w].nw = nw;
		args[w].results = results;
		if (pthread_create(&th[w], NULL, ast_search_thread_fn, &args[w]) != 0) { MCC_TRACE("br\n");
			int j;
			for (j = 0; j < w; j++)
				{ MCC_TRACE("br\n"); pthread_join(th[j], NULL); }
			mcc_free(results);
			return 0;
		}
	}
	for (w = 0; w < nw; w++)
		{ MCC_TRACE("br\n"); pthread_join(th[w], NULL); }
	for (i = 0; i < nc; i++)
		{ MCC_TRACE("br\n"); if (results[i] >= 0 && (best_score < 0 || results[i] < best_score)) { MCC_TRACE("br\n");
			best_score = results[i];
			best = gatelist[i];
		} }
	mcc_free(results);
	if (best_score < 0)
		{ MCC_TRACE("br\n"); return 0; }
	*best_out = best;
	*best_score_out = best_score;
	return 1;
}
#endif

static void ast_search_select_order(Sym *sym, int faithful, int saved_loc,
																		int saved_anon, AstArena *pristine, uint64_t h) { MCC_TRACE("enter\n");
	int rows[AST_STRAT_COUNT_MAX], nrows = 0;
	int best_seq[AST_STRAT_COUNT_MAX], best_k;
	int i, si, g0, p0, o0;
	long best_score;
	char sq[AST_STRAT_COUNT_MAX * 4];
	for (si = 0; si < AST_STRAT_COUNT; si++)
		{ MCC_TRACE("br\n"); if (faithful && ast_strategies[si].gate())
			{ MCC_TRACE("br\n"); rows[nrows++] = si; } }
	if (h) { MCC_TRACE("br\n");
		for (i = 0; i < ast_search_memo_n; i++)
			{ MCC_TRACE("br\n"); if (ast_search_memo[i].hash == h) { MCC_TRACE("br\n");
				if (ast_search_memo[i].order_n > 0) { MCC_TRACE("br\n");
					int useq[AST_STRAT_COUNT_MAX], un = (int)ast_search_memo[i].order_n, j, kk = 0;
					if (un > AST_STRAT_COUNT_MAX)
						{ MCC_TRACE("br\n"); un = AST_STRAT_COUNT_MAX; }
					ast_order_unpack(ast_search_memo[i].order_packed, un, useq);
					for (j = 0; j < un; j++) { MCC_TRACE("br\n");
						int r = useq[j];
						if (r >= 0 && r < AST_STRAT_COUNT && ast_strategies[r].gate())
							{ MCC_TRACE("br\n"); ast_strat_order[kk++] = r; }
					}
					if (ast_search_fullset_env) { MCC_TRACE("br\n");
						int rj, mm;
						for (rj = 0; rj < nrows && kk < AST_STRAT_COUNT_MAX; rj++) { MCC_TRACE("br\n");
							int rr = rows[rj], present = 0;
							for (mm = 0; mm < kk; mm++)
								{ MCC_TRACE("br\n"); if (ast_strat_order[mm] == rr) { MCC_TRACE("br\n"); present = 1; break; } }
							if (!present)
								{ MCC_TRACE("br\n"); ast_strat_order[kk++] = rr; }
						}
					}
					ast_strat_order_n = kk;
					ast_order_seq_str(ast_strat_order, kk, sq);
					MCC_TRACE("memo hit order %s hash=%016llx n=%d seq=%s\n", funcname,
										(unsigned long long)h, kk, sq);
					ast_search_disk_store(ast_search_memo[i].hash, ast_search_memo[i].gates,
																ast_search_memo[i].refcount + 1,
																ast_search_memo[i].score, ast_search_memo[i].tried,
																ast_search_memo[i].order_packed,
																ast_search_memo[i].order_n);
					ast_arena_free(pristine);
					return;
				}
				break;
			} }
	}
	g0 = ast_graft_total;
	p0 = ast_promo_total;
	o0 = ast_opt_total;
	best_k = nrows;
	for (i = 0; i < nrows; i++)
		{ MCC_TRACE("br\n"); best_seq[i] = rows[i]; }
	best_score = ast_search_score_order(pristine, sym, faithful, best_seq, nrows,
																			saved_loc, saved_anon);
	if (nrows > 0 && !ast_search_should_stop()) { MCC_TRACE("br\n");
		ComboSpec spec;
		ComboBest cbest;
		AstOrderCtx cx;
		cx.pristine = pristine;
		cx.sym = sym;
		cx.faithful = faithful;
		cx.saved_loc = saved_loc;
		cx.saved_anon = saved_anon;
		cx.rows = rows;
		cx.tried = 0;
		cx.ord = 0;
		spec.nitems = nrows;
		spec.min_k = 1;
		spec.max_k = nrows;
		spec.ordered = 1;
		spec.walk = ast_search_walk_env;
		spec.budget = AST_SEARCH_MAX_CAND;
		spec.score = ast_search_order_combo_score;
		spec.visit = ast_search_walk_trace;
		spec.user = &cx;
		if (combo_run(&spec, &cbest) && cbest.score >= 0 && cbest.score < best_score) { MCC_TRACE("br\n");
			best_k = cbest.k;
			for (i = 0; i < cbest.k; i++)
				{ MCC_TRACE("br\n"); best_seq[i] = rows[cbest.sel[i]]; }
			best_score = cbest.score;
		}
	}
	ast_graft_total = g0;
	ast_promo_total = p0;
	ast_opt_total = o0;
	if (ast_search_fullset_env) { MCC_TRACE("br\n");
		int j, m;
		for (j = 0; j < nrows && best_k < AST_STRAT_COUNT_MAX; j++) { MCC_TRACE("br\n");
			int r = rows[j], present = 0;
			for (m = 0; m < best_k; m++)
				{ MCC_TRACE("br\n"); if (best_seq[m] == r) { MCC_TRACE("br\n"); present = 1; break; } }
			if (!present)
				{ MCC_TRACE("br\n"); best_seq[best_k++] = r; }
		}
	}
	for (i = 0; i < best_k; i++)
		{ MCC_TRACE("br\n"); ast_strat_order[i] = best_seq[i]; }
	ast_strat_order_n = best_k;
	ast_order_seq_str(best_seq, best_k, sq);
	MCC_TRACE("combo winner order n=%d seq=%s score=%ld nitems=%d walk=%s\n", best_k, sq,
						best_score, nrows, combo_walk_name(ast_search_walk_env));
	if (h) { MCC_TRACE("br\n");
		MCC_TRACE("memo store order %s hash=%016llx n=%d seq=%s score=%ld\n", funcname,
							(unsigned long long)h, best_k, sq, best_score);
		ast_search_disk_store(h, ast_search_gates_now(), 1, best_score, 0,
													ast_order_pack(best_seq, best_k),
													best_k <= AST_ORDER_MAXN ? (uint64_t)best_k : 0);
	}
	ast_arena_free(pristine);
}

static void ast_search_axis_pick(Sym *sym, int faithful, int saved_loc,
																 int saved_anon) { MCC_TRACE("enter\n");
	AstArena *saved_cur, *trial;
	int ci, first = 1, bi = 1;
	long best = -1;
	if (!ast_search_inline_env || !ast_search_emitiso_env ||
			!ast_search_emitsize_env || !faithful)
		{ MCC_TRACE("br\n"); return; }
	trial = ast_arena_clone(ast_cur);
	if (!trial)
		{ MCC_TRACE("br\n"); return; }
	saved_cur = ast_cur;
	ast_cur = trial;
	AstLtempSave ltsv;
	ast_ltemp_save(&ltsv);
	ast_run_strat_cycle(trial, sym, faithful, ast_strat_order, ast_strat_order_n,
											NULL);
	for (ci = 1; ci >= 0; ci--) { MCC_TRACE("br\n");
		long sz;
		ast_search_want_inline = ci;
		sz = ast_search_emit_size(trial, saved_loc, saved_anon);
		MCC_TRACE("emit-size inline=%d size=%ld\n", ci, sz);
		if (sz >= 0 && (first || sz < best)) { MCC_TRACE("br\n");
			best = sz;
			bi = ci;
			first = 0;
		}
	}
	ast_search_want_inline = 0;
	ast_ltemp_restore(&ltsv);
	ast_cur = saved_cur;
	ast_arena_free(trial);
	ast_search_pick_inline = bi;
	ast_search_axis_ran = 1;
	MCC_TRACE("search picks inline=%d\n", bi);
}

static unsigned long ast_search_searchable(unsigned long base) { MCC_TRACE("enter\n");
	return base | AST_SG_RANGE | AST_SG_DIVMAGIC | AST_SG_ABS | AST_SG_REASSOC |
				 AST_SG_REASSOC_ASSOC | AST_SG_REASSOC_SHLSHR | AST_SG_REASSOC_SHRSHL | AST_SG_REASSOC_MULDIST |
				 ((base & AST_SG_NARROW) ? (AST_SG_NARROWFIX | AST_SG_NARROW_C0 | AST_SG_NARROW_C1 | AST_SG_NARROW_C2 | AST_SG_NARROW_C3) : 0) |
				 ((base & AST_SG_SETHI) ? AST_SG_SETHILEAF : 0) |
				 ((base & AST_SG_TEMPLATES) ? (AST_SG_LTEMP | AST_SG_IVSR | AST_SG_PRE | AST_SG_DSECALL | AST_SG_TCOPTR | AST_SG_CSECOMM | AST_SG_SCCPFIX | AST_SG_IDENT_CONV | AST_SG_IDENT_SHIFT | AST_SG_IDENT_ARITH | AST_SG_IDENT_BIT | AST_SG_IDENT_REL | AST_SG_IDENT_URANGE | AST_SG_BFOLD_SQRT | AST_SG_BFOLD_SIGN | AST_SG_BFOLD_ROUND | AST_SG_BFOLD_MINMAX)
																			 : 0);
}

static void ast_slice_consume(void) { MCC_TRACE("enter\n");
	AstGateMask base, searchable, warm;
	uint64_t cached = 0;
	if (!ast_slice_flush_armed) { MCC_TRACE("br\n");
		ast_slice_flush_armed = 1;
		atexit(ast_slice_flush_atexit);
	}
	ast_slice_disk_load();
	if (ast_slice_disk_n <= 0)
		{ MCC_TRACE("br\n"); return; }
	base = ast_search_gates_now();
	searchable = ast_search_searchable(base);
	if (!ast_slice_probe_table_ex(ast_cur, ast_slice_disk, ast_slice_disk_n,
																(uint64_t)searchable, &cached, 0))
		{ MCC_TRACE("br\n"); return; }
	warm = (AstGateMask)cached & searchable;
	MCC_TRACE("slice warm-start gates=%llx&%llx->%llx\n", (unsigned long long)cached,
						(unsigned long long)searchable, (unsigned long long)warm);
	ast_search_gates_set(warm);
}

static void ast_search_roi_order(Sym *sym, int faithful, int saved_loc,
																 int saved_anon, AstArena *pristine) { MCC_TRACE("enter\n");
	int rows[AST_STRAT_COUNT_MAX], nrows = 0, i, j;
	long roi[AST_STRAT_COUNT_MAX], ben[AST_STRAT_COUNT_MAX];
	unsigned tim[AST_STRAT_COUNT_MAX];
	int g0 = ast_graft_total, p0 = ast_promo_total, o0 = ast_opt_total;
	for (i = 0; i < AST_STRAT_COUNT; i++)
		{ MCC_TRACE("br\n"); if (faithful && ast_strategies[i].gate())
			{ MCC_TRACE("br\n"); rows[nrows++] = i; } }
	if (nrows == 0)
		{ MCC_TRACE("br\n"); ast_arena_free(pristine); return; }
	for (i = 0; i < nrows; i++) { MCC_TRACE("br\n");
		AstArena *trial = ast_arena_clone(pristine), *saved_cur;
		long m0, m1, b, work;
		int wg, wp, wo;
		if (!trial)
			{ MCC_TRACE("br\n"); roi[i] = 0; ben[i] = 0; tim[i] = 0; continue; }
		saved_cur = ast_cur;
		ast_cur = trial;
		(void)saved_loc; (void)saved_anon;
		m0 = ast_cost_score(trial);
		wg = ast_graft_total; wp = ast_promo_total; wo = ast_opt_total;
		(void)ast_strategies[rows[i]].apply(trial, sym);
		work = (long)(ast_graft_total - wg) + (ast_promo_total - wp) + (ast_opt_total - wo);
		if (work < 0)
			{ MCC_TRACE("br\n"); work = 0; }
		tim[i] = (unsigned)work;
		m1 = ast_cost_score(trial);
		ast_cur = saved_cur;
		ast_arena_free(trial);
		b = m0 - m1;
		if (b < 0)
			{ MCC_TRACE("br\n"); b = 0; }
		ben[i] = b;
		roi[i] = (b * 1000L) / (long)(tim[i] + 1u);
	}
	ast_graft_total = g0;
	ast_promo_total = p0;
	ast_opt_total = o0;
	for (i = 1; i < nrows; i++) { MCC_TRACE("br\n");
		int kr = rows[i];
		long kv = roi[i], kb = ben[i];
		unsigned kt = tim[i];
		for (j = i - 1; j >= 0 && roi[j] < kv; j--) { MCC_TRACE("br\n");
			rows[j + 1] = rows[j];
			roi[j + 1] = roi[j];
			ben[j + 1] = ben[j];
			tim[j + 1] = tim[j];
		}
		rows[j + 1] = kr;
		roi[j + 1] = kv;
		ben[j + 1] = kb;
		tim[j + 1] = kt;
	}
	for (i = 0; i < nrows; i++)
		{ MCC_TRACE("br\n"); ast_strat_order[i] = rows[i]; }
	ast_strat_order_n = nrows;
	if (ast_roi_dump) { MCC_TRACE("br\n");
		fprintf(stderr, "[roi] %s:", funcname ? funcname : "?");
		for (i = 0; i < nrows; i++)
			{ MCC_TRACE("br\n"); fprintf(stderr, " %s(roi=%ld b=%ld cost=%u)",
									ast_strategies[rows[i]].name, roi[i], ben[i], tim[i]); }
		fprintf(stderr, "\n");
	}
	ast_arena_free(pristine);
}

#define AST_SEARCH_PREDICT_TOP 4
#define AST_SEARCH_PREDICT_WANT 4

static void ast_search_predict_round(AstArena *pristine, Sym *sym, int faithful,
																		 int saved_loc, int saved_anon,
																		 AstGateMask round_base,
																		 long round_base_score,
																		 const AstGateMask *items,
																		 const long *idelta, int nitems,
																		 AstGateMask *best,
																		 long *best_score) { MCC_TRACE("enter\n");
	SurroObs obs[1 + SURRO_MAXN + (AST_SEARCH_PREDICT_TOP *
																 (AST_SEARCH_PREDICT_TOP - 1)) / 2];
	SurroFit fit;
	SurroProp prop;
	int top[AST_SEARCH_PREDICT_TOP];
	int n = nitems > SURRO_MAXN ? SURRO_MAXN : nitems;
	int nobs = 0, ntop = 0, i, j;

	if (!ast_search_predict_env || n < 3 || round_base_score < 0)
		{ MCC_TRACE("br\n"); return; }
	obs[nobs].mask = 0u;
	obs[nobs].score = round_base_score;
	nobs++;
	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		if (idelta[i] == LONG_MAX)
			{ MCC_TRACE("br\n"); continue; }
		obs[nobs].mask = (uint32_t)1 << i;
		obs[nobs].score = idelta[i];
		nobs++;
	}
	if (nobs < 4)
		{ MCC_TRACE("br\n"); return; }

	for (i = 0; i < n; i++) { MCC_TRACE("br\n");
		long mi = idelta[i] - round_base_score;
		if (idelta[i] == LONG_MAX)
			{ MCC_TRACE("br\n"); continue; }
		if (mi < 0)
			{ MCC_TRACE("br\n"); mi = -mi; }
		if (mi == 0)
			{ MCC_TRACE("br\n"); continue; }
		if (ntop < AST_SEARCH_PREDICT_TOP)
			{ MCC_TRACE("br\n"); top[ntop++] = i; }
		else { MCC_TRACE("br\n");
			int w = 0;
			long mw;
			for (j = 1; j < ntop; j++) { MCC_TRACE("br\n");
				long ma = idelta[top[j]] - round_base_score;
				long mb = idelta[top[w]] - round_base_score;
				if (ma < 0)
					{ MCC_TRACE("br\n"); ma = -ma; }
				if (mb < 0)
					{ MCC_TRACE("br\n"); mb = -mb; }
				if (ma < mb)
					{ MCC_TRACE("br\n"); w = j; }
			}
			mw = idelta[top[w]] - round_base_score;
			if (mw < 0)
				{ MCC_TRACE("br\n"); mw = -mw; }
			if (mi > mw)
				{ MCC_TRACE("br\n"); top[w] = i; }
		}
	}
	if (ntop < 2)
		{ MCC_TRACE("br\n"); return; }
	for (i = 1; i < ntop; i++) { MCC_TRACE("br\n");
		int k = top[i];
		for (j = i - 1; j >= 0 && top[j] > k; j--)
			{ MCC_TRACE("br\n"); top[j + 1] = top[j]; }
		top[j + 1] = k;
	}

	for (i = 0; i < ntop; i++)
		{ MCC_TRACE("br\n"); for (j = i + 1; j < ntop; j++) { MCC_TRACE("br\n");
			AstGateMask cand;
			long sc;
			if (ast_search_should_stop())
				{ MCC_TRACE("br\n"); goto fit_now; }
			cand = round_base ^ items[top[i]] ^ items[top[j]];
			sc = ast_search_score_one(pristine, sym, faithful, cand, saved_loc,
																saved_anon);
			if (sc < 0)
				{ MCC_TRACE("br\n"); continue; }
			obs[nobs].mask = ((uint32_t)1 << top[i]) | ((uint32_t)1 << top[j]);
			obs[nobs].score = sc;
			nobs++;
			if (*best_score < 0 || sc < *best_score) { MCC_TRACE("br\n");
				*best = cand;
				*best_score = sc;
			}
		} }

fit_now:
	surro_fit(obs, nobs, n, 0u, &fit);
	if (ast_search_verbose_env) { MCC_TRACE("br\n");
		int64_t maxd = 0, maxg = 0;
		int a, b;
		for (a = 0; a < n; a++) { MCC_TRACE("br\n");
			int64_t v = fit.d1[a] < 0 ? -fit.d1[a] : fit.d1[a];
			if (fit.have1[a] && v > maxd)
				{ MCC_TRACE("br\n"); maxd = v; }
			for (b = a + 1; b < n; b++) { MCC_TRACE("br\n");
				int64_t w = fit.d2[a][b] < 0 ? -fit.d2[a][b] : fit.d2[a][b];
				if (fit.have2[a][b] && w > maxg)
					{ MCC_TRACE("br\n"); maxg = w; }
			}
		}
		fprintf(stderr,
						"[search] anova n=%d obs=%d main=%d pair=%d max|d|=%lld "
						"max|g|=%lld\n",
						n, nobs, fit.known1, fit.known2, (long long)maxd, (long long)maxg);
	}
	if (!fit.have_base || fit.known2 == 0)
		{ MCC_TRACE("br\n"); return; }
	if (!surro_propose(&fit, obs, nobs, (uint32_t)((1u << n) - 1u), &prop,
										 AST_SEARCH_PREDICT_WANT))
		{ MCC_TRACE("br\n"); return; }
	for (i = 0; i < prop.n; i++) { MCC_TRACE("br\n");
		AstGateMask cand = round_base;
		long sc;
		if (ast_search_should_stop())
			{ MCC_TRACE("br\n"); return; }
		for (j = 0; j < n; j++)
			{ MCC_TRACE("br\n"); if ((prop.mask[i] >> j) & 1u)
				{ MCC_TRACE("br\n"); cand ^= items[j]; } }
		sc = ast_search_score_one(pristine, sym, faithful, cand, saved_loc,
															saved_anon);
		ast_search_predict_tries++;
		if (sc < 0)
			{ MCC_TRACE("br\n"); continue; }
		if (*best_score < 0 || sc < *best_score) { MCC_TRACE("br\n");
			ast_search_predict_hits++;
			MCC_TRACE("predict hit mask=%x pred=%lld actual=%ld prev=%ld\n",
								prop.mask[i], (long long)prop.pred[i], sc, *best_score);
			*best = cand;
			*best_score = sc;
		}
	}
}

static void ast_search_select(Sym *sym, int faithful, int saved_loc,
															int saved_anon) { MCC_TRACE("enter\n");
	AstArena *pristine;
	uint64_t h;
	AstGateMask base, best, searchable;
	long best_score = -1;
	long base_cost = -1;
	uint64_t tried_mask = 0;
	uint64_t resume_skip = 0;
	AstGateMask resume_best = 0;
	int resume_active = 0;
	int search_complete = 0;
	int g0, p0, o0, nc = 0;
	AstGateMask gatelist[AST_SEARCH_MAX_CAND];
	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_search_enter(); }
	if (!ast_search_started) { MCC_TRACE("br\n");
		ast_search_started = 1;
		if (ast_search_verbose_env)
			{ MCC_TRACE("br\n"); atexit(ast_search_evals_report); }
		ast_search_start_ms = ast_now_ms();
		ast_search_budget_ms = mcc_search_cap_ms();
		ast_search_eval_quota =
				(unsigned long)ast_search_ticks *
				mcc_env_count("MCC_SEARCH_TU_EVALS", MCC_OPT_SEARCH_TU_EVALS);
		ast_search_disk_load();
	}
	base = ast_search_gates_now();
	searchable = ast_search_searchable(base);
	if (ast_search_should_stop())
		{ MCC_TRACE("br\n"); return; }
	pristine = ast_arena_clone(ast_cur);
	if (!pristine)
		{ MCC_TRACE("br\n"); return; }
	ast_isa_key_update(pristine);
	h = ast_intention_hash(pristine, AST_NONE);
	if (h)
		{ MCC_TRACE("br\n"); h = ast_search_key_salt(h); }
#ifdef MCC_EMBED_JIT
	if (h) { MCC_TRACE("br\n");
		const JitGraduatedRecord *gr =
				jit_graduated_find(h, ast_slice_key_salt(0xcbf29ce484222325ULL));
		if (gr) { MCC_TRACE("br\n");
			MCC_TRACE("jit-graduated hit %s hash=%016llx gates=%llx\n", funcname,
								(unsigned long long)h,
								(unsigned long long)(gr->gate_mask & searchable));
			ast_search_gates_set(gr->gate_mask & searchable);
			ast_arena_free(pristine);
			return;
		}
	}
#endif
	if (ast_search_order_env) { MCC_TRACE("br\n");
		ast_search_select_order(sym, faithful, saved_loc, saved_anon, pristine, h);
		return;
	}
	if (h) { MCC_TRACE("br\n");
		for (int i = 0; i < ast_search_memo_n; i++)
			{ MCC_TRACE("br\n"); if (ast_search_memo[i].hash == h) { MCC_TRACE("br\n");
				MCC_TRACE("memo hit %s hash=%016llx gates=%llx&%llx->%llx refcount=%u->%u complete=%llu\n",
									funcname, (unsigned long long)h,
									(unsigned long long)ast_search_memo[i].gates,
									(unsigned long long)searchable,
									(unsigned long long)(ast_search_memo[i].gates & searchable),
									ast_search_memo[i].refcount, ast_search_memo[i].refcount + 1,
									(unsigned long long)ast_search_memo[i].order_n);
				if (ast_search_memo[i].order_n == 0 && !ast_search_should_stop()) { MCC_TRACE("br\n");
					resume_active = 1;
					resume_best = ast_search_memo[i].gates;
					resume_skip = ast_search_memo[i].tried;
					MCC_TRACE("search continue %s hash=%016llx seed-gates=%llx tried=%llx\n",
										funcname, (unsigned long long)h,
										(unsigned long long)resume_best, (unsigned long long)resume_skip);
					if (ast_search_verbose_env)
						{ MCC_TRACE("br\n"); fprintf(stderr,
							"[search] continue %s: seed=0x%llx already-tried=0x%llx\n", funcname,
							(unsigned long long)resume_best, (unsigned long long)resume_skip); }
					break;
				}
				if (mcc_stats_mask)
					{ MCC_TRACE("br\n"); mcc_stats_search_memo(1); }
				ast_search_gates_set(ast_search_memo[i].gates & searchable);
				ast_search_disk_store(ast_search_memo[i].hash, ast_search_memo[i].gates,
															ast_search_memo[i].refcount + 1,
															ast_search_memo[i].score, ast_search_memo[i].tried,
															ast_search_memo[i].order_packed,
															ast_search_memo[i].order_n);
				ast_arena_free(pristine);
				return;
			} }
	}
	if (h && mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_search_memo(0); }
	g0 = ast_graft_total;
	p0 = ast_promo_total;
	o0 = ast_opt_total;
	best = base;
	{
		AstGateMask items[64];
		int nitems = 0, b;
		for (b = 0; b < 64; b++)
			{ MCC_TRACE("br\n"); if (searchable & ((AstGateMask)1 << b))
				{ MCC_TRACE("br\n"); items[nitems++] = (AstGateMask)1 << b; } }
		if (mcc_stats_mask)
			{ MCC_TRACE("br\n"); mcc_stats_search_begin(funcname, h, base, searchable, nitems,
														 ast_search_walk_env, ast_search_ordered_env ? 1 : 0); }
#if MCC_HOST_POSIX
		if ((ast_search_threads_env || ast_search_pthreads_env) &&
				mcc_env_on("MCC_OPT_SEARCH_THREADS_UNSAFE")) { MCC_TRACE("br\n");
			AstGateMask sub = searchable;
			int pooled;
			for (;;) { MCC_TRACE("br\n");
				if (nc < AST_SEARCH_MAX_CAND)
					{ MCC_TRACE("br\n"); gatelist[nc++] = sub; }
				else { MCC_TRACE("br\n");
					MCC_TRACE("fork pool: submask space > %d, capped (budget)\n",
										AST_SEARCH_MAX_CAND);
					break;
				}
				if (sub == 0)
					{ MCC_TRACE("br\n"); break; }
				sub = (sub - 1) & searchable;
			}
			pooled = ast_search_pthreads_env
									 ? ast_search_pool_pthreads(pristine, sym, faithful, gatelist, nc,
																							saved_loc, saved_anon, &best, &best_score)
									 : ast_search_pool(pristine, sym, faithful, gatelist, nc, saved_loc,
																		 saved_anon, &best, &best_score);
			if (pooled)
				{ MCC_TRACE("br\n"); goto search_done; }
			best_score = -1;
		}
#endif
		if (best_score < 0) { MCC_TRACE("br\n");
			ComboSpec spec;
			ComboBest cbest;
			AstComboCtx cx;
			cx.pristine = pristine;
			cx.sym = sym;
			cx.faithful = faithful;
			cx.saved_loc = saved_loc;
			cx.saved_anon = saved_anon;
			unsigned tick, nticks = ast_search_ticks ? ast_search_ticks : 1u;
			AstGateMask round_base;
			int all_rounds_exhausted = 1;
			cx.items = items;
			cx.tried = 0;
			cx.skip = resume_active ? resume_skip : 0;
			cx.ord = 0;
			cx.best_score = -1;
			cbest.k = 0;
			cbest.score = 0;
			cbest.evaluated = 0;
			cbest.exhausted = 1;
			spec.nitems = nitems;
			spec.min_k = 1;
			spec.max_k = nitems;
			spec.ordered = ast_search_ordered_env ? 1 : 0;
			spec.walk = ast_search_walk_env;
			spec.budget = AST_SEARCH_CAND_MAX;
			spec.score = ast_search_combo_score;
			spec.visit = ast_search_walk_trace;
			spec.user = &cx;
			best = base;
			best_score = ast_search_score_one(pristine, sym, faithful, base, saved_loc,
																				saved_anon);
			base_cost = best_score;
			if (resume_active) { MCC_TRACE("br\n");
				AstGateMask rg = resume_best & searchable;
				long rs = ast_search_score_one(pristine, sym, faithful, rg, saved_loc,
																			 saved_anon);
				if (rs >= 0 && (best_score < 0 || rs < best_score)) { MCC_TRACE("br\n");
					best = rg;
					best_score = rs;
				}
			}
			round_base = best;
			for (tick = 0; tick < nticks; tick++) { MCC_TRACE("br\n");
			long round_entry_score = best_score;
			cx.tried = 0;
			cx.skip = (tick == 0 && resume_active) ? resume_skip : 0;
			cx.ord = 0;
			cx.best_score = -1;
			if (nitems > 6) { MCC_TRACE("br\n");
				long idelta[64];
				int i, j;
				for (i = 0; i < nitems; i++) { MCC_TRACE("br\n");
					AstGateMask cand = round_base ^ items[i];
					long sc;
					if (ast_search_should_stop()) { MCC_TRACE("br\n");
						idelta[i] = LONG_MAX;
						continue;
					}
					sc = ast_search_score_one(pristine, sym, faithful, cand, saved_loc,
																		saved_anon);
					idelta[i] = (sc < 0) ? LONG_MAX : sc;
					if (sc >= 0 && (best_score < 0 || sc < best_score)) { MCC_TRACE("br\n");
						best = cand;
						best_score = sc;
					}
				}
				ast_search_predict_round(pristine, sym, faithful, saved_loc, saved_anon,
																round_base, round_entry_score, items, idelta,
																nitems, &best, &best_score);
				for (i = 1; i < nitems; i++) { MCC_TRACE("br\n");
					AstGateMask ki = items[i];
					long kd = idelta[i];
					for (j = i - 1; j >= 0 && idelta[j] > kd; j--) { MCC_TRACE("br\n");
						items[j + 1] = items[j];
						idelta[j + 1] = idelta[j];
					}
					items[j + 1] = ki;
					idelta[j + 1] = kd;
				}
			}
			if (nitems > COMBO_MAX)
				{ MCC_TRACE("br\n"); MCC_TRACE("combo enum clamped nitems=%d -> COMBO_MAX=%d (frontier scored "
									"every single-toggle; only combinations of the %d least-improving "
									"knobs are dropped)\n",
									nitems, COMBO_MAX, nitems - COMBO_MAX); }
			if (combo_run(&spec, &cbest)) { MCC_TRACE("br\n");
				AstGateMask g = 0;
				int i;
				for (i = 0; i < cbest.k; i++)
					{ MCC_TRACE("br\n"); g |= items[cbest.sel[i]]; }
				if (cbest.score >= 0 && (best_score < 0 || cbest.score < best_score)) { MCC_TRACE("br\n");
					best = g;
					best_score = cbest.score;
				}
			}
			tried_mask = cx.tried;
			if (!cbest.exhausted)
				{ MCC_TRACE("br\n"); all_rounds_exhausted = 0; }
			MCC_TRACE("search tick %u/%u %s base=%llx best=%llx score=%ld->%ld "
								"evaluated=%ld exhausted=%d\n",
								tick + 1u, nticks, funcname, (unsigned long long)round_base,
								(unsigned long long)best, round_entry_score, best_score,
								cbest.evaluated, cbest.exhausted);
			if (nitems <= 6 && cbest.exhausted)
				{ MCC_TRACE("br\n"); break; }
			if (best_score < 0 || (round_entry_score >= 0 && best_score >= round_entry_score))
				{ MCC_TRACE("br\n"); break; }
			round_base = best;
			}
			{
				long z = ast_search_score_one(pristine, sym, faithful, 0, saved_loc,
																			saved_anon);
				if (z >= 0 && best_score >= 0 && z < best_score) { MCC_TRACE("br\n");
					best = 0;
					best_score = z;
				}
			}
			search_complete = (all_rounds_exhausted ||
												 cbest.evaluated >= spec.budget) &&
												!ast_search_should_stop();
			MCC_TRACE("combo winner gates=%llx base=%llx searchable=%llx score=%ld "
								"ordered=%d nitems=%d tried=%llx\n",
								(unsigned long long)best, (unsigned long long)base,
								(unsigned long long)searchable, best_score, spec.ordered, nitems,
								(unsigned long long)tried_mask);
		}
	}
search_done:
	ast_graft_total = g0;
	ast_promo_total = p0;
	ast_opt_total = o0;
	ast_search_gates_set(best);
	if (mcc_stats_mask)
		{ MCC_TRACE("br\n"); mcc_stats_search_end(best, best_score, base_cost, (long)nc,
																								ast_search_memo_n); }
	if (h)
		{ MCC_TRACE("br\n"); ast_search_disk_store(h, best, 1, best_score, tried_mask, 0,
																								search_complete ? 1 : 0);
			if (ast_search_verbose_env)
				{ MCC_TRACE("br\n"); fprintf(stderr,
					"[search] store %s: gates=0x%llx score=%ld tried=0x%llx %s%s\n", funcname,
					(unsigned long long)best, best_score, (unsigned long long)tried_mask,
					search_complete ? "COMPLETE" : "incomplete",
					resume_active ? " (continued)" : ""); } }
	ast_arena_free(pristine);
}

#ifdef MCC_EMBED_JIT
static void ast_jit_submit_aot(Sym *sym) { MCC_TRACE("enter\n");
	if (!sym || !ast_cur || !mcc_env_on("MCC_JIT_SUBMIT_AOT"))
		{ MCC_TRACE("br\n"); return; }
	if (mcc_jit_submit_ast(sym, ast_cur, (uint64_t)ast_search_gates_now(), 0) == 0 &&
			mcc_env_on("MCC_JIT_VERBOSE"))
		{ MCC_TRACE("br\n"); fprintf(stderr, "mccjit-aot-submit[%s]: submitted sym->v=%ld\n",
						funcname, (long)sym->v); }
}
#endif

static FILE *ast_refcensus_fp;
static int ast_refcensus_slice;

static FILE *ast_refcensus_out(void) { MCC_TRACE("enter\n");
	if (ast_refcensus_fp)
		{ MCC_TRACE("br\n"); return ast_refcensus_fp; }
	if (!ast_refcensus_path[0] || !strcmp(ast_refcensus_path, "1") ||
			!strcmp(ast_refcensus_path, "-"))
		{ MCC_TRACE("br\n"); ast_refcensus_fp = stderr; }
	else
		{ MCC_TRACE("br\n"); ast_refcensus_fp = fopen(ast_refcensus_path, "a"); }
	if (!ast_refcensus_fp)
		{ MCC_TRACE("br\n"); ast_refcensus_fp = stderr; }
	return ast_refcensus_fp;
}

static int ast_refcensus_size(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	int k = 1;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); k += ast_refcensus_size(a, c); }
	return k;
}

static int ast_refcensus_hascall(const AstArena *a, AstLocal n) { MCC_TRACE("enter\n");
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return 0; }
	if (ast_kind(a, n) == AST_Invoke)
		{ MCC_TRACE("br\n"); return 1; }
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); if (ast_refcensus_hascall(a, c))
			{ MCC_TRACE("br\n"); return 1; } }
	return 0;
}

static const char *ast_refcensus_class(AstArena *a, AstLocal n, int *off) { MCC_TRACE("enter\n");
	int r = ast_op(a, n), t = ast_type_t(a, n), vm = r & VT_VALMASK;
	*off = 0;
	if (r & VT_SYM)
		{ MCC_TRACE("br\n"); return "sym"; }
	if (vm == VT_CONST)
		{ MCC_TRACE("br\n"); return "const"; }
	if (vm == VT_LLOCAL)
		{ MCC_TRACE("br\n"); *off = (int)(int64_t)ast_ival(a, n); return "llocal"; }
	if (vm != VT_LOCAL)
		{ MCC_TRACE("br\n"); return "reg"; }
	*off = (int)(int64_t)ast_ival(a, n);
	if (t & VT_VOLATILE)
		{ MCC_TRACE("br\n"); return "lvol"; }
	if ((t & (VT_ARRAY | VT_VLA)) || (t & VT_BTYPE) == VT_STRUCT ||
			(t & VT_BTYPE) == VT_FUNC)
		{ MCC_TRACE("br\n"); return "lagg"; }
	if (t & VT_BITFIELD)
		{ MCC_TRACE("br\n"); return "lbf"; }
	if (ast_cprop_escapes(a, *off))
		{ MCC_TRACE("br\n"); return "laddr"; }
	return "lval";
}

static void ast_refcensus_walk(AstArena *a, AstLocal n, int cfdepth, int sid,
															 const char *fn, FILE *f) { MCC_TRACE("enter\n");
	AstLocal c;
	int cf = cfdepth;
	if (n == AST_NONE)
		{ MCC_TRACE("br\n"); return; }
	if (!cf && !ast_refcensus_hascall(a, n)) { MCC_TRACE("br\n");
		cf = ast_refcensus_size(a, n);
		sid = ++ast_refcensus_slice;
	}
	if (ast_kind(a, n) == AST_Ref) { MCC_TRACE("br\n");
		int off = 0;
		const char *cl = ast_refcensus_class(a, n, &off);
		AstLocal p = ast_parent(a, n);
		int pk = p == AST_NONE ? -1 : (int)ast_kind(a, p);
		int isdef = p != AST_NONE && ast_kind(a, p) == AST_Store &&
								ast_first_child(a, p) == n;
		fprintf(f, "R\t%s\t%s\t%d\t%#x\t%#x\t%d\t%d\t%d\t%d\n", fn, cl, off,
						(unsigned)ast_op(a, n), (unsigned)ast_type_t(a, n), cf, isdef, pk,
						sid);
	}
	for (c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		{ MCC_TRACE("br\n"); ast_refcensus_walk(a, c, cf, sid, fn, f); }
}

static void ast_refcensus(AstArena *a, const char *fn) { MCC_TRACE("enter\n");
	FILE *f = ast_refcensus_out();
	AstLocal nn = ast_count(a);
	int refs = 0, inv = 0;
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		if (ast_kind(a, n) == AST_Ref)
			{ MCC_TRACE("br\n"); refs++; }
		else if (ast_kind(a, n) == AST_Invoke)
			{ MCC_TRACE("br\n"); inv++; }
	}
	fprintf(f, "F\t%s\t%u\t%d\t%d\t%d\n", fn ? fn : "?", (unsigned)nn, refs, inv,
					loc);
	ast_refcensus_walk(a, ast_root(a), 0, 0, fn ? fn : "?", f);
	fflush(f);
}

#if MCC_HOST_WIN32 && defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O0")))
#endif
void ast_func_end(Sym *sym) { MCC_TRACE("enter\n");
	MCC_TRACE("%s\n", funcname);
	ir_cap_active = 0;
	AstArena *ast_rir_prod = NULL;
	int ast_rir_used = 0;
	if (rir_started) { MCC_TRACE("br\n");
		ir_cap_gap();
		if (rir_prod_env) { MCC_TRACE("br\n");
			ast_rir_prod = rir_prod_take();
			if (!ast_rir_prod)
				{ MCC_TRACE("br\n"); rir_prod_body_set((long)(ind - ast_body_ind_sv));
					rir_prod_note("nomodel"); }
		}
		if (rir_env)
			{ MCC_TRACE("br\n"); rir_verify(); }
		rir_started = 0;
	}
	rir_active = 0;
	if (ast_rir_prod && !rir_try_active) { MCC_TRACE("br\n");
		ast_arena_free(ast_rir_prod);
		ast_rir_prod = NULL;
	}
	if (rir_try_active) { MCC_TRACE("br\n");
		Section *ast_rsec = cur_text_section->reloc;
		addr_t ast_reloc1 = ast_rsec ? ast_rsec->data_offset : 0;
		ast_active = 0;
		ast_fn_faithful = 0;
		ast_fn_tco = 0;
		int ast_rir_arena = 0;
		if (rir_prod_env) { MCC_TRACE("br\n");
			ast_arena_free(ast_cur);
			if (ast_rir_prod) { MCC_TRACE("br\n");
				ast_cur = ast_rir_prod;
				ast_rir_used = 1;
				ast_rir_arena = 1;
				ast_rir_prod = NULL;
			} else { MCC_TRACE("br\n");
				ast_cur = ast_arena_new();
				ast_node(ast_cur, AST_BasicBlock);
			}
			if (rir_prod_low_env && ast_rir_arena)
				{ MCC_TRACE("br\n"); ast_low_census(ast_cur); }
		}
		{ MCC_TRACE("br\n");
			const char *pd0 = ast_rirproddump_cached;
			if (pd0 && funcname && !strcmp(pd0, funcname)) { MCC_TRACE("br\n");
				static char pdb0[8192];
				ast_dump(ast_cur, ast_root(ast_cur), pdb0, sizeof pdb0);
				fprintf(stderr, "[ast-handover] %s:\n%s\n", funcname, pdb0);
			}
		}
		uint64_t ast_fnh = ast_intention_hash(ast_cur, AST_NONE);
		ast_intention_acc = ast_intention_acc * 0x100000001b3u ^ ast_fnh;
		ast_hash_out_emit(NULL, funcname, ast_fnh);
		if (ast_cost_env)
			{ MCC_TRACE("br\n"); ast_fn_cost(ast_cur, funcname); }
		if (ast_bitflag_report_env && !ast_search_worker)
			{ MCC_TRACE("br\n"); ast_bf_report(ast_cur, funcname); }
		if (ast_loopnest_dump_env)
			{ MCC_TRACE("br\n"); ast_loopnest_dump(ast_cur, funcname); }
		if (ast_loopdep_dump_env)
			{ MCC_TRACE("br\n"); ast_loopdep_dump(ast_cur, funcname, "pre-opt"); }
		ast_loop_par_census(ast_cur);
		ast_thread_census(ast_cur, funcname);
		if (ast_refcensus_path)
			{ MCC_TRACE("br\n"); ast_refcensus(ast_cur, funcname); }
		int ast_sv_tmpl = ast_templates_env, ast_sv_promo = ast_promote_env,
				ast_sv_inl = ast_inline_env;
		if (ast_fncfg_n) { MCC_TRACE("br\n");
			int fi = ast_fncfg_find(funcname);
			if (fi >= 0) { MCC_TRACE("br\n");
				ast_templates_env = ast_fncfg[fi].tmpl;
				ast_promote_env = ast_fncfg[fi].promo;
				ast_inline_env = ast_fncfg[fi].inl;
			}
		}
		{ MCC_TRACE("br\n");
			const char *pd1 = ast_rirproddump_cached;
			if (pd1 && funcname && !strcmp(pd1, funcname)) { MCC_TRACE("br\n");
				static char pdb1[8192];
				ast_dump(ast_cur, ast_root(ast_cur), pdb1, sizeof pdb1);
				fprintf(stderr, "[ast-mid] %s arena=%p root=%d count=%u:\n%s\n", funcname, (void *)ast_cur, (int)ast_root(ast_cur), (unsigned)ast_count(ast_cur), pdb1);
			}
		}
		int keep_baseline = 0;
		if (ast_replay_ok(ast_cur)) { MCC_TRACE("br\n");
			int orig_ind = ind, orig_rsym = rsym;
			int body_len = orig_ind - ast_body_ind_sv;
			unsigned char *orig = mcc_malloc(body_len > 0 ? body_len : 1);
			memcpy(orig, cur_text_section->data + ast_body_ind_sv, body_len);

			addr_t rel_len = ast_reloc1 - ast_reloc0_sv;
			unsigned char *orig_rel = mcc_malloc(rel_len > 0 ? rel_len : 1);
			if (rel_len)
				{ MCC_TRACE("br\n"); memcpy(orig_rel, ast_rsec->data + ast_reloc0_sv, rel_len); }

			ind = ast_body_ind_sv;
			rsym = 0;
			if (ast_rsec)
				{ MCC_TRACE("br\n"); ast_rsec->data_offset = ast_reloc0_sv; }
			nocode_wanted = 0;
			unsigned char ast_sv_warn = mcc_state->warn_none;
			mcc_state->warn_none = 1;
			ast_rp_bsym = ast_rp_csym = NULL;
			ast_tmpl_folds = 0;
			if (ast_templates_env)
				{ MCC_TRACE("br\n"); ast_run_templates(ast_cur); }
			ast_fconst_i = 0;
			ast_locrec_i = 0;
			ast_temp_frontier = 1;
			ast_replaying = 1;
			ast_rp_switch = NULL;
			ast_rp_nlabel = 0;
			Sym *ast_saved_free = sym_free_first;
			sym_free_first = NULL;
			int saved_loc = loc, saved_anon = anon_sym;
			int saved_func_alloca = mcc_state->cg_func_alloca;
			Section *rsec2 = cur_text_section->reloc;
			volatile int faithful = 0;
			volatile int ast_inv_verdict = 0;
			volatile int ast_replay_completed = 0;
			const char *volatile ast_unf_why = "abort";
			const int ast_fn_hole = ast_arena_has_hole(ast_cur);
			const int ast_fn_alloca = ast_body_uses_func_alloca();
			AstAsmEff ast_fn_asm;
			const int ast_fn_has_asm = ast_body_asm_eff(ast_cur, &ast_fn_asm);
			ast_fn_asm_live = ast_fn_has_asm;
			if (ast_fn_has_asm && ast_env_int("MCC_ASM_EFF", 0)) { MCC_TRACE("br\n");
				fprintf(stderr,
								"[asm-eff] %s reads=%llx writes=%llx clob=%llx vol=%d goto=%d "
								"mem=%d cc=%d x87=%d ccout=%d labels=%d unknown=%d admit=%d\n",
								funcname ? funcname : "?",
								(unsigned long long)ast_fn_asm.reads,
								(unsigned long long)ast_fn_asm.writes,
								(unsigned long long)ast_fn_asm.clobbers,
								!!(ast_fn_asm.eff & MCC_ASM_EFF_VOLATILE),
								!!(ast_fn_asm.eff & MCC_ASM_EFF_GOTO),
								!!(ast_fn_asm.eff & MCC_ASM_EFF_MEM),
								!!(ast_fn_asm.eff & MCC_ASM_EFF_CC),
								!!(ast_fn_asm.eff & MCC_ASM_EFF_X87),
								!!(ast_fn_asm.eff & MCC_ASM_EFF_CCOUT),
								ast_fn_asm.nlabels, ast_fn_asm.unknown,
								!ast_fn_asm.unknown && !ast_arena_has_dangle(ast_cur));
			}
			int promoted = 0;
			ast_search_axis_ran = 0;
			int bfolds = 0;
			int idents = 0;
			int cprops = 0;
			int cses = 0;
			int licms = 0;
			int dses = 0;
			int sccps = 0;
			int jts = 0;
			int bfs = 0;
			int sethis = 0;
			int tcos = 0;
			int narrows = 0;
			int divmagics = 0;
			int cloads = 0, sras = 0, unread = 0;
			int selects = 0;
			int interchanged = 0;
			int fused = 0;
			int tiled = 0;
			int unrolled = 0;
			int math_inlined = 0;
			int bswapped = 0;
			int vectorized = 0;
			jmp_buf ast_outer_jmp;
			int ast_outer_en = mcc_state->error_set_jmp_enabled;
			int ast_saved_nberr = mcc_state->nb_errors;
			void (*ast_sv_efunc)(void *, const char *) = mcc_state->error_func;
			void *ast_sv_eopaque = mcc_state->error_opaque;
			int ast_saved_floor = stk_data_floor;
			memcpy(ast_outer_jmp, mcc_state->error_jmp_buf, sizeof(jmp_buf));
			mcc_state->error_func = ast_error_sink;
			stk_data_floor = nb_stk_data;
			rir_prod_span(-1, -1, -1, -1);
			if (setjmp(mcc_state->error_jmp_buf) == 0) { MCC_TRACE("br\n");
				mcc_state->error_set_jmp_enabled = 1;
				{ MCC_TRACE("br\n");
					const char *pd2 = ast_rirproddump_cached;
					if (pd2 && funcname && !strcmp(pd2, funcname)) { MCC_TRACE("br\n");
						static char pdb3[8192];
						ast_dump(ast_cur, ast_root(ast_cur), pdb3, sizeof pdb3);
						fprintf(stderr, "[ast-injmp] %s arena=%p root=%d count=%u: body_len=%d rel_len=%llu reloc1=%llu reloc0=%llu\n%s\n",
										funcname, (void *)ast_cur, (int)ast_root(ast_cur), (unsigned)ast_count(ast_cur), body_len, (unsigned long long)rel_len,
										(unsigned long long)ast_reloc1,
										(unsigned long long)ast_reloc0_sv, pdb3);
					}
				}
				ast_promo_n = 0;
				ast_pinned_regs = 0;
				if (ast_rir_used)
					{ MCC_TRACE("br\n"); rir_prod_replay_begin(); }
				{ MCC_TRACE("br\n");
					const char *pd = ast_rirproddump_cached;
					if (pd && funcname && !strcmp(pd, funcname)) { MCC_TRACE("br\n");
						static char pdb2[8192];
						ast_dump(ast_cur, ast_root(ast_cur), pdb2, sizeof pdb2);
						fprintf(stderr, "[ast-predump] %s:\n%s\n", funcname, pdb2);
					}
				}
				ast_slc_open();
				if (ast_slc_on)
					{ MCC_TRACE("br\n"); ast_sattr_begin(ast_cur); }
				ast_replay_body(ast_cur);
				if (ast_rir_used)
					{ MCC_TRACE("br\n"); rir_prod_replay_end(); }
				ast_replaying = 0;

				addr_t new_rel = rsec2 ? rsec2->data_offset : 0;
				int new_len = ind - ast_body_ind_sv;
				int f_len = new_len == body_len;
				int f_byte =
						f_len &&
						memcmp(cur_text_section->data + ast_body_ind_sv, orig, body_len) == 0;
				int f_rlen = new_rel - ast_reloc0_sv == rel_len;
				int f_rel = rel_len == 0 ||
										ast_reloc_range_equiv(rsec2->data + ast_reloc0_sv, orig_rel,
																					(int)rel_len);
				faithful = f_byte && f_rlen && f_rel;
				ast_unf_why = !f_len     ? "len"
											: !f_byte  ? "bytes"
											: !f_rlen  ? "rellen"
											: !f_rel   ? "relcontent"
																 : "";
				rir_unfaithful_why = ast_unf_why;
				if (!f_byte) { MCC_TRACE("br\n");
					const unsigned char *nb = cur_text_section->data + ast_body_ind_sv;
					int lim = new_len < body_len ? new_len : body_len;
					int sp_first = 0, sp_suf = 0;
					while (sp_first < lim && nb[sp_first] == orig[sp_first])
						{ MCC_TRACE("br\n"); sp_first++; }
					while (sp_suf < lim - sp_first &&
								 nb[new_len - 1 - sp_suf] == orig[body_len - 1 - sp_suf])
						{ MCC_TRACE("br\n"); sp_suf++; }
					rir_prod_span(sp_first, body_len - sp_suf, body_len, new_len);
				}
				ast_replay_completed = 1;
				{ MCC_TRACE("br\n");
					const char *pd3 = ast_rirproddump_cached;
					if (pd3 && funcname && !strcmp(pd3, funcname)) { MCC_TRACE("br\n");
						fprintf(stderr,
										"[ast-postreplay] %s loc=%d saved_loc=%d newlen=%d bodylen=%d\n",
										funcname, loc, saved_loc, new_len, body_len);
					}
				}
				ast_fn_faithful = faithful;
				if (ast_rir_used)
					{ MCC_TRACE("br\n"); rir_arena_normalise(ast_cur); }
				if (ast_slc_on)
					{ MCC_TRACE("br\n"); ast_slc_dump(ast_cur, funcname, (long)body_len,
																					 (long)new_len, faithful); }
				ast_adump_body(ast_cur, funcname);
				if (!faithful && mcc_log_enabled(MCC_LOG_TRACE)) { MCC_TRACE("br\n");
					int ast_bd = -1, ast_i;
					int ast_lim = new_len < body_len ? new_len : body_len;
					for (ast_i = 0; ast_i < ast_lim; ast_i++)
						if (cur_text_section->data[ast_body_ind_sv + ast_i] != orig[ast_i])
							{ MCC_TRACE("br\n"); ast_bd = ast_i; break; }
					MCC_TRACE_IF("UNFAITHFUL %s newlen=%d oldlen=%d firstdiff=%d "
											 "relnew=%d relold=%d\n",
											 funcname ? funcname : "?", new_len, (int)body_len, ast_bd,
											 (int)(new_rel - ast_reloc0_sv), (int)rel_len);
					if (getenv("MCC_AST_UNFAITHFUL_DUMP")) { MCC_TRACE("br\n");
						int ast_win = atoi(getenv("MCC_AST_UNFAITHFUL_DUMP"));
						int ast_w = ast_bd >= 0 ? ast_bd - 8 : (int)body_len - 24;
						int ast_dn, ast_do;
						if (ast_win < 16) { MCC_TRACE("br\n"); ast_win = 48; }
						if (ast_win >= (int)body_len && ast_win >= new_len)
							{ MCC_TRACE("br\n"); ast_w = 0; }
						if (ast_w < 0) { MCC_TRACE("br\n"); ast_w = 0; }
						ast_dn = new_len - ast_w < ast_win ? new_len - ast_w : ast_win;
						ast_do = (int)body_len - ast_w < ast_win ? (int)body_len - ast_w : ast_win;
						if (ast_dn < 0) { MCC_TRACE("br\n"); ast_dn = 0; }
						if (ast_do < 0) { MCC_TRACE("br\n"); ast_do = 0; }
						fprintf(stderr, "[unfaithful] %s @%d parser:", funcname ? funcname : "?", ast_w);
						for (ast_i = 0; ast_i < ast_do; ast_i++)
							fprintf(stderr, " %02x", orig[ast_w + ast_i]);
						fprintf(stderr, "\n[unfaithful] %s @%d replay:", funcname ? funcname : "?", ast_w);
						for (ast_i = 0; ast_i < ast_dn; ast_i++)
							fprintf(stderr, " %02x",
											cur_text_section->data[ast_body_ind_sv + ast_w + ast_i]);
						fprintf(stderr, "\n");
						{
							ElfW_Rel *rp;
							unsigned long ri;
							fprintf(stderr, "[unfaithful-rel] %s parser:", funcname ? funcname : "?");
							for (ri = 0; ri + sizeof *rp <= rel_len; ri += sizeof *rp) { MCC_TRACE("br\n");
								rp = (ElfW_Rel *)(orig_rel + ri);
								fprintf(stderr, " %s", ast_relsym_name(ELFW(R_SYM)(rp->r_info)));
							}
							fprintf(stderr, "\n[unfaithful-rel] %s replay:", funcname ? funcname : "?");
							for (ri = 0; ri + sizeof *rp <= new_rel - ast_reloc0_sv; ri += sizeof *rp) { MCC_TRACE("br\n");
								rp = (ElfW_Rel *)(rsec2->data + ast_reloc0_sv + ri);
								fprintf(stderr, " %s", ast_relsym_name(ELFW(R_SYM)(rp->r_info)));
							}
							fprintf(stderr, "\n");
						}
					}
				}
				if (ast_treechk_on())
					{ MCC_TRACE("br\n"); ast_treechk(ast_cur, funcname,
																	 faithful ? "FAITHFUL" : "not-faithful"); }

				if (ast_verify_diff && ast_verify_diff[0] && !faithful &&
						ast_verify_diff_match(funcname))
					{ MCC_TRACE("br\n"); ast_verify_dump_diff(funcname, orig, body_len,
															 cur_text_section->data + ast_body_ind_sv, new_len); }

				ast_inv_verdict = 1;
				mcc_inv_add("ast.body", 1);
				mcc_inv_add("ast.arena", ast_cur ? 1 : 0);
				mcc_inv_add("ast.faithful", faithful ? 1 : 0);
				mcc_inv_add("ast.parser_bytes", (long long)body_len);
				mcc_inv_add("ast.replay_bytes", (long long)new_len);

				ast_ltemp_cur = saved_loc;
				ast_ltemp_n = 0;
				const int ast_opt_ok =
						faithful && !ast_fn_hole && !ast_func_has_labeladdr;
				const int ast_asm_only_hole =
						ast_fn_hole && ast_fn_has_asm && !ast_fn_asm.unknown &&
						!ast_arena_has_dangle(ast_cur);
				const int ast_asm_strat_ok = faithful && ast_asm_only_hole;
				const uint32_t ast_asm_strat_admit =
						AST_STRAT_BIT(AST_STRAT_BFOLD) | AST_STRAT_BIT(AST_STRAT_IDENT) |
						AST_STRAT_BIT(AST_STRAT_RANGE) |
						AST_STRAT_BIT(AST_STRAT_DIVMAGIC) |
						AST_STRAT_BIT(AST_STRAT_ABS) | AST_STRAT_BIT(AST_STRAT_REASSOC) |
						AST_STRAT_BIT(AST_STRAT_CPROP) | AST_STRAT_BIT(AST_STRAT_DSE);
				if (ast_vlat_env && faithful) { MCC_TRACE("br\n");
					AstVLat ast_vlat_ctx;
					ast_vlat_sync(ast_cur);
					(void)ast_vlat_narrowing(ast_cur, 0, VT_INT);
					(void)ast_vlat_context(ast_cur, 0, &ast_vlat_ctx);
					(void)ast_vlat_context_at(ast_cur, ast_root(ast_cur), &ast_vlat_ctx);
#if MCC_DEV
					{
						AstLocal ast_vlat_nn = ast_count(ast_cur);
						for (AstLocal ast_vlat_u = 0; ast_vlat_u < ast_vlat_nn; ast_vlat_u++)
							{ MCC_TRACE("br\n"); (void)ast_vlat_context_at(ast_cur, ast_vlat_u, &ast_vlat_ctx); }
					}
#endif
				}
				if (ast_opt_ok && (ast_opt_limit < 0 || ast_opt_total < ast_opt_limit)) { MCC_TRACE("br\n");
					if (ast_math_inline_prepass_env)
						{ MCC_TRACE("br\n"); math_inlined = ast_math_inline_run(ast_cur); }
					/* The dump that actually predicts the transforms below: same
					 * tree, same point in the pipeline, same legality answers. The
					 * "pre-opt" one near the top of this driver is taken before the
					 * tree is rewritten and is kept only for comparison. */
					if (ast_loopdep_dump_env &&
							(ast_interchange_env || ast_fusion_env || ast_tile_env))
						{ MCC_TRACE("br\n"); ast_loopdep_dump(ast_cur, funcname, "at-transform"); }
					if (ast_interchange_env)
						{ MCC_TRACE("br\n"); interchanged = ast_interchange_run(ast_cur); }
					if (ast_fusion_env)
						{ MCC_TRACE("br\n"); fused = ast_fusion_run(ast_cur); }
					if (ast_tile_env)
						{ MCC_TRACE("br\n"); tiled = ast_tile_run(ast_cur); }
					if (ast_unroll_env)
						{ MCC_TRACE("br\n"); unrolled = ast_unroll_run(ast_cur); }
					if (ast_loopidiom_env) 						{ MCC_TRACE("br\n"); ast_loopidiom_run(ast_cur); }
					if (ast_bswap_idiom_env || ast_rotate_idiom_env)
						{ MCC_TRACE("br\n"); bswapped = ast_bswap_run(ast_cur); }
					if (ast_vectorize_env)
						{ MCC_TRACE("br\n"); vectorized = ast_vectorize_run(ast_cur); }
				}
				AstGateMask ast_search_sv_gates = ast_search_gates_now();
				if (!ast_strat_order_forced)
					{ MCC_TRACE("br\n"); ast_strat_order_reset(); }
				if (ast_opt_ok && ast_search_env && ast_search_ticks > 0) { MCC_TRACE("br\n");
					ast_search_select(sym, ast_opt_ok, saved_loc, saved_anon);
					ast_search_axis_pick(sym, ast_opt_ok, saved_loc, saved_anon);
				}
				if (ast_opt_ok && ast_roi_env) { MCC_TRACE("br\n");
					AstArena *roi_pr = ast_arena_clone(ast_cur);
					if (roi_pr)
						{ MCC_TRACE("br\n"); ast_search_roi_order(sym, ast_opt_ok, saved_loc,
																								saved_anon, roi_pr); }
				}
				if (ast_opt_ok && ast_slice_env) { MCC_TRACE("br\n");
					ast_slice_consume();
					(void)ast_slice_window_scan(ast_cur, (uint64_t)ast_search_gates_now());
				}
				{
					int sf[AST_STRAT_COUNT];
					for (int si = 0; si < AST_STRAT_COUNT; si++)
						{ MCC_TRACE("br\n"); sf[si] = 0; }
					ast_strat_admit = ast_opt_ok ? 0xffffffffu : ast_asm_strat_admit;
					ast_run_strat_cycle(ast_cur, sym, ast_opt_ok || ast_asm_strat_ok,
															ast_strat_order, ast_strat_order_n, sf);
					ast_strat_admit = 0xffffffffu;
					if (mcc_stats_mask)
						{ MCC_TRACE("br\n"); mcc_stats_strat_hits(sf, AST_STRAT_COUNT); }
					bfolds = sf[AST_STRAT_BFOLD];
					idents = sf[AST_STRAT_IDENT];
					narrows = sf[AST_STRAT_NARROW];
					cprops = sf[AST_STRAT_CPROP];
					cses = sf[AST_STRAT_CSE];
					licms = sf[AST_STRAT_LICM];
					dses = sf[AST_STRAT_DSE];
					sccps = sf[AST_STRAT_SCCP];
					jts = sf[AST_STRAT_JT];
					bfs = sf[AST_STRAT_BF];
					sethis = sf[AST_STRAT_SETHI];
					tcos = sf[AST_STRAT_TCO];
					divmagics = sf[AST_STRAT_DIVMAGIC];
					cloads = sf[AST_STRAT_CLOAD];
					sras = sf[AST_STRAT_SRA] + sf[AST_STRAT_SROA];
					selects = sf[AST_STRAT_SELECT];
					unread = sf[AST_STRAT_LTEMP] + sf[AST_STRAT_IVSR] +
									 sf[AST_STRAT_PRE] + sf[AST_STRAT_RANGE] +
									 sf[AST_STRAT_ABS] + sf[AST_STRAT_REASSOC] +
									 sf[AST_STRAT_INLINE];
				}
				ast_search_gates_set(ast_search_sv_gates);
				int do_divmagic = divmagics > 0;
				int do_cload = cloads > 0;
				int do_sra = sras > 0;
				int do_bfold = bfolds > 0;
				int do_ident = idents > 0;
				int do_narrow = narrows > 0 && !ast_opt_user_disabled(MCC_OPT_NARROW);
				int do_cprop = cprops > 0 && !ast_opt_user_disabled(MCC_OPT_TREE_COPY_PROP);
				int do_cse = cses > 0 && !ast_opt_user_disabled(MCC_OPT_GCSE);
				int do_licm = licms > 0 && !ast_opt_user_disabled(MCC_OPT_TREE_LOOP_IM);
				int do_dse = dses > 0 && !ast_opt_user_disabled(MCC_OPT_TREE_DSE);
				int do_sccp = sccps > 0;
				int do_jt = jts > 0;
				int do_bf = bfs > 0;
				int do_sethi = sethis > 0 && !ast_opt_user_disabled(MCC_OPT_SETHI_ULLMAN);
				int do_tco = tcos > 0;
				int do_select = selects > 0;
			int do_unread = unread > 0;
				int do_inline = ast_opt_ok && !do_tco && !ast_inline_pass_env &&
												ast_has_graftable_call(ast_cur);
				if (ast_search_axis_ran && !ast_search_pick_inline)
					{ MCC_TRACE("br\n"); do_inline = 0; }
				ast_no_callful_promo = do_inline;
				int do_promote = ast_opt_ok && !do_tco &&
												 ast_promote_env && ast_plan_promotion(ast_cur) > 0;
				ast_no_callful_promo = 0;
				MCC_TRACE("branch %s faithful=%d inline=%d promote=%d tco=%d\n",
									funcname, faithful, do_inline, do_promote, do_tco);
				if (do_promote && ast_promo_limit >= 0 && ast_promo_total >= ast_promo_limit) { MCC_TRACE("br\n");
					do_promote = 0;
					ast_promo_n = 0;
				}
				if (do_promote)
					{ MCC_TRACE("br\n"); ast_promo_total++; }
				if (ast_opt_limit >= 0 && ast_opt_total >= ast_opt_limit) { MCC_TRACE("br\n");
					do_inline = do_promote = do_bfold = do_ident = do_cprop = 0;
					do_cse = do_licm = do_dse = do_sccp = do_jt = do_bf = do_sethi = do_tco = 0;
					do_narrow = 0;
					do_divmagic = 0;
					do_select = 0;
					do_cload = 0;
					do_sra = 0;
					do_unread = 0;
					ast_promo_n = 0;
				}
				if (do_inline || do_promote || do_bfold || do_ident || do_cprop ||
						do_cse || do_licm || do_dse || do_sccp || do_jt || do_bf || do_sethi ||
						do_tco || do_narrow || do_divmagic || do_select || do_cload ||
						do_sra || do_unread ||
						interchanged || fused || tiled || unrolled || math_inlined || bswapped || vectorized)
					{ MCC_TRACE("br\n"); ast_opt_total++; }
				if (faithful && !do_inline && !do_promote && !do_bfold && !do_ident &&
						!do_cprop && !do_cse && !do_licm && !do_dse && !do_sccp && !do_jt &&
						!do_bf && !do_sethi && !do_tco && !do_narrow && !do_divmagic && !do_select &&
						!do_cload && !do_sra && !do_unread && !interchanged && !fused &&
						!tiled && !unrolled && !math_inlined && !bswapped && !vectorized)
					{ MCC_TRACE("br\n"); loc = saved_loc; }
				if (ast_jit_splice_env && ast_opt_ok) { MCC_TRACE("br\n");
					ast_promo_n = 0;
					ast_promo_callful = 0;
					ind = ast_body_ind_sv;
					rsym = 0;
					if (ast_rsec)
						{ MCC_TRACE("br\n"); ast_rsec->data_offset = ast_reloc0_sv; }
					nocode_wanted = 0;
					loc = saved_loc;
					int ast_splice_jmp = gjmp(0);
					gsym(ast_splice_jmp);
					ast_baseline_splice(orig, body_len, orig_rel, (int)rel_len, ast_body_ind_sv,
															orig_rsym);
					MCC_TRACE("jit-splice %s (%d code, %d rel)\n", funcname, body_len,
										(int)rel_len);
				} else if (do_inline || do_promote || do_bfold || do_ident || do_cprop ||
						do_cse || do_licm || do_dse || do_sccp || do_jt || do_bf || do_sethi ||
						do_tco || do_narrow || do_divmagic || do_select || do_cload ||
						do_sra || do_unread ||
						interchanged || fused || tiled || unrolled || math_inlined || bswapped || vectorized) { MCC_TRACE("br\n");
#define AST_PF_EMIT(ui)                                                          \
	do {                                                                           \
		ind = ast_body_ind_sv;                                                       \
		rsym = 0;                                                                    \
		if (ast_rsec)                                                                \
			ast_rsec->data_offset = ast_reloc0_sv;                                     \
		nocode_wanted = 0;                                                           \
		loc = saved_loc;                                                             \
		if (ast_ltemp_n)                                                             \
			loc = ast_ltemp_cur;                                                       \
		anon_sym = saved_anon;                                                        \
		ast_fconst_i = (do_bfold || do_ident || do_cprop || do_cse || do_licm ||     \
										do_dse || do_sccp || do_jt || do_bf || do_sethi ||           \
										do_tco || do_narrow || do_divmagic || do_select ||          \
										do_cload || do_sra || interchanged || fused || tiled ||     \
										math_inlined || unrolled || (ui))                            \
											 ? ast_fconst_n                                            \
											 : 0;                                                      \
		ast_locrec_i = 0;                                                            \
		ast_replaying = 1;                                                           \
		ast_rp_switch = NULL;                                                         \
		ast_rp_nlabel = 0;                                                           \
		ast_rp_bsym = ast_rp_csym = NULL;                                             \
		ast_pinned_regs = 0;                                                          \
		ast_inline_active = (ui);                                                     \
		ast_graft_budget = ast_graft_budget_max;                                     \
		ast_loc_low = loc;                                                            \
		ast_graft_base = loc;                                                         \
		ast_temp_frontier = 1;                                                        \
		for (int pi = 0; pi < ast_promo_n; pi++)                                      \
			ast_pinned_regs |= ((uint64_t)1 << ast_promo_regpool_at(pi));               \
		if (do_promote)                                                              \
			ast_promo_entry_init();                                                    \
		ast_replay_body(ast_cur);                                                     \
		if (ast_loc_low < loc)                                                        \
			loc = ast_loc_low;                                                         \
		ast_replaying = 0;                                                            \
		ast_inline_active = 0;                                                        \
		ast_pinned_regs = 0;                                                          \
	} while (0)
					int pf_best = do_inline;
					if (ast_perfn_inproc_env && do_inline) { MCC_TRACE("br\n");
						Sym *pf_symmark = local_stack;
						int pf_bestlen = -1;
						for (int ui = 0; ui <= 1; ui++) { MCC_TRACE("br\n");
							AST_PF_EMIT(ui);
							int len = ind - ast_body_ind_sv;
							if (pf_bestlen < 0 || len < pf_bestlen) { MCC_TRACE("br\n");
								pf_bestlen = len;
								pf_best = ui;
							}
							sym_pop(&local_stack, pf_symmark, 0);
						}
					}
					AST_PF_EMIT(pf_best);
					promoted = ast_promo_n;
#undef AST_PF_EMIT
				}
				if (ast_jit_dispatch_env && ast_opt_ok && !ast_jit_splice_env &&
						ast_jit_want(funcname, sym) &&
#if defined(MCC_TARGET_ARM64)
						ast_jit_dispatch_env == 6 && mcc_state &&
						(mcc_state->embed_jit ||
						 mcc_state->output_type == MCC_OUTPUT_MEMORY ||
						 mcc_env_on("MCC_JIT_SUBMIT_AOT"))
#else
						(ast_jit_dispatch_env != 6 ||
						 (mcc_state && (mcc_state->embed_jit ||
														mcc_state->output_type == MCC_OUTPUT_MEMORY)))
#endif
				) { MCC_TRACE("br\n");
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64) || \
		defined(MCC_TARGET_ARM64)
#if defined(MCC_TARGET_ARM64)
					int aot_base = (int)mcc_state->cg_arm64_func_start_offset;
#else
					int aot_base = ast_body_ind_sv;
#endif
					int aot_len = ind - aot_base;
					Section *rs = cur_text_section->reloc;
					int aot_rlen = rs ? (int)(rs->data_offset - ast_reloc0_sv) : 0;
					int aot_chain = rsym;
					unsigned char *aot_code = mcc_malloc(aot_len > 0 ? aot_len : 1);
					memcpy(aot_code, cur_text_section->data + aot_base, aot_len > 0 ? aot_len : 1);
					unsigned char *aot_rel = mcc_malloc(aot_rlen > 0 ? aot_rlen : 1);
					if (aot_rlen > 0)
						{ MCC_TRACE("br\n"); memcpy(aot_rel, rs->data + ast_reloc0_sv, aot_rlen); }
#if defined(MCC_TARGET_X86_64)
					if (ast_jit_dispatch_env == 6) { MCC_TRACE("br\n");
						int slot_off = (int)section_add(data_section, MCC_PTR_SIZE, MCC_PTR_SIZE);
						unsigned char *slotp = data_section->data + slot_off;
						memset(slotp, 0, MCC_PTR_SIZE);
#ifdef MCC_EMBED_JIT
						if (mcc_state && (mcc_state->embed_jit ||
															mcc_state->output_type == MCC_OUTPUT_MEMORY) &&
								!ast_jit_slot_taken(funcname)) { MCC_TRACE("br\n");
							char slotname[256];
							snprintf(slotname, sizeof slotname, "%s__mccjit_slot_%s",
											 mcc_state->leading_underscore ? "_" : "", funcname);
							set_global_sym(mcc_state, slotname, data_section, slot_off);
							mccjit_embed_note(funcname, ast_cur, sym,
																(uint64_t)ast_search_gates_now());
							ast_jit_submit_aot(sym);
						}
#endif
						ind = aot_base;
						rsym = 0;
						if (rs)
							{ MCC_TRACE("br\n"); rs->data_offset = ast_reloc0_sv; }
						nocode_wanted = 0;
						g(0xff);
						g(0x25);
						Sym *slot_sym =
							get_sym_ref(&char_pointer_type, data_section, slot_off, MCC_PTR_SIZE);
						greloca(cur_text_section, slot_sym, ind, R_X86_64_PC32, -4);
						gen_le32(0);
						Sym *body_sym =
							get_sym_ref(&char_pointer_type, cur_text_section, aot_base + 6, MCC_PTR_SIZE);
						greloca(data_section, body_sym, slot_off, R_X86_64_64, 0);
						ast_baseline_splice(aot_code, aot_len, aot_rel, aot_rlen, aot_base,
																aot_chain);
						if (saved_loc < loc)
							{ MCC_TRACE("br\n"); loc = saved_loc; }
						mcc_free(aot_code);
						mcc_free(aot_rel);
						MCC_TRACE("jit-slot %s (slot@sec+%d, body+6)\n", funcname, slot_off);
						goto ast_jit_dispatch_done;
					}
#elif defined(MCC_TARGET_I386)
					if (ast_jit_dispatch_env == 6) { MCC_TRACE("br\n");
						int slot_off = (int)section_add(data_section, MCC_PTR_SIZE, MCC_PTR_SIZE);
						unsigned char *slotp = data_section->data + slot_off;
						memset(slotp, 0, MCC_PTR_SIZE);
#ifdef MCC_EMBED_JIT
						if (mcc_state && (mcc_state->embed_jit ||
															mcc_state->output_type == MCC_OUTPUT_MEMORY) &&
								!ast_jit_slot_taken(funcname)) { MCC_TRACE("br\n");
							char slotname[256];
							snprintf(slotname, sizeof slotname, "%s__mccjit_slot_%s",
											 mcc_state->leading_underscore ? "_" : "", funcname);
							set_global_sym(mcc_state, slotname, data_section, slot_off);
							mccjit_embed_note(funcname, ast_cur, sym,
																(uint64_t)ast_search_gates_now());
							ast_jit_submit_aot(sym);
						}
#endif
						ind = aot_base;
						rsym = 0;
						if (rs)
							{ MCC_TRACE("br\n"); rs->data_offset = ast_reloc0_sv; }
						nocode_wanted = 0;
						g(0xff);
						g(0x25);
						Sym *slot_sym =
							get_sym_ref(&char_pointer_type, data_section, slot_off, MCC_PTR_SIZE);
						greloca(cur_text_section, slot_sym, ind, R_386_32, 0);
						gen_le32(0);
						Sym *body_sym =
							get_sym_ref(&char_pointer_type, cur_text_section, aot_base + 6, MCC_PTR_SIZE);
						greloca(data_section, body_sym, slot_off, R_386_32, 0);
						ast_baseline_splice(aot_code, aot_len, aot_rel, aot_rlen, aot_base,
																aot_chain);
						if (saved_loc < loc)
							{ MCC_TRACE("br\n"); loc = saved_loc; }
						mcc_free(aot_code);
						mcc_free(aot_rel);
						MCC_TRACE("jit-slot %s (slot@sec+%d, body+6)\n", funcname, slot_off);
						goto ast_jit_dispatch_done;
					}
#elif defined(MCC_TARGET_ARM64)
					if (ast_jit_dispatch_env == 6 && mcc_state &&
							(mcc_state->embed_jit ||
							 mcc_state->output_type == MCC_OUTPUT_MEMORY ||
							 mcc_env_on("MCC_JIT_SUBMIT_AOT"))) { MCC_TRACE("br\n");
						int slot_off = (int)section_add(data_section, MCC_PTR_SIZE, MCC_PTR_SIZE);
						unsigned char *slotp = data_section->data + slot_off;
						Sym *slot_sym, *body_sym;
						memset(slotp, 0, MCC_PTR_SIZE);
						if (!ast_jit_slot_taken(funcname)) { MCC_TRACE("br\n");
							char slotname[256];
							snprintf(slotname, sizeof slotname, "%s__mccjit_slot_%s",
											 mcc_state->leading_underscore ? "_" : "", funcname);
							set_global_sym(mcc_state, slotname, data_section, slot_off);
#ifdef MCC_EMBED_JIT
							if (mcc_state && (mcc_state->embed_jit ||
																mcc_state->output_type == MCC_OUTPUT_MEMORY)) { MCC_TRACE("br\n");
								mccjit_embed_note(funcname, ast_cur, sym,
																	(uint64_t)ast_search_gates_now());
								ast_jit_submit_aot(sym);
							}
#endif
						}
						ind = aot_base;
						rsym = 0;
						if (rs)
							{ MCC_TRACE("br\n"); rs->data_offset = ast_reloc0_sv; }
						nocode_wanted = 0;
						slot_sym =
							get_sym_ref(&char_pointer_type, data_section, slot_off, MCC_PTR_SIZE);
						greloca(cur_text_section, slot_sym, ind, R_AARCH64_ADR_PREL_PG_HI21, 0);
						o(0x90000010);
						greloca(cur_text_section, slot_sym, ind, R_AARCH64_ADD_ABS_LO12_NC, 0);
						o(0x91000210);
						o(0xf9400210);
						o(0xd61f0200);
						body_sym = get_sym_ref(&char_pointer_type, cur_text_section,
																	 aot_base + 16, MCC_PTR_SIZE);
						greloca(data_section, body_sym, slot_off, R_AARCH64_ABS64, 0);
						ast_baseline_splice(aot_code, aot_len, aot_rel, aot_rlen, aot_base,
																aot_chain);
						mcc_state->cg_arm64_func_sub_sp_offset += 16;
						if (saved_loc < loc)
							{ MCC_TRACE("br\n"); loc = saved_loc; }
						mcc_free(aot_code);
						mcc_free(aot_rel);
						MCC_TRACE("jit-slot %s (slot@sec+%d, body+16)\n", funcname, slot_off);
						goto ast_jit_dispatch_done;
					}
#endif
#if !defined(MCC_TARGET_ARM64)
					int specmode = ast_jit_dispatch_env;
					int poffs[AST_TCO_MAXP];
					int64_t pvals[AST_TCO_MAXP], plos[AST_TCO_MAXP], phis[AST_TCO_MAXP];
					int npoff = 0;
					if (specmode == 3)
						{ MCC_TRACE("br\n"); npoff = ast_nonnull_params(ast_cur, sym, poffs, AST_TCO_MAXP); }
					else if (specmode == 4)
						{ MCC_TRACE("br\n"); npoff = ast_constparam_params(ast_cur, sym, poffs, pvals, AST_TCO_MAXP); }
					else if (specmode == 5)
						{ MCC_TRACE("br\n"); npoff = ast_rangeparam_params(ast_cur, sym, poffs, plos, phis, AST_TCO_MAXP); }
					AstArena *ast_spec = NULL;
					if (npoff > 0 && (ast_spec = ast_arena_clone(ast_cur))) { MCC_TRACE("br\n");
						AstArena *sv = ast_cur;
						ast_cur = ast_spec;
						if (specmode == 3)
							{ MCC_TRACE("br\n"); ast_nonnull_fold(ast_spec, poffs, npoff); }
						else if (specmode == 4)
							{ MCC_TRACE("br\n"); ast_constparam_fold(ast_spec, poffs, pvals, npoff); }
						ast_sccp_run(ast_spec);
						ast_cur = sv;
#ifdef AST_EVAL_SLICE_PROVIDED
						{
							int forced = mcc_env_on("MCC_AST_EVAL_FORCE_UNSOUND");
							int gate = mcc_env_on("MCC_AST_JIT_EVAL_GATE");
#if MCC_DEV
							int need = 1;
#else
							int need = gate || forced;
#endif
							int sound = 1;
							if (need)
								{ MCC_TRACE("br\n"); sound = forced ? 0
															 : ast_eval_slice_sound(ast_cur, ast_spec, specmode,
																											poffs, pvals, plos, phis,
																											npoff, AST_TCO_MAXP); }
							if (!sound) { MCC_TRACE("br\n");
#if MCC_DEV
								if (!forced) { MCC_TRACE("br\n");
									fprintf(stderr,
													"mcc: AST side-car divergence: jit-spec-slice(mode=%d) return-value mismatch over guarded domain\n",
													specmode);
									abort();
								}
#endif
								if (gate) { MCC_TRACE("br\n");
									ast_arena_free(ast_spec);
									ast_spec = NULL;
									ast_jit_eval_refused++;
									if (mcc_env_on("MCC_JIT_VERBOSE"))
										{ MCC_TRACE("br\n"); fprintf(stderr,
														"mccjit: eval-slice hard-gate refused unsound spec (mode=%d)\n",
														specmode); }
								}
							}
						}
#endif
					}
					int spec = ast_spec != NULL;
					ind = aot_base;
					rsym = 0;
					if (rs)
						{ MCC_TRACE("br\n"); rs->data_offset = ast_reloc0_sv; }
					nocode_wanted = 0;
					int ast_guard_slots[2 * AST_TCO_MAXP + 1], ast_guard_n = 0;
					if (spec && specmode == 4) { MCC_TRACE("br\n");
						for (int i = 0; i < npoff; i++) { MCC_TRACE("br\n");
							g(0x48);
							g(0x81);
							g(0xbd);
							gen_le32(poffs[i]);
							gen_le32((int)pvals[i]);
							g(0x0f);
							ast_guard_slots[ast_guard_n++] = oad(0x85, 0);
						}
					} else if (spec && specmode == 5) { MCC_TRACE("br\n");
						for (int i = 0; i < npoff; i++) { MCC_TRACE("br\n");
							if ((int64_t)(int32_t)plos[i] == plos[i]) { MCC_TRACE("br\n");
								g(0x48);
								g(0x81);
								g(0xbd);
								gen_le32(poffs[i]);
								gen_le32((int)plos[i]);
							} else { MCC_TRACE("br\n");
								g(0x48);
								g(0xb8);
								gen_le32((int)(uint32_t)(uint64_t)plos[i]);
								gen_le32((int)(uint32_t)((uint64_t)plos[i] >> 32));
								g(0x48);
								g(0x39);
								g(0x85);
								gen_le32(poffs[i]);
							}
							g(0x0f);
							ast_guard_slots[ast_guard_n++] = oad(0x8c, 0);
							if ((int64_t)(int32_t)phis[i] == phis[i]) { MCC_TRACE("br\n");
								g(0x48);
								g(0x81);
								g(0xbd);
								gen_le32(poffs[i]);
								gen_le32((int)phis[i]);
							} else { MCC_TRACE("br\n");
								g(0x48);
								g(0xb8);
								gen_le32((int)(uint32_t)(uint64_t)phis[i]);
								gen_le32((int)(uint32_t)((uint64_t)phis[i] >> 32));
								g(0x48);
								g(0x39);
								g(0x85);
								gen_le32(poffs[i]);
							}
							g(0x0f);
							ast_guard_slots[ast_guard_n++] = oad(0x8f, 0);
						}
					} else if (spec) { MCC_TRACE("br\n");
						for (int i = 0; i < npoff; i++) { MCC_TRACE("br\n");
							g(0x48);
							g(0x83);
							g(0xbd);
							gen_le32(poffs[i]);
							g(0x00);
							g(0x0f);
							ast_guard_slots[ast_guard_n++] = oad(0x84, 0);
						}
					} else { MCC_TRACE("br\n");
						o(0xc031);
						o(0x0f);
						ast_guard_slots[ast_guard_n++] = oad(ast_jit_dispatch_env >= 2 ? 0x84 : 0x85, 0);
					}
					if (spec) { MCC_TRACE("br\n");
						AstArena *sv = ast_cur;
						ast_cur = ast_spec;
						loc = saved_loc;
						if (ast_ltemp_n)
							{ MCC_TRACE("br\n"); loc = ast_ltemp_cur; }
						anon_sym = saved_anon;
						ast_fconst_i = ast_fconst_n;
						ast_locrec_i = 0;
						ast_replaying = 1;
						ast_rp_switch = NULL;
						ast_rp_nlabel = 0;
						ast_rp_bsym = ast_rp_csym = NULL;
						ast_pinned_regs = 0;
						ast_inline_active = 0;
						ast_inline_bias = 0;
						ast_argsub_n = 0;
						ast_promo_n = 0;
						ast_graft_budget = ast_graft_budget_max;
						ast_loc_low = loc;
						ast_temp_frontier = 1;
						ast_replay_body(ast_spec);
						if (ast_loc_low < loc)
							{ MCC_TRACE("br\n"); loc = ast_loc_low; }
						ast_replaying = 0;
						ast_cur = sv;
						ast_arena_free(ast_spec);
					} else { MCC_TRACE("br\n");
						ast_baseline_splice(aot_code, aot_len, aot_rel, aot_rlen, aot_base, aot_chain);
					}
					nocode_wanted = 0;
					rsym = gjmp(rsym);
					for (int i = 0; i < ast_guard_n; i++)
						{ MCC_TRACE("br\n"); gsym(ast_guard_slots[i]); }
					ast_baseline_splice(aot_code, aot_len, aot_rel, aot_rlen, aot_base, aot_chain);
					if (saved_loc < loc)
						{ MCC_TRACE("br\n"); loc = saved_loc; }
					mcc_free(aot_code);
					mcc_free(aot_rel);
					MCC_TRACE("jit-dispatch %s mode=%d spec=%d np=%d (%d code, %d rel)\n", funcname,
										ast_jit_dispatch_env, spec, npoff, aot_len, aot_rlen);
#endif
#if defined(MCC_TARGET_X86_64) || defined(MCC_TARGET_ARM64) || \
		defined(MCC_TARGET_I386)
				ast_jit_dispatch_done:;
#endif
#endif
				}
			} else { MCC_TRACE("br\n");
				mcc_asm_inline_unwind();
				if (ast_rir_used)
					{ MCC_TRACE("br\n"); rir_prod_replay_end(); }
				mcc_state->nb_errors = ast_saved_nberr;
				vtop = vstack + ast_base_depth - 1;
				ast_replaying = 0;
				ast_inline_active = 0;
				ast_pinned_regs = 0;
				ast_promo_n = 0;
				ast_promo_callful = 0;
				loc = saved_loc;
				anon_sym = saved_anon;
				faithful = 0;
				ast_replay_completed = 0;
				ast_unf_why = "posterr";
				mcc_inv_add(ast_inv_verdict ? "ast.abort_post" : "ast.abort", 1);
			}
			memcpy(mcc_state->error_jmp_buf, ast_outer_jmp, sizeof(jmp_buf));
			mcc_state->error_set_jmp_enabled = ast_outer_en;
			mcc_state->error_func = ast_sv_efunc;
			mcc_state->error_opaque = ast_sv_eopaque;
			nb_stk_data = stk_data_floor;
			stk_data_floor = ast_saved_floor;
			mcc_state->nb_errors = ast_saved_nberr;
			sym_free_first = ast_saved_free;
			mcc_state->warn_none = ast_sv_warn;
			seqp_reset();
			int keep = faithful ||
								 (ast_rir_nofb_env && ast_replay_completed && !ast_fn_hole &&
									!ast_fn_alloca && saved_func_alloca == 0);
			if (keep && !faithful && funcname) { MCC_TRACE("br\n");
				const char *sk = getenv("MCC_RIR_NOFB_SKIP");
				if (sk && *sk) { MCC_TRACE("br\n");
					size_t fl = strlen(funcname);
					const char *q = sk;
					while (*q) { MCC_TRACE("br\n");
						const char *e2 = q;
						while (*e2 && *e2 != ',')
							{ MCC_TRACE("br\n"); e2++; }
						if ((size_t)(e2 - q) == fl && !memcmp(q, funcname, fl))
							{ MCC_TRACE("br\n"); keep = 0; break; }
						q = *e2 ? e2 + 1 : e2;
					}
				}
			}
			if (keep && saved_loc < loc)
				loc = saved_loc;
			if (ast_rir_used) { MCC_TRACE("br\n");
				rir_unfaithful_why = ast_replay_completed ? ast_unf_why : "abort";
				rir_prod_body_set((long)(keep ? ind - ast_body_ind_sv : body_len));
				rir_prod_note(keep ? "used" : "fallback");
			}
			if (!keep) { MCC_TRACE("br\n");
				mcc_state->cg_func_alloca = saved_func_alloca;
				memcpy(cur_text_section->data + ast_body_ind_sv, orig, body_len);
				if (rel_len)
					{ MCC_TRACE("br\n"); memcpy(ast_rsec->data + ast_reloc0_sv, orig_rel, rel_len); }
				if (rsec2)
					{ MCC_TRACE("br\n"); rsec2->data_offset = ast_reloc1; }
				ind = orig_ind;
				rsym = orig_rsym;
				loc = saved_loc;
			} else if (ast_replay_dump) { MCC_TRACE("br\n");
				char buf[512];
				ast_dump(ast_cur, ast_root(ast_cur), buf, sizeof buf);
				fprintf(stderr, "[ast-replay] %s\n%s", funcname, buf);
				if (ast_templates_env)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-template] const-fold %d %s\n",
									ast_tmpl_folds, funcname); }
				if (bfolds)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-bfold] %d %s\n", bfolds, funcname); }
				if (cloads)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-cload] %d %s\n", cloads, funcname); }
				if (idents)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-ident] %d %s\n", idents, funcname); }
				if (narrows)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-narrow] %d %s\n", narrows, funcname); }
				if (cprops)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-cprop] %d %s\n", cprops, funcname); }
				if (cses)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-cse] %d %s\n", cses, funcname); }
				if (licms)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-licm] %d %s\n", licms, funcname); }
				if (dses)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-dse] %d %s\n", dses, funcname); }
				if (sccps)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-sccp] %d %s\n", sccps, funcname); }
				if (jts)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-jt] %d %s\n", jts, funcname); }
				if (bfs)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-bitflag] %d %s\n", bfs, funcname); }
				if (sethis)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-sethi] %d %s\n", sethis, funcname); }
				if (tcos)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-tco] %d %s\n", tcos, funcname); }
				if (promoted)
					{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-promote] %d %s\n", promoted, funcname); }
			}
			if (faithful && ind < orig_ind)
				{ MCC_TRACE("br\n"); memset(cur_text_section->data + ind, 0, (size_t)(orig_ind - ind)); }
			if (ast_jit_env && faithful && ast_jit_want(funcname, sym))
				{ MCC_TRACE("br\n"); keep_baseline = ast_baseline_retain(sym, ast_cur, ast_body_ind_sv,
																						ast_reloc0_sv, rsym); }
			mcc_free(orig);
			mcc_free(orig_rel);
		} else { MCC_TRACE("br\n");
			mcc_inv_add("ast.noreplay", 1);
			if (ast_rir_used) { MCC_TRACE("br\n");
				rir_prod_why_set("replayok");
				rir_prod_body_set((long)(ind - ast_body_ind_sv));
				rir_prod_note("nomodel");
			}
		}
#if defined(AST_EVAL_SLICE_PROVIDED) && MCC_EMBED_JIT
		if (ast_cur)
			{ MCC_TRACE("br\n"); ast_ladder_census(ast_cur); }
#endif
#ifdef MCC_EMBED_JIT
		if ((ast_jit_dispatch_env || ast_jit_fns_n > 0) && ast_fn_faithful &&
				ast_cur && ast_jit_want(funcname, sym) && !ast_arena_has_hole(ast_cur))
			{ MCC_TRACE("br\n"); mcc_inv_add("jit.baked", 1);
				mccjit_embed_stash_leaf(ast_cur, sym); }
#endif
		int keep_inline = ast_fn_faithful && ast_inline_retain(ast_cur, sym);
		int keep_reemit = ast_fn_faithful && ast_reemit_retain(ast_cur, sym);
		if (!keep_inline && !keep_reemit && !keep_baseline)
			{ MCC_TRACE("br\n"); ast_arena_free(ast_cur); }
		ast_cur = NULL;
		ast_sym_defer_on = 0;
		if (!(keep_inline || keep_reemit || keep_baseline)) { MCC_TRACE("br\n");
			while (ast_sym_deferred_n) { MCC_TRACE("br\n");
				Sym *s = ast_sym_deferred[--ast_sym_deferred_n];
#if !MCC_DIAG
				s->next = sym_free_first;
				sym_free_first = s;
#else
				mcc_free(s);
#endif
			}
		}
		while (ast_sym_deferred_n) { MCC_TRACE("br\n");
			Sym *rs = ast_sym_deferred[--ast_sym_deferred_n];
			if (ast_sym_retained_n == ast_sym_retained_cap) { MCC_TRACE("br\n");
				int nc = ast_sym_retained_cap ? ast_sym_retained_cap * 2 : 64;
				ast_sym_retained =
						mcc_realloc(ast_sym_retained, nc * sizeof(*ast_sym_retained));
				ast_sym_retained_cap = nc;
			}
			ast_sym_retained[ast_sym_retained_n++] = rs;
		}
		ast_templates_env = ast_sv_tmpl;
		ast_promote_env = ast_sv_promo;
		ast_inline_env = ast_sv_inl;
	}
	mcc_stackref_commit();
}

void ast_func_epilog(void) { MCC_TRACE("enter\n");
	ast_promo_exit_restore();
	ast_promo_n = 0;
	ast_promo_callful = 0;
}

/* Forward-inline re-emit runs at end-of-translation, after every frame whose
   leaves this arena references has been torn down by ast_func_end. A leaf's
   captured sym is the referencing frame's own local Sym -- valid, and required,
   during in-function RIR replay (wide256_sv_is_stable_lval consults it), but a
   dangling pointer here. gaddrof dereferences it for its VLA-struct check and
   faults on the freed Sym (defect W8). Re-emit rebuilds the frame from scratch
   exactly as an AOT compile would, and an AOT compile carries no sym on a plain
   local lvalue -- VT_LOCAL without VT_SYM -- so drop those before replay. A real
   symbol reference (VT_SYM) stays valid to end-of-translation and is kept; a VLA
   leaf keeps its sym (gaddrof still needs it), recognised off the node type so
   the possibly-dangling pointer is never dereferenced. */
static void ast_reemit_scrub_leaf_syms(AstArena *a) { MCC_TRACE("enter\n");
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
		uint16_t k = ast_kind(a, n);
		if (k != AST_Ref && k != AST_Literal)
			{ MCC_TRACE("br\n"); continue; }
		unsigned r = (unsigned)ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM) &&
				!(ast_type_t(a, n) & VT_VLA))
			{ MCC_TRACE("br\n"); ast_set_sym(a, n, 0); }
	}
}

static void ast_reemit(Sym *sym, AstArena *ast) { MCC_TRACE("enter\n");
	struct scope f = {0};
	cur_scope = root_scope = &f;
	nocode_wanted = 0;
	cur_text_section = text_section;
	ind = cur_text_section->data_offset;
	if (sym->a.aligned) { MCC_TRACE("br\n");
		size_t no = section_add(cur_text_section, 0, 1 << (sym->a.aligned - 1));
		gen_fill_nops(no - ind);
	}
	int new_ind = ind;
	funcname = get_tok_str(sym->v, NULL);
	func_ind = ind;
	func_vt = sym->type.ref->type;
	func_var = sym->type.ref->f.func_type == FUNC_ELLIPSIS;
	func_old = sym->type.ref->f.func_type == FUNC_OLD;
	cur_func_noreturn = sym->type.ref->f.func_noreturn;
	cur_func_inline_extern = 0;
	vla_seq = 0;
	nb_vla_open = 0;
	vla_track_ovf = 0;

	sym_push2(&local_stack, SYM_FIELD, 0, 0);
	local_scope = 1;
	sym_push_params(sym->type.ref);
	local_scope = 0;
	rsym = 0;
	nb_temp_local_vars = 0;
	gfunc_prolog(sym);
	func_vla_arg(sym);

	{
		AstLocal nn = ast_count(ast);
		for (AstLocal n = 0; n < nn; n++) { MCC_TRACE("br\n");
			uint16_t k = ast_kind(ast, n);
			if (k != AST_Ref && k != AST_Literal)
				{ MCC_TRACE("br\n"); continue; }
			int r = ast_op(ast, n);
			if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) { MCC_TRACE("br\n");
				int off = (int)ast_ival(ast, n);
				if (off < loc)
					{ MCC_TRACE("br\n"); loc = off; }
			}
		}
	}

	Sym *saved_free = sym_free_first;
	sym_free_first = NULL;
	ast_cur = ast;
	int saved_in_reemit = ast_in_reemit;
	ast_in_reemit = 1;
	ast_reemit_scrub_leaf_syms(ast);
	ast_replaying = 1;
	ast_rp_switch = NULL;
	ast_rp_nlabel = 0;
	ast_rp_label_floor = 0;
	ast_rp_bsym = ast_rp_csym = NULL;
	ast_fconst_i = 0;
	ast_locrec_i = 0;
	ast_temp_frontier = 1;
	ast_promo_n = 0;
	ast_pinned_regs = 0;
	ast_inline_active = 1;
	ast_graft_budget = ast_graft_budget_max;
	ast_loc_low = loc;
	ast_graft_base = loc;
	unsigned char ast_re_warn = mcc_state ? mcc_state->warn_none : 0;
	if (mcc_state)
		{ MCC_TRACE("br\n"); mcc_state->warn_none = 1; }
	ast_replay_body(ast);
	if (mcc_state)
		{ MCC_TRACE("br\n"); mcc_state->warn_none = ast_re_warn; }
	if (ast_loc_low < loc)
		{ MCC_TRACE("br\n"); loc = ast_loc_low; }
	ast_inline_active = 0;
	ast_replaying = 0;
	ast_in_reemit = saved_in_reemit;
	sym_free_first = saved_free;
	ast_cur = NULL;

	gsym(rsym);
	ast_promo_n = 0;
	nocode_wanted = 0;
	gfunc_epilog();
#if MCC_EH_FRAME
	mcc_debug_frame_end(mcc_state, ind - new_ind);
#endif
	put_extern_sym(sym, cur_text_section, new_ind, ind - new_ind);
	rir_prod_reemit((long)(ind - new_ind));
	elfsym(sym)->st_size = ind - new_ind;
	cur_text_section->data_offset = ind;

	sym_pop(&local_stack, NULL, 0);
	label_pop(&global_label_stack, NULL, 0);
	sym_pop(&all_cleanups, NULL, 0);
	local_scope = 0;
	cur_text_section = NULL;
	funcname = "";
	func_vt.t = VT_VOID;
	func_var = 0;
	ind = 0;
	func_ind = -1;
	nocode_wanted = DATA_ONLY_WANTED;
	if (ast_replay_dump)
		{ MCC_TRACE("br\n"); fprintf(stderr, "[ast-inline] re-emitted %s (forward inline)\n",
						get_tok_str(sym->v, NULL)); }
}

void ast_reemit_forward_inlines(void) { MCC_TRACE("enter\n");
	for (int i = 0; i < ast_reemit_n; i++)
		{ MCC_TRACE("br\n"); if (ast_reemit_has_forward(&ast_reemit_pool[i])) { MCC_TRACE("br\n");
			mcc_inv_add("ast.orphan_fn", 1);
			mcc_inv_add("ast.orphan_bytes", ast_reemit_pool[i].body_len);
			mcc_inv_add("ast.orphan_relbytes", ast_reemit_pool[i].rel_len);
			MCC_TRACE("reemit orphan at +%d len=%d rel0=%ld rellen=%d\n",
								ast_reemit_pool[i].body_ind, ast_reemit_pool[i].body_len,
								(long)ast_reemit_pool[i].reloc0, ast_reemit_pool[i].rel_len);
			ast_reemit(ast_reemit_pool[i].sym, ast_reemit_pool[i].ast); } }
}

#ifdef MCC_EMBED_JIT
void ast_reemit_extern(Sym *sym, AstArena *ast) { MCC_TRACE("enter\n");
	ast_reemit(sym, ast);
}

void ast_reemit_with_gates(Sym *sym, AstArena *ast, uint64_t gate_mask) { MCC_TRACE("enter\n");
	AstArena *saved_cur = ast_cur;
	AstGateMask saved_gates = ast_search_gates_now();
	AstArena *trial = ast_arena_clone(ast);
	if (!trial) { MCC_TRACE("br\n"); ast_reemit(sym, ast); return; }
	ast_search_gates_set((AstGateMask)gate_mask);
	ast_cur = trial;
	ast_run_strat_cycle(trial, sym, 1, ast_strat_order, ast_strat_order_n, NULL);
	ast_search_gates_set(saved_gates);
	ast_cur = saved_cur;
	ast_reemit(sym, trial);
	ast_arena_free(trial);
}

int mccjit_ast_blind_retype(AstArena *ast) { MCC_TRACE("enter\n");
	AstLocal n, nn;
	AstLocal fallback = AST_NONE;
	AstArena *sv = ast_cur;
	int saved_env;
	if (!ast)
		{ MCC_TRACE("br\n"); return 0; }
	ast_cur = ast;
	saved_env = ast_vlat_env;
	ast_vlat_env = 1;
	nn = ast->count;
	for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
		int t = ast_type_t(ast, n);
		int off, kt;
		if ((t & VT_BTYPE) != VT_LLONG)
			{ MCC_TRACE("br\n"); continue; }
		if (fallback == AST_NONE)
			{ MCC_TRACE("br\n"); fallback = n; }
		if (ast_vlat_use_of(ast, n, &off, &kt) &&
				ast_vlat_narrowing(ast, off, VT_INT)) { MCC_TRACE("br\n");
			ast_set_type(ast, n, (t & ~VT_BTYPE) | VT_INT, 0);
			ast_vlat_env = saved_env;
			ast_cur = sv;
			return 2;
		}
	}
	if (fallback != AST_NONE) { MCC_TRACE("br\n");
		for (n = 0; n < nn; n++) { MCC_TRACE("br\n");
			int t = ast_type_t(ast, n);
			int off, kt;
			if ((t & VT_BTYPE) != VT_LLONG)
				{ MCC_TRACE("br\n"); continue; }
			if (mcc_stats_mask) { MCC_TRACE("br\n");
				mcc_stats_jit_blind_narrow_kind(
						ast_vlat_use_of(ast, n, &off, &kt));
			}
			ast_set_type(ast, n, (t & ~VT_BTYPE) | VT_INT, 0);
			if (ast_kind(ast, n) == AST_Binary) { MCC_TRACE("br\n");
				int bop = ast_op(ast, n);
				if (bop == '+' || bop == '-' || bop == '*')
					{ MCC_TRACE("br\n"); ast_set_fbits(ast, n,
							ast_fbits(ast, n) | AST_FB_JIT_GUARD); }
			}
		}
		ast_vlat_env = saved_env;
		ast_cur = sv;
		return 1;
	}
	ast_vlat_env = saved_env;
	ast_cur = sv;
	return 0;
}

int mccjit_ast_spec_fold(AstArena *ast, int off, int64_t val) { MCC_TRACE("enter\n");
	AstArena *sv = ast_cur;
	int folds;
	if (!ast)
		{ MCC_TRACE("br\n"); return 0; }
	ast_cur = ast;
	folds = ast_constparam_fold(ast, &off, &val, 1);
	ast_sccp_run(ast);
	ast_cur = sv;
	return folds;
}
#endif

#undef gjmp_addr
#undef gjmp

#endif

#endif
