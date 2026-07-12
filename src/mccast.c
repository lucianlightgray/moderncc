#if MCC_CONFIG_OPTIMIZER && (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))

#include "mccast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "mccforecast.h"

#include "algorithms/lzss.h"
#include "algorithms/lzw.h"
#include "algorithms/rle.h"
#include "mcccombo.h"
#include "mccmagic.h" /* constant-division magic (selftested in tools/asttool.c:suite_magic) */

#ifndef AST_ASSERT
#include <assert.h>
#define AST_ASSERT(x) assert(x)
#endif

#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef realloc
#undef free

struct AstArena {
	uint16_t *kind;
	AstLocal *parent;
	AstLocal *first_child;
	AstLocal *last_child;
	AstLocal *next_sib;
	uint32_t *nchild;

	int32_t *op;
	int32_t *type_t;
	uint64_t *type_ref;
	uint64_t *ival;
	uint64_t *fbits;
	uint64_t *sym;
	uint64_t *cst;

	AstLocal count;
	AstLocal cap;
	uint64_t epoch;
};

static void ast_grow(AstArena *a, AstLocal need) {
	if (need <= a->cap)
		return;
	AstLocal ncap = a->cap ? a->cap * 2 : 64;
	while (ncap < need)
		ncap *= 2;
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
	AST_REGROW(type_ref);
	AST_REGROW(ival);
	AST_REGROW(fbits);
	AST_REGROW(sym);
	AST_REGROW(cst);
#undef AST_REGROW
	a->cap = ncap;
}

#if defined(MCC_INTERNAL) && MCC_CONFIG_OPTIMIZER
static void ast_du_invalidate(const AstArena *a);
static void ast_memo_invalidate(const AstArena *a);
static void ast_hash_invalidate(const AstArena *a);
#endif

AstArena *ast_arena_new(void) {
	AstArena *a = calloc(1, sizeof *a);
	return a;
}

void ast_arena_reset(AstArena *a) {
	a->count = 0;
	a->epoch++;
}

void ast_arena_free(AstArena *a) {
	if (!a)
		return;
#if defined(MCC_INTERNAL) && MCC_CONFIG_OPTIMIZER
	ast_du_invalidate(a);
	ast_memo_invalidate(a);
	ast_hash_invalidate(a);
#endif
	free(a->kind);
	free(a->parent);
	free(a->first_child);
	free(a->last_child);
	free(a->next_sib);
	free(a->nchild);
	free(a->op);
	free(a->type_t);
	free(a->type_ref);
	free(a->ival);
	free(a->fbits);
	free(a->sym);
	free(a->cst);
	free(a);
}

AstArena *ast_arena_clone(const AstArena *src) {
	AstArena *a = calloc(1, sizeof *a);
	if (!a)
		return NULL;
	a->count = src->count;
	a->cap = src->count;
	if (src->count == 0)
		return a;
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
	AST_DUP(type_ref);
	AST_DUP(ival);
	AST_DUP(fbits);
	AST_DUP(sym);
	AST_DUP(cst);
#undef AST_DUP
	return a;
}

AstLocal ast_node(AstArena *a, uint16_t kind) {
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
	a->type_ref[n] = 0;
	a->ival[n] = 0;
	a->fbits[n] = 0;
	a->sym[n] = 0;
	a->cst[n] = 0;
	a->epoch++;
	return n;
}

void ast_add_child(AstArena *a, AstLocal parent, AstLocal child) {
	AST_ASSERT(parent < a->count && child < a->count);
	a->epoch++;
	a->parent[child] = parent;
	a->next_sib[child] = AST_NONE;
	if (a->first_child[parent] == AST_NONE) {
		a->first_child[parent] = child;
	} else {
		a->next_sib[a->last_child[parent]] = child;
	}
	a->last_child[parent] = child;
	a->nchild[parent]++;
}

void ast_set_kind(AstArena *a, AstLocal n, uint16_t kind) {
	a->epoch++;
	a->kind[n] = kind;
}
void ast_clear_children(AstArena *a, AstLocal n) {
	a->epoch++;
	a->first_child[n] = AST_NONE;
	a->last_child[n] = AST_NONE;
	a->nchild[n] = 0;
}

void ast_set_op(AstArena *a, AstLocal n, int op) {
	a->epoch++;
	a->op[n] = op;
}
void ast_set_type(AstArena *a, AstLocal n, int type_t, uint64_t type_ref) {
	a->epoch++;
	a->type_t[n] = type_t;
	a->type_ref[n] = type_ref;
}
void ast_set_ival(AstArena *a, AstLocal n, uint64_t v) {
	a->epoch++;
	a->ival[n] = v;
}
void ast_set_fbits(AstArena *a, AstLocal n, uint64_t bits) {
	a->epoch++;
	a->fbits[n] = bits;
}
void ast_set_sym(AstArena *a, AstLocal n, uint64_t sym) {
	a->epoch++;
	a->sym[n] = sym;
}
void ast_set_cst(AstArena *a, AstLocal n, uint64_t cst_id) {
	a->epoch++;
	a->cst[n] = cst_id;
}

uint16_t ast_kind(const AstArena *a, AstLocal n) {
	return a->kind[n];
}
int ast_op(const AstArena *a, AstLocal n) {
	return a->op[n];
}
int ast_type_t(const AstArena *a, AstLocal n) {
	return a->type_t[n];
}
uint64_t ast_type_ref(const AstArena *a, AstLocal n) {
	return a->type_ref[n];
}
uint64_t ast_ival(const AstArena *a, AstLocal n) {
	return a->ival[n];
}
uint64_t ast_fbits(const AstArena *a, AstLocal n) {
	return a->fbits[n];
}
uint64_t ast_sym(const AstArena *a, AstLocal n) {
	return a->sym[n];
}
uint64_t ast_cst(const AstArena *a, AstLocal n) {
	return a->cst[n];
}

AstLocal ast_parent(const AstArena *a, AstLocal n) {
	return a->parent[n];
}
AstLocal ast_first_child(const AstArena *a, AstLocal n) {
	return a->first_child[n];
}
AstLocal ast_last_child(const AstArena *a, AstLocal n) {
	return a->last_child[n];
}
AstLocal ast_next_sib(const AstArena *a, AstLocal n) {
	return a->next_sib[n];
}
uint32_t ast_nchild(const AstArena *a, AstLocal n) {
	return a->nchild[n];
}
AstLocal ast_count(const AstArena *a) {
	return a->count;
}
AstLocal ast_root(const AstArena *a) {
	return a->count ? 0 : AST_NONE;
}

AstLocal ast_child(const AstArena *a, AstLocal n, uint32_t i) {
	AstLocal c = a->first_child[n];
	while (c != AST_NONE && i--)
		c = a->next_sib[c];
	return c;
}

static const char *const kind_names[AST_KIND_COUNT] = {
		"TranslationUnit",
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
		"InitList",
		"Poison",
};

const char *ast_kind_name(uint16_t kind) {
	if (kind >= AST_KIND_COUNT)
		return "?";
	return kind_names[kind];
}

static void op_str(int op, char *buf, size_t cap) {
	if (op > 0x20 && op < 0x7f)
		snprintf(buf, cap, "%c", op);
	else if (op)
		snprintf(buf, cap, "op#%d", op);
	else
		buf[0] = 0;
}

typedef struct {
	char *out;
	size_t cap;
	size_t len;
} DumpBuf;

static void dump_emit(DumpBuf *d, const char *s) {
	size_t n = strlen(s);
	for (size_t i = 0; i < n; i++) {
		if (d->out && d->len < d->cap)
			d->out[d->len] = s[i];
		d->len++;
	}
}

static void dump_rec(const AstArena *a, AstLocal n, int depth, DumpBuf *d) {
	char line[128], ops[32];
	for (int i = 0; i < depth; i++)
		dump_emit(d, "  ");
	op_str(a->op[n], ops, sizeof ops);
	switch (a->kind[n]) {
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
		dump_rec(a, c, depth + 1, d);
}

size_t ast_dump(const AstArena *a, AstLocal root, char *out, size_t cap) {
	DumpBuf d = {out, cap ? cap - 1 : 0, 0};
	if (root != AST_NONE && root < a->count)
		dump_rec(a, root, 0, &d);
	if (out && cap)
		out[d.len < cap ? d.len : cap - 1] = 0;
	return d.len;
}

int ast_validate(const AstArena *a, char *msg, size_t msgcap) {
#define AST_FAIL(m)                   \
	do {                                \
		if (msg)                          \
			snprintf(msg, msgcap, "%s", m); \
		return -1;                        \
	} while (0)
	for (AstLocal n = 0; n < a->count; n++) {
		if (a->kind[n] >= AST_KIND_COUNT)
			AST_FAIL("node kind out of range");
		uint32_t seen = 0;
		AstLocal prev = AST_NONE;
		for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c]) {
			if (a->parent[c] != n)
				AST_FAIL("child parent link mismatch");
			prev = c;
			seen++;
			if (seen > a->count)
				AST_FAIL("cyclic sibling chain");
		}
		if (seen != a->nchild[n])
			AST_FAIL("nchild disagrees with sibling chain");
		if (seen && a->last_child[n] != prev)
			AST_FAIL("last_child is not the final sibling");
	}
#undef AST_FAIL
	return 0;
}

typedef struct {
	uint64_t *syms;
	uint32_t nsym, cap;
	int oom;
} AstIhSyms;

static uint64_t ast_ih_fold(uint64_t h, uint64_t v) {
	for (int i = 0; i < 8; i++) {
		h ^= (v >> (i * 8)) & 0xff;
		h *= 0x100000001b3u;
	}
	return h;
}

static uint64_t ast_ih_sym(AstIhSyms *m, uint64_t sym) {
	for (uint32_t i = 0; i < m->nsym; i++)
		if (m->syms[i] == sym)
			return i + 1;
	if (m->nsym == m->cap) {
		uint32_t ncap = m->cap ? m->cap * 2 : 32;
		uint64_t *ns = realloc(m->syms, ncap * sizeof *ns);
		if (!ns) {
			m->oom = 1;
			return 0;
		}
		m->syms = ns;
		m->cap = ncap;
	}
	m->syms[m->nsym++] = sym;
	return m->nsym;
}

static uint64_t ast_ih_node(const AstArena *a, AstLocal n, AstIhSyms *m,
														uint64_t h) {
	h = ast_ih_fold(h, a->kind[n]);
	h = ast_ih_fold(h, (uint32_t)a->op[n]);
	h = ast_ih_fold(h, (uint32_t)a->type_t[n]);
	h = ast_ih_fold(h, a->sym[n] ? ast_ih_sym(m, a->sym[n]) : 0);
	if (a->kind[n] != AST_Ref)
		h = ast_ih_fold(h, a->ival[n]);
	h = ast_ih_fold(h, a->fbits[n]);
	h = ast_ih_fold(h, a->nchild[n]);
	for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c])
		h = ast_ih_node(a, c, m, h);
	return h;
}

uint64_t ast_intention_hash(const AstArena *a, AstLocal root) {
	if (!a || !a->count)
		return 0;
	if (root == AST_NONE)
		root = ast_root(a);
	if (root >= a->count)
		return 0;
	AstIhSyms m = {NULL, 0, 0, 0};
	uint64_t h = ast_ih_node(a, root, &m, 0xcbf29ce484222325u);
	free(m.syms);
	return m.oom ? 0 : h;
}

#pragma pop_macro("malloc")
#pragma pop_macro("realloc")
#pragma pop_macro("free")

#define AST_COLOR_MAX 64

static int ast_popcount64(uint64_t x) {
	int c = 0;
	while (x) {
		x &= x - 1;
		c++;
	}
	return c;
}

int ast_color_graph(int n, const uint64_t *adj, const int *cost, int k, int *color) {
	for (int i = 0; i < n; i++)
		color[i] = -1;
	if (n <= 0 || k <= 0 || n > AST_COLOR_MAX)
		return 0;
	int deg[AST_COLOR_MAX];
	int order[AST_COLOR_MAX];
	uint64_t removed = 0;
	int top = 0;
	uint64_t all = (n < 64) ? ((uint64_t)1 << n) - 1 : ~(uint64_t)0;
	for (int i = 0; i < n; i++) {
		color[i] = -1;
		deg[i] = ast_popcount64(adj[i] & all);
	}
	for (int cnt = 0; cnt < n; cnt++) {
		int pick = -1;
		for (int i = 0; i < n; i++)
			if (!(removed & ((uint64_t)1 << i)) && deg[i] < k) {
				pick = i;
				break;
			}
		if (pick < 0)
			for (int i = 0; i < n; i++)
				if (!(removed & ((uint64_t)1 << i)) &&
						(pick < 0 || cost[i] < cost[pick]))
					pick = i;
		if (pick < 0)
			break;
		removed |= (uint64_t)1 << pick;
		order[top++] = pick;
		for (int j = 0; j < n; j++)
			if (!(removed & ((uint64_t)1 << j)) && (adj[pick] & ((uint64_t)1 << j)))
				deg[j]--;
	}
	int ncol = 0;
	for (int idx = top - 1; idx >= 0; idx--) {
		int i = order[idx];
		uint64_t used = 0;
		for (int j = 0; j < n; j++)
			if (color[j] >= 0 && (adj[i] & ((uint64_t)1 << j)))
				used |= (uint64_t)1 << color[j];
		int c = -1;
		for (int cc = 0; cc < k; cc++)
			if (!(used & ((uint64_t)1 << cc))) {
				c = cc;
				break;
			}
		color[i] = c;
		if (c >= 0)
			ncol++;
	}
	return ncol;
}

#ifdef MCC_INTERNAL

#define gjmp_addr gjmp_addr_acs
#define gjmp gjmp_acs

static int ast_env_gate(const char *name, int dflt) {
	const char *v = getenv(name);
	if (!v)
		return dflt;
	return strcmp(v, "0") != 0;
}
static int ast_env_int(const char *name, int dflt) {
	const char *v = getenv(name);
	int n;
	if (!v || !v[0])
		return dflt;
	n = atoi(v);
	return n > 0 ? n : dflt;
}
int ast_active;
static int ast_replay_env;
static int ast_replay_dump;
static int ast_graft_limit;
static int ast_graft_total;
static int ast_promo_limit;
static int ast_promo_total;
static int ast_opt_limit;
static int ast_opt_total;
static int ast_inline_node_limit = 64;
static int ast_graft_budget_max = 2048;
static int ast_cost_env;
static int ast_sethi_env;
static int ast_sethi_leaf_env;
static int ast_bitflag_env;
static int ast_bitflag_report_env;
static int ast_bitflag_min;
static int ast_cprop_join_env;
static int ast_narrow_env;
static int ast_narrow_fix_env;
static int ast_sccp_fix_env;
static int ast_dse_call_env;
static int ast_tco_ptr_env;
static int ast_cse_comm_env;
static int ast_range_env; /* MCC_AST_RANGE: fold lo<=x && x<=hi to one unsigned compare */
static int ast_divmagic_env; /* MCC_AST_DIVMAGIC: strength-reduce unsigned x/C, x%C */
static int ast_abs_env; /* MCC_AST_ABS: branchless abs from x<0?-x:x */
static int ast_reassoc_env; /* MCC_AST_REASSOC: combine (x OP c1) OP c2 */
static int ast_data_report_env; /* MCC_AST_DATA_REPORT: dump const-data records to stderr */
int ast_zero_bss_env;           /* MCC_ZERO_BSS: move all-zero .data statics into .bss */
int ast_merge_strings_env;      /* MCC_MERGE_STRINGS: pool identical rodata string literals */
static int ast_strpool_n;       /* live entries in the per-TU string-content pool */
/* Array capacity for the CSE availability window; the *effective* window is the runtime
 * `ast_cse_window` (V-cse(d): MCC_AST_CSE_WINDOW, default 64 → byte-identical; raise up to
 * AST_CSE_MAX to catch more common subexpressions in large functions). */
#define AST_CSE_MAX 256
static int ast_cse_window = 64;
/* V-cprop(d): same pattern for the const-propagation state cap — default 128 (the
 * former fixed AST_CPROP_MAX) → byte-identical; raise to catch more live constants. */
#define AST_CPROP_MAX 512
static int ast_cprop_window = 128;
/* §23 inliner budget: the recursion-depth cap. Array sized to the max; the effective
 * cap is the runtime `ast_inline_depth_max` (MCC_AST_INLINE_DEPTH, default 8 →
 * byte-identical; raise for deeper inlining — the graft/node/limit budgets are already
 * env-knobs, this completes "widen the §23 inliner budgets"). */
#define AST_INLINE_MAX_DEPTH 32
static int ast_inline_depth_max = 8;
/* V-tco: the max tail-recursion params handled (former fixed AST_TCO_MAXP=16). Array
 * cap 64; effective cap is runtime `ast_tco_maxp` (MCC_AST_TCO_MAXP, default 16 →
 * byte-identical; raise so functions with more params can be TCO'd to a loop). */
#define AST_TCO_MAXP 64
static int ast_tco_maxp = 16;
static int ast_cse_join_env;
static int ast_call_window_env;
static int ast_licm_temp_env;
static int ast_ivsr_env;
static int ast_pre_env;
static int ast_perfn_inproc_env;

#define AST_LTEMP_MAX 32
#define AST_LTEMP_PER_LOOP 8
static int ast_ltemp_off[AST_LTEMP_MAX];
static int ast_ltemp_n;
static int ast_ltemp_cur;
static int ast_color_env;
static int ast_search_worker;
static uint64_t ast_intention_acc;
static const char *ast_hash_out;

uint64_t ast_intention_value(void) {
	return ast_intention_acc;
}

#define AST_FNCFG_MAX 256
static struct {
	char name[80];
	int tmpl, promo, inl;
} ast_fncfg[AST_FNCFG_MAX];
static int ast_fncfg_n;

static void ast_fncfg_parse(void) {
	const char *s = getenv("MCC_AST_FN_CONFIG");
	ast_fncfg_n = 0;
	if (!s)
		return;
	while (*s && ast_fncfg_n < AST_FNCFG_MAX) {
		const char *name = s;
		int nlen, bits;
		while (*s && *s != '=' && *s != ';')
			s++;
		if (*s != '=')
			break;
		nlen = (int)(s - name);
		if (nlen >= 80)
			nlen = 79;
		bits = atoi(s + 1);
		while (*s && *s != ';')
			s++;
		if (*s == ';')
			s++;
		memcpy(ast_fncfg[ast_fncfg_n].name, name, nlen);
		ast_fncfg[ast_fncfg_n].name[nlen] = 0;
		ast_fncfg[ast_fncfg_n].tmpl = bits & 1;
		ast_fncfg[ast_fncfg_n].promo = (bits >> 1) & 1;
		ast_fncfg[ast_fncfg_n].inl = (bits >> 2) & 1;
		ast_fncfg_n++;
	}
}

static int ast_fncfg_find(const char *fn) {
	int i;
	if (!fn)
		return -1;
	for (i = 0; i < ast_fncfg_n; i++)
		if (!strcmp(ast_fncfg[i].name, fn))
			return i;
	return -1;
}
static int ast_templates_env;
static int ast_search_env;
static int ast_search_emitsize_env;
static int ast_search_threads_env;
static int ast_search_ordered_env;
static unsigned ast_search_seconds;
static int ast_promote_env;
static int ast_no_callful_env;
static int ast_no_callful_promo;
static int ast_inline_env;
static int ast_tmpl_folds;
static AstArena *ast_cur;
int ast_bail;
static int ast_reemit_poison;
static AstLocal ast_ret_val;
static AstLocal ast_last_return;

#define AST_VS_MAX 64
static AstLocal ast_vs[AST_VS_MAX];
static int ast_vn;
static int ast_capture;
static int ast_desync;
static int ast_base_depth;
int ast_in_op;
static int ast_in_call;
static AstLocal ast_call_pending;
static AstLocal ast_inc_pending;
static AstLocal ast_cur_bb;
#define AST_CF_MAX 32
static AstLocal ast_cf_if[AST_CF_MAX];
static AstLocal ast_cf_savebb[AST_CF_MAX];
static int ast_cf_top;
static int *ast_rp_bsym, *ast_rp_csym;
static AstLocal ast_tern[16];
static int ast_tern_top;
static AstLocal ast_lor[16];
static int ast_lor_top;

static int *ast_fconst;
static int ast_fconst_n;
static int ast_fconst_cap;
static int ast_fconst_i;
int ast_replaying;

static int *ast_locrec;
static int ast_locrec_n, ast_locrec_cap, ast_locrec_i;
static int ast_loc_low;

unsigned ast_pinned_regs;
int ast_func_has_asm;

int ast_alloc_loc(int size, int align) {
	if (ast_replaying && ast_locrec_i < ast_locrec_n) {
		loc = ast_locrec[ast_locrec_i++];
		if (loc < ast_loc_low)
			ast_loc_low = loc;
		return loc;
	}
	loc = (loc - size) & -align;
	if (loc < ast_loc_low)
		ast_loc_low = loc;
	if (ast_active && !ast_replaying) {
		if (ast_locrec_n == ast_locrec_cap) {
			ast_locrec_cap = ast_locrec_cap ? ast_locrec_cap * 2 : 16;
			ast_locrec = mcc_realloc(ast_locrec, ast_locrec_cap * sizeof *ast_locrec);
		}
		ast_locrec[ast_locrec_n++] = loc;
	}
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
void ast_hook_convert(CType *type);
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
#define AST_OP_ADDR 0x40000
#define AST_OP_MEMBER 0x40001
#define AST_OP_MEMBER_ARROW 0x40002
#define AST_OP_IMAG 0x40003
#define AST_OP_VLA 0x40004
#define AST_OP_VLA_RESTORE 0x40005
void ast_hook_indir(void);
void ast_hook_gaddrof(void);
void ast_hook_member_begin(int is_arrow);
void ast_hook_member_end(int cumofs, CType *mtype, int nonlval, int qual,
																int bcheck);
void ast_hook_imag_begin(void);
void ast_hook_imag_end(int t);
void ast_hook_builtin_complex_begin(void);
void ast_hook_builtin_complex_end(void);
void ast_hook_vla_alloc_begin(void);
void ast_hook_vla_alloc_end(CType *type, int addr, int new_save, int locorig);
void ast_hook_vla_restore(int loc);
static int ast_bad_type(int tt);
void ast_hook_ternary_begin(int c, int g);
void ast_hook_ternary_branch(int which);
void ast_hook_ternary_branch_done(int which);
void ast_hook_ternary_end(void);
void ast_hook_landor_operand(int op, int c, int first);
void ast_hook_landor_next(void);
void ast_hook_landor_end(int materialized);

static Sym *ast_sym_deferred;
static int ast_sym_defer_on;

int ast_sym_defer(Sym *sym) {
	if (!ast_sym_defer_on)
		return 0;
	sym->next = ast_sym_deferred;
	ast_sym_deferred = sym;
	return 1;
}

void ast_configure(MCCState *s1) {
	int opt_promote = 0;
#ifdef MCC_TARGET_X86_64
	opt_promote = s1->optimize >= 2;
#endif
	ast_replay_env = s1->optimize >= 1;
	ast_replay_dump = ast_env_gate("MCC_AST_REPLAY_DUMP", 0);
	ast_templates_env = ast_env_gate("MCC_AST_TEMPLATES", s1->optimize >= 1);
	ast_search_env = ast_env_gate("MCC_AST_SEARCH", 0);
	ast_search_emitsize_env = ast_env_gate("MCC_AST_SEARCH_EMITSIZE", 0);
	ast_search_threads_env = ast_env_gate("MCC_AST_SEARCH_THREADS", 0);
	ast_search_ordered_env = ast_env_gate("MCC_AST_SEARCH_ORDERED", 0);
	ast_search_seconds = s1->optimize_search_seconds;
	ast_promote_env = ast_env_gate("MCC_AST_PROMOTE", opt_promote);
	ast_no_callful_env = ast_env_gate("MCC_AST_NO_CALLFUL", 0);
	ast_inline_env = ast_env_gate("MCC_AST_INLINE",
																s1->optimize >= 3 && !s1->optimize_size);
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
	ast_cost_env = ast_env_gate("MCC_AST_COST", 0);
	ast_sethi_env = ast_env_gate("MCC_AST_SETHI", s1->optimize >= 2);
	ast_sethi_leaf_env = ast_env_gate("MCC_AST_SETHI_LEAF", 0);
	ast_bitflag_env = ast_env_gate("MCC_AST_BITFLAG", s1->optimize >= 2);
	ast_bitflag_report_env = ast_env_gate("MCC_AST_BITFLAG_REPORT", 0);
	ast_bitflag_min = ast_env_int("MCC_AST_BITFLAG", 5);
	if (ast_bitflag_min < 3)
		ast_bitflag_min = 5;
	ast_cprop_join_env = ast_env_gate("MCC_AST_CPROP_JOIN", s1->optimize >= 2);
	ast_narrow_env = ast_env_gate("MCC_AST_NARROW", s1->optimize >= 2);
	ast_narrow_fix_env = ast_env_gate("MCC_AST_NARROW_FIX", 0);
	ast_sccp_fix_env = ast_env_gate("MCC_AST_SCCP_FIX", 0);
	ast_dse_call_env = ast_env_gate("MCC_AST_DSE_CALL", 0);
	ast_tco_ptr_env = ast_env_gate("MCC_AST_TCO_PTR", 0);
	ast_cse_comm_env = ast_env_gate("MCC_AST_CSE_COMM", 0);
	ast_range_env = ast_env_gate("MCC_AST_RANGE", 0);
	ast_divmagic_env = ast_env_gate("MCC_AST_DIVMAGIC", 0);
	ast_abs_env = ast_env_gate("MCC_AST_ABS", 0);
	ast_reassoc_env = ast_env_gate("MCC_AST_REASSOC", 0);
	ast_data_report_env = ast_env_gate("MCC_AST_DATA_REPORT", 0);
	ast_zero_bss_env = ast_env_gate("MCC_ZERO_BSS", 0);
	ast_merge_strings_env = ast_env_gate("MCC_MERGE_STRINGS", 0);
	ast_strpool_n = 0; /* content pool is per translation unit */
	ast_cse_window = ast_env_int("MCC_AST_CSE_WINDOW", 64);
	if (ast_cse_window < 1)
		ast_cse_window = 1;
	if (ast_cse_window > AST_CSE_MAX)
		ast_cse_window = AST_CSE_MAX;
	ast_cprop_window = ast_env_int("MCC_AST_CPROP_WINDOW", 128);
	if (ast_cprop_window < 1)
		ast_cprop_window = 1;
	if (ast_cprop_window > AST_CPROP_MAX)
		ast_cprop_window = AST_CPROP_MAX;
	ast_inline_depth_max = ast_env_int("MCC_AST_INLINE_DEPTH", 8);
	if (ast_inline_depth_max < 1)
		ast_inline_depth_max = 1;
	if (ast_inline_depth_max > AST_INLINE_MAX_DEPTH)
		ast_inline_depth_max = AST_INLINE_MAX_DEPTH;
	ast_tco_maxp = ast_env_int("MCC_AST_TCO_MAXP", 16);
	if (ast_tco_maxp < 1)
		ast_tco_maxp = 1;
	if (ast_tco_maxp > AST_TCO_MAXP)
		ast_tco_maxp = AST_TCO_MAXP;
	ast_cse_join_env = ast_env_gate("MCC_AST_CSE_JOIN", s1->optimize >= 2);
	ast_call_window_env = ast_env_gate("MCC_AST_CALL_WINDOW", s1->optimize >= 2);
	ast_licm_temp_env = ast_env_gate("MCC_AST_LICM_TEMP", 0);
	ast_ivsr_env = ast_env_gate("MCC_AST_IVSR", 0);
	ast_pre_env = ast_env_gate("MCC_AST_PRE", 0);
	ast_perfn_inproc_env = ast_env_gate("MCC_AST_PERFN_INPROC", 0);
	ast_color_env = ast_env_gate("MCC_AST_COLOR", 0);
	ast_intention_acc = 0;
	ast_hash_out = getenv("MCC_AST_HASH_OUT");
	ast_search_worker = getenv("MCC_SEARCH_WORKER") != NULL;
	ast_fncfg_parse();
}

int ast_fconst_reuse(void) {
	if (ast_replaying && ast_fconst_i < ast_fconst_n)
		return ast_fconst[ast_fconst_i++];
	return 0;
}
void ast_fconst_record(int c) {
	if (!ast_active || ast_replaying)
		return;
	if (ast_fconst_n == ast_fconst_cap) {
		ast_fconst_cap = ast_fconst_cap ? ast_fconst_cap * 2 : 16;
		ast_fconst = mcc_realloc(ast_fconst, ast_fconst_cap * sizeof *ast_fconst);
	}
	ast_fconst[ast_fconst_n++] = c;
}
void ast_fconst_push_ref(CType *type, int fc) {
	Sym *fs = sym_push(anon_sym++, type, VT_CONST | VT_SYM, fc);
	fs->type.t |= VT_STATIC;
	vpushsym(type, fs);
	vtop->r |= VT_LVAL;
}


void ast_hook_stmt(int t) {
	if (!ast_active)
		return;
	switch (t) {
	default:
		break;
	}
}

void ast_hook_vpush(void) {
	if (!ast_capture || ast_desync || ast_in_op || ast_in_call)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel - 1 || rel > AST_VS_MAX) {
		ast_desync = 1;
		return;
	}
	int r = vtop->r, tt = vtop->type.t;
	int is_const = (r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	int is_sym = (r & VT_VALMASK) == VT_CONST && (r & VT_SYM);
	int is_local = (r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM);
	int agg_lval = (tt & VT_BTYPE) == VT_STRUCT && !(tt & VT_BITFIELD) &&
								 (is_local || is_sym);
	if ((ast_bad_type(tt) && !agg_lval) || (!is_const && !is_sym && !is_local)) {
		ast_desync = 1;
		return;
	}
	AstLocal n = ast_node(ast_cur, is_const ? AST_Literal : AST_Ref);
	ast_set_op(ast_cur, n, r);
	ast_set_type(ast_cur, n, tt, (uint64_t)(uintptr_t)vtop->type.ref);
	ast_set_ival(ast_cur, n, (uint64_t)vtop->c.i);
	ast_set_sym(ast_cur, n, (uint64_t)(uintptr_t)vtop->sym);
	if ((is_sym && vtop->sym && vtop->sym->sym_scope) ||
			(vtop->type.ref && (tt & VT_BTYPE) == VT_STRUCT && sym_scope_ex(vtop->type.ref)))
		ast_reemit_poison = 1;
	ast_vs[ast_vn++] = n;
}

static void ast_finalize_leaf(AstLocal n, SValue *sv) {
	uint16_t k = ast_kind(ast_cur, n);
	if (k != AST_Literal && k != AST_Ref)
		return;
	ast_set_op(ast_cur, n, sv->r);
	ast_set_type(ast_cur, n, sv->type.t, (uint64_t)(uintptr_t)sv->type.ref);
	ast_set_ival(ast_cur, n, (uint64_t)sv->c.i);
	ast_set_sym(ast_cur, n, (uint64_t)(uintptr_t)sv->sym);
	if (((sv->r & VT_VALMASK) == VT_CONST && (sv->r & VT_SYM) && sv->sym && sv->sym->sym_scope) ||
			(sv->type.ref && (sv->type.t & VT_BTYPE) == VT_STRUCT && sym_scope_ex(sv->type.ref)))
		ast_reemit_poison = 1;
}

static int ast_op_modeled(int op) {
	switch (op) {
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

static int ast_bad_type(int tt) {
	int bt = tt & VT_BTYPE;
	return bt == VT_STRUCT || (tt & VT_BITFIELD) ||
				 bt == VT_LDOUBLE || bt == VT_QFLOAT;
}

void ast_hook_genop(int op) {
	if (!ast_active)
		return;
	int model = ast_in_op == 0 && ast_capture && !ast_desync && !ast_in_call;
	ast_in_op++;
	if (!model)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	int bf0 = (vtop->type.t & VT_BITFIELD) && (vtop->type.t & VT_BTYPE) != VT_STRUCT;
	int bf1 =
			(vtop[-1].type.t & VT_BITFIELD) && (vtop[-1].type.t & VT_BTYPE) != VT_STRUCT;
	int cx0 = is_complex_type(&vtop->type);
	int cx1 = is_complex_type(&vtop[-1].type);
	if (!ast_op_modeled(op) || ast_vn != rel || ast_vn < 2 ||
			(ast_bad_type(vtop->type.t) && !bf0 && !cx0) ||
			(ast_bad_type(vtop[-1].type.t) && !bf1 && !cx1)) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 2], vtop - 1);
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	AstLocal b = ast_node(ast_cur, AST_Binary);
	ast_set_op(ast_cur, b, op);
	ast_add_child(ast_cur, b, ast_vs[ast_vn - 2]);
	ast_add_child(ast_cur, b, ast_vs[ast_vn - 1]);
	ast_vn -= 2;
	ast_vs[ast_vn++] = b;
}

void ast_hook_genop_end(void) {
	if (!ast_active || ast_in_op == 0)
		return;
	ast_in_op--;
	if (ast_in_op == 0 && ast_capture && !ast_desync && !ast_in_call) {
		int rel = (int)(vtop - vstack + 1) - ast_base_depth;
		if (ast_vn != rel)
			ast_desync = 1;
	}
}

void ast_hook_convert(CType *type) {
	if (!ast_capture || ast_desync || ast_in_op || ast_in_call)
		return;
	if (ast_vn < 1) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	AstLocal cvt = ast_node(ast_cur, AST_Convert);
	ast_set_type(ast_cur, cvt, type->t, (uint64_t)(uintptr_t)type->ref);
	ast_add_child(ast_cur, cvt, ast_vs[ast_vn - 1]);
	ast_vs[ast_vn - 1] = cvt;
}

void ast_hook_inc(int post, int c) {
	ast_inc_pending = AST_NONE;
	if (!ast_active || ast_desync || ast_in_op || ast_in_call)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn < 1 || ast_vn != rel) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	AstLocal u = ast_node(ast_cur, AST_Unary);
	ast_set_op(ast_cur, u, c);
	ast_set_ival(ast_cur, u, (uint64_t)post);
	ast_add_child(ast_cur, u, ast_vs[ast_vn - 1]);
	ast_vs[ast_vn - 1] = u;
	ast_inc_pending = u;
	ast_in_call = 1;
}

void ast_hook_inc_end(void) {
	if (ast_inc_pending == AST_NONE)
		return;
	ast_inc_pending = AST_NONE;
	ast_in_call = 0;
	if (ast_desync || !ast_capture)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel)
		ast_desync = 1;
}

void ast_hook_ternary_begin(int c, int g) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (c >= 0 || g || ast_in_call || ast_in_op || ast_tern_top >= 16 ||
			ast_vn < 1) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	AstLocal cn = ast_node(ast_cur, AST_If);
	ast_set_op(ast_cur, cn, 5);
	ast_add_child(ast_cur, cn, ast_vs[ast_vn - 1]);
	ast_vn--;
	ast_tern[ast_tern_top++] = cn;
	ast_in_call = 1;
}

void ast_hook_ternary_branch(int which) {
	(void)which;
	if (!ast_active || ast_desync || ast_bail || ast_tern_top < 1)
		return;
	ast_in_call = 0;
}

void ast_hook_ternary_branch_done(int which) {
	(void)which;
	if (!ast_active || ast_desync || ast_bail || ast_tern_top < 1)
		return;
	if (ast_vn < 1) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	ast_add_child(ast_cur, ast_tern[ast_tern_top - 1], ast_vs[ast_vn - 1]);
	ast_vn--;
	ast_in_call = 1;
}

void ast_hook_ternary_end(void) {
	if (!ast_active || ast_tern_top < 1)
		return;
	AstLocal cn = ast_tern[--ast_tern_top];
	ast_in_call = 0;
	if (ast_desync || ast_bail)
		return;
	if (ast_vn >= AST_VS_MAX) {
		ast_desync = 1;
		return;
	}
	ast_vs[ast_vn++] = cn;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel)
		ast_desync = 1;
}

void ast_hook_landor_operand(int op, int c, int first) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (first) {
		if (c >= 0 || ast_in_call || ast_in_op || ast_lor_top >= 16 ||
				ast_vn < 1) {
			ast_desync = 1;
			return;
		}
		AstLocal nd = ast_node(ast_cur, AST_Binary);
		ast_set_op(ast_cur, nd, op);
		ast_lor[ast_lor_top++] = nd;
	}
	if (ast_lor_top < 1)
		return;
	if (c >= 0 || ast_vn < 1) {
		ast_desync = 1;
		return;
	}
	AstLocal opnd = ast_vs[ast_vn - 1];
	if (ast_kind(ast_cur, opnd) == AST_If && ast_op(ast_cur, opnd) == 5) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	ast_add_child(ast_cur, ast_lor[ast_lor_top - 1], ast_vs[ast_vn - 1]);
	ast_vn--;
	ast_in_call = 1;
}

void ast_hook_landor_next(void) {
	if (!ast_active || ast_desync || ast_bail || ast_lor_top < 1)
		return;
	ast_in_call = 0;
}

void ast_hook_landor_end(int materialized) {
	if (!ast_active || ast_lor_top < 1)
		return;
	AstLocal nd = ast_lor[--ast_lor_top];
	ast_in_call = 0;
	if (ast_desync || ast_bail)
		return;
	if (materialized) {
		ast_desync = 1;
		return;
	}
	if (ast_vn >= AST_VS_MAX) {
		ast_desync = 1;
		return;
	}
	ast_vs[ast_vn++] = nd;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel)
		ast_desync = 1;
}

void ast_hook_if_begin(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_vn != 1 || ast_cf_top >= AST_CF_MAX) {
		ast_bail = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[0], vtop);
	AstLocal cond = ast_vs[0];
	ast_vn = 0;
	AstLocal iff = ast_node(ast_cur, AST_If);
	ast_add_child(ast_cur, iff, cond);
	AstLocal thenbb = ast_node(ast_cur, AST_BasicBlock);
	ast_add_child(ast_cur, iff, thenbb);
	ast_add_child(ast_cur, ast_cur_bb, iff);
	ast_cf_if[ast_cf_top] = iff;
	ast_cf_savebb[ast_cf_top] = ast_cur_bb;
	ast_cf_top++;
	ast_cur_bb = thenbb;
	ast_in_call = 1;
}

void ast_hook_if_gvtst_done(void) {
	if (!ast_active)
		return;
	ast_in_call = 0;
	if (ast_desync || ast_bail)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel)
		ast_desync = 1;
}

void ast_hook_if_else(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	AstLocal iff = ast_cf_if[ast_cf_top - 1];
	AstLocal elsebb = ast_node(ast_cur, AST_BasicBlock);
	ast_add_child(ast_cur, iff, elsebb);
	ast_cur_bb = elsebb;
}

void ast_hook_if_end(void) {
	if (!ast_active)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	ast_cf_top--;
	ast_cur_bb = ast_cf_savebb[ast_cf_top];
}

void ast_hook_while_begin(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_vn != 1 || ast_cf_top >= AST_CF_MAX) {
		ast_bail = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[0], vtop);
	AstLocal cond = ast_vs[0];
	ast_vn = 0;
	AstLocal loop = ast_node(ast_cur, AST_If);
	ast_set_op(ast_cur, loop, 2);
	ast_add_child(ast_cur, loop, cond);
	AstLocal body = ast_node(ast_cur, AST_BasicBlock);
	ast_add_child(ast_cur, loop, body);
	ast_add_child(ast_cur, ast_cur_bb, loop);
	ast_cf_if[ast_cf_top] = loop;
	ast_cf_savebb[ast_cf_top] = ast_cur_bb;
	ast_cf_top++;
	ast_cur_bb = body;
	ast_in_call = 1;
}

void ast_hook_while_end(void) {
	if (!ast_active)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	ast_cf_top--;
	ast_cur_bb = ast_cf_savebb[ast_cf_top];
}

void ast_hook_do_begin(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_vn != 0 || ast_cf_top >= AST_CF_MAX) {
		ast_bail = 1;
		return;
	}
	AstLocal loop = ast_node(ast_cur, AST_If);
	ast_set_op(ast_cur, loop, 4);
	ast_add_child(ast_cur, ast_cur_bb, loop);
	AstLocal body = ast_node(ast_cur, AST_BasicBlock);
	ast_add_child(ast_cur, loop, body);
	ast_cf_if[ast_cf_top] = loop;
	ast_cf_savebb[ast_cf_top] = ast_cur_bb;
	ast_cf_top++;
	ast_cur_bb = body;
}

void ast_hook_do_body_end(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	ast_cur_bb = ast_cf_savebb[ast_cf_top - 1];
}

void ast_hook_do_cond(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top < 1 || ast_vn != 1) {
		ast_bail = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[0], vtop);
	ast_add_child(ast_cur, ast_cf_if[ast_cf_top - 1], ast_vs[0]);
	ast_vn = 0;
	ast_in_call = 1;
}

void ast_hook_do_end(void) {
	if (!ast_active)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	ast_cf_top--;
	ast_cur_bb = ast_cf_savebb[ast_cf_top];
}

void ast_hook_break_continue(int is_continue) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_vn != 0) {
		ast_desync = 1;
		return;
	}
	AstLocal j = ast_node(ast_cur, AST_Jump);
	ast_set_op(ast_cur, j, is_continue ? 1 : 0);
	ast_add_child(ast_cur, ast_cur_bb, j);
}

void ast_hook_for_begin(int has_cond) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top >= AST_CF_MAX) {
		ast_bail = 1;
		return;
	}
	AstLocal loop = ast_node(ast_cur, AST_If);
	ast_set_op(ast_cur, loop, has_cond ? 3 : 5);
	ast_add_child(ast_cur, ast_cur_bb, loop);
	ast_cf_if[ast_cf_top] = loop;
	ast_cf_savebb[ast_cf_top] = ast_cur_bb;
	ast_cf_top++;
}

void ast_hook_for_cond(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top < 1 || ast_vn != 1) {
		ast_bail = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[0], vtop);
	ast_add_child(ast_cur, ast_cf_if[ast_cf_top - 1], ast_vs[0]);
	ast_vn = 0;
	ast_in_call = 1;
}

void ast_hook_for_incr_begin(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	AstLocal incrbb = ast_node(ast_cur, AST_BasicBlock);
	ast_add_child(ast_cur, ast_cf_if[ast_cf_top - 1], incrbb);
	ast_cur_bb = incrbb;
}

void ast_hook_for_incr_end(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	ast_cur_bb = ast_cf_savebb[ast_cf_top - 1];
}

void ast_hook_for_no_incr(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top < 1)
		return;
	ast_add_child(ast_cur, ast_cf_if[ast_cf_top - 1],
								ast_node(ast_cur, AST_BasicBlock));
}

void ast_hook_for_body_begin(void) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	AstLocal bodybb = ast_node(ast_cur, AST_BasicBlock);
	ast_add_child(ast_cur, ast_cf_if[ast_cf_top - 1], bodybb);
	ast_cur_bb = bodybb;
}

void ast_hook_for_end(void) {
	if (!ast_active)
		return;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	ast_cf_top--;
	ast_cur_bb = ast_cf_savebb[ast_cf_top];
}

void ast_hook_switch_begin(void) {
	ast_switch_node = AST_NONE;
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (ast_vn != 1 || ast_cf_top >= AST_CF_MAX) {
		ast_bail = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[0], vtop);
	AstLocal val = ast_vs[0];
	uint16_t vk = ast_kind(ast_cur, val);
	if ((vk != AST_Ref && vk != AST_Literal) ||
			ast_bad_type(ast_type_t(ast_cur, val))) {
		ast_bail = 1;
		return;
	}
	ast_vn = 0;
	AstLocal sw = ast_node(ast_cur, AST_If);
	ast_set_op(ast_cur, sw, 6);
	ast_add_child(ast_cur, sw, val);
	AstLocal body = ast_node(ast_cur, AST_BasicBlock);
	ast_add_child(ast_cur, sw, body);
	ast_add_child(ast_cur, ast_cur_bb, sw);
	ast_cf_if[ast_cf_top] = sw;
	ast_cf_savebb[ast_cf_top] = ast_cur_bb;
	ast_cf_top++;
	ast_cur_bb = body;
	ast_switch_node = sw;
}

void ast_hook_case(int64_t v1, int64_t v2, int type) {
	(void)type;
	if (!ast_active || ast_desync || ast_bail || ast_cf_top < 1)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel) {
		ast_desync = 1;
		return;
	}
	AstLocal m = ast_node(ast_cur, AST_Jump);
	ast_set_op(ast_cur, m, 2);
	ast_set_ival(ast_cur, m, (uint64_t)v1);
	ast_set_fbits(ast_cur, m, (uint64_t)v2);
	ast_add_child(ast_cur, ast_cur_bb, m);
}

void ast_hook_default(void) {
	if (!ast_active || ast_desync || ast_bail || ast_cf_top < 1)
		return;
	AstLocal m = ast_node(ast_cur, AST_Jump);
	ast_set_op(ast_cur, m, 3);
	ast_add_child(ast_cur, ast_cur_bb, m);
}

void ast_hook_switch_body_end(void) {
	if (!ast_active)
		return;
	ast_in_call++;
}

void ast_hook_switch_end(void) {
	if (!ast_active)
		return;
	if (ast_in_call > 0)
		ast_in_call--;
	if (ast_cf_top < 1) {
		ast_bail = 1;
		return;
	}
	ast_cf_top--;
	ast_cur_bb = ast_cf_savebb[ast_cf_top];
	ast_switch_node = AST_NONE;
}

void ast_hook_label(int v) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (nb_vla_open > 0 || cur_scope->cl.s) {
		ast_bail = 1;
		return;
	}
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel) {
		ast_desync = 1;
		return;
	}
	AstLocal m = ast_node(ast_cur, AST_Jump);
	ast_set_op(ast_cur, m, 4);
	ast_set_ival(ast_cur, m, (uint64_t)(unsigned)v);
	ast_add_child(ast_cur, ast_cur_bb, m);
}

void ast_hook_goto(int v) {
	if (!ast_active || ast_desync || ast_bail)
		return;
	if (nb_vla_open > 0 || cur_scope->cl.s) {
		ast_bail = 1;
		return;
	}
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel) {
		ast_desync = 1;
		return;
	}
	AstLocal m = ast_node(ast_cur, AST_Jump);
	ast_set_op(ast_cur, m, 5);
	ast_set_ival(ast_cur, m, (uint64_t)(unsigned)v);
	ast_add_child(ast_cur, ast_cur_bb, m);
}

void ast_hook_indir(void) {
	if (!ast_capture || ast_desync || ast_in_op || ast_in_call)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	int bad_deref = (vtop->type.t & VT_BTYPE) == VT_PTR &&
									ast_bad_type(pointed_type(&vtop->type)->t) &&
									(pointed_type(&vtop->type)->t & VT_BTYPE) != VT_STRUCT;
	if (ast_vn < 1 || ast_vn != rel || bad_deref) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	AstLocal ld = ast_node(ast_cur, AST_Load);
	ast_add_child(ast_cur, ld, ast_vs[ast_vn - 1]);
	ast_vs[ast_vn - 1] = ld;
}

void ast_hook_gaddrof(void) {
	if (!ast_capture || ast_desync || ast_in_op || ast_in_call)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn < 1 || ast_vn != rel) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	AstLocal u = ast_node(ast_cur, AST_Unary);
	ast_set_op(ast_cur, u, AST_OP_ADDR);
	ast_set_type(ast_cur, u, vtop->type.t, (uint64_t)(uintptr_t)vtop->type.ref);
	ast_add_child(ast_cur, u, ast_vs[ast_vn - 1]);
	ast_vs[ast_vn - 1] = u;
}

void ast_hook_member_begin(int is_arrow) {
	ast_member_cap = 0;
	if (!ast_active)
		return;
	if (ast_capture && !ast_desync && !ast_in_op && !ast_in_call && ast_vn >= 1) {
		int rel = (int)(vtop - vstack + 1) - ast_base_depth;
		if (ast_vn == rel) {
			ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
			ast_member_cap = 1;
			ast_member_arrow = is_arrow;
		} else {
			ast_desync = 1;
		}
	}
	ast_in_call++;
}

void ast_hook_member_end(int cumofs, CType *mtype, int nonlval, int qual,
																int bcheck) {
	if (!ast_active)
		return;
	if (ast_in_call > 0)
		ast_in_call--;
	if (!ast_member_cap)
		return;
	ast_member_cap = 0;
	if (ast_desync)
		return;
	int mt_bf_ok = (mtype->t & VT_BITFIELD) && (mtype->t & VT_BTYPE) != VT_STRUCT;
	if (qual || bcheck || (ast_bad_type(mtype->t) && !mt_bf_ok)) {
		ast_desync = 1;
		return;
	}
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel) {
		ast_desync = 1;
		return;
	}
	AstLocal m = ast_node(ast_cur, AST_Unary);
	ast_set_op(ast_cur, m,
						 ast_member_arrow ? AST_OP_MEMBER_ARROW : AST_OP_MEMBER);
	ast_set_ival(ast_cur, m, (uint64_t)(unsigned)cumofs);
	ast_set_type(ast_cur, m, mtype->t, (uint64_t)(uintptr_t)mtype->ref);
	ast_set_fbits(ast_cur, m, (uint64_t)(unsigned)nonlval);
	ast_add_child(ast_cur, m, ast_vs[ast_vn - 1]);
	ast_vs[ast_vn - 1] = m;
}

void ast_hook_imag_begin(void) {
	ast_imag_cap = 0;
	if (!ast_active)
		return;
	if (ast_capture && !ast_desync && !ast_in_op && !ast_in_call && ast_vn >= 1) {
		int rel = (int)(vtop - vstack + 1) - ast_base_depth;
		if (ast_vn == rel && ast_kind(ast_cur, ast_vs[ast_vn - 1]) == AST_Literal) {
			ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
			ast_imag_cap = 1;
		} else {
			ast_desync = 1;
		}
	}
	ast_in_op++;
}

void ast_hook_imag_end(int t) {
	if (!ast_active)
		return;
	if (ast_in_op > 0)
		ast_in_op--;
	if (!ast_imag_cap)
		return;
	ast_imag_cap = 0;
	if (ast_desync)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel) {
		ast_desync = 1;
		return;
	}
	AstLocal m = ast_node(ast_cur, AST_Unary);
	ast_set_op(ast_cur, m, AST_OP_IMAG);
	ast_set_ival(ast_cur, m, (uint64_t)(unsigned)t);
	ast_add_child(ast_cur, m, ast_vs[ast_vn - 1]);
	ast_vs[ast_vn - 1] = m;
}

void ast_hook_builtin_complex_begin(void) {
	ast_bcplx_cap = 0;
	if (!ast_active)
		return;
	if (ast_capture && !ast_desync && !ast_in_op && !ast_in_call) {
		int rel = (int)(vtop - vstack + 1) - ast_base_depth;
		if (ast_vn == rel)
			ast_bcplx_cap = 1;
		else
			ast_desync = 1;
	}
	ast_in_op++;
}

void ast_hook_builtin_complex_end(void) {
	if (!ast_active)
		return;
	if (ast_in_op > 0)
		ast_in_op--;
	if (!ast_bcplx_cap)
		return;
	ast_bcplx_cap = 0;
	if (ast_desync)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel - 1 || rel > AST_VS_MAX) {
		ast_desync = 1;
		return;
	}
	int r = vtop->r;
	if (!((r & VT_VALMASK) == VT_CONST && (r & VT_SYM) && (r & VT_LVAL))) {
		ast_desync = 1;
		return;
	}
	AstLocal n = ast_node(ast_cur, AST_Ref);
	ast_set_op(ast_cur, n, r);
	ast_set_type(ast_cur, n, vtop->type.t, (uint64_t)(uintptr_t)vtop->type.ref);
	ast_set_ival(ast_cur, n, (uint64_t)vtop->c.i);
	ast_set_sym(ast_cur, n, (uint64_t)(uintptr_t)vtop->sym);
	ast_vs[ast_vn++] = n;
}

void ast_hook_vla_alloc_begin(void) {
	if (!ast_active)
		return;
	ast_in_op++;
}

void ast_hook_vla_alloc_end(CType *type, int addr, int new_save,
																	 int locorig) {
#if defined MCC_TARGET_PE && defined MCC_TARGET_X86_64
	if (ast_active && ast_in_op > 0)
		ast_in_op--;
	if (ast_active)
		ast_desync = 1;
	(void)type, (void)addr, (void)new_save, (void)locorig;
#else
	if (!ast_active)
		return;
	if (ast_in_op > 0)
		ast_in_op--;
	if (ast_desync || ast_bail || ast_in_call)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel) {
		ast_desync = 1;
		return;
	}
	AstLocal n = ast_node(ast_cur, AST_Unary);
	ast_set_op(ast_cur, n, AST_OP_VLA);
	ast_set_type(ast_cur, n, type->t, (uint64_t)(uintptr_t)type->ref);
	ast_set_ival(ast_cur, n, (uint64_t)(int64_t)addr);
	ast_set_sym(ast_cur, n, (uint64_t)(int64_t)locorig);
	ast_set_fbits(ast_cur, n, (uint64_t)(unsigned)new_save);
	ast_add_child(ast_cur, ast_cur_bb, n);
#endif
}

void ast_hook_vla_restore(int loc) {
	if (!ast_active || ast_desync || ast_bail || loc == 0)
		return;
	if (NODATA_WANTED)
		return;
	if (ast_last_return != AST_NONE) {
		if (ast_ival(ast_cur, ast_last_return) != 0) {
			ast_desync = 1;
			return;
		}
		ast_set_ival(ast_cur, ast_last_return, (uint64_t)(int64_t)loc);
		return;
	}
	AstLocal n = ast_node(ast_cur, AST_Unary);
	ast_set_op(ast_cur, n, AST_OP_VLA_RESTORE);
	ast_set_ival(ast_cur, n, (uint64_t)(int64_t)loc);
	ast_add_child(ast_cur, ast_cur_bb, n);
}

void ast_hook_call_begin(int nb_args, int is_struct_ret, int ret_nregs,
																int variadic) {
	ast_call_pending = AST_NONE;
	if (!ast_capture || ast_desync || ast_in_op || ast_in_call)
		return;
	if (nocode_wanted) {
		ast_desync = 1;
		return;
	}
	(void)variadic;
	int need = nb_args + 1;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel || ast_vn < need) {
		ast_desync = 1;
		return;
	}
	for (int i = 0; i < nb_args; i++) {
		CType *at = &(vtop - nb_args + 1 + i)->type;
		if (ast_bad_type(at->t) &&
				((at->t & VT_BTYPE) != VT_STRUCT || is_complex_type(at))) {
			ast_desync = 1;
			return;
		}
		AstLocal an = ast_vs[ast_vn - nb_args + i];
		int ak = ast_op(ast_cur, an);
		if (ast_kind(ast_cur, an) == AST_Binary &&
				(ak == TOK_LAND || ak == TOK_LOR)) {
			ast_desync = 1;
			return;
		}
	}
	if (ast_kind(ast_cur, ast_vs[ast_vn - need]) != AST_Ref) {
		ast_desync = 1;
		return;
	}
	for (int i = 0; i < need; i++)
		ast_finalize_leaf(ast_vs[ast_vn - need + i], vtop - nb_args + i);
	AstLocal inv = ast_node(ast_cur, AST_Invoke);
	for (int i = 0; i < need; i++)
		ast_add_child(ast_cur, inv, ast_vs[ast_vn - need + i]);
	ast_vn -= need;
	ast_call_pending = inv;
	ast_in_call = 1;
}

void ast_hook_call_end(void) {
	if (ast_call_pending == AST_NONE)
		return;
	AstLocal inv = ast_call_pending;
	ast_call_pending = AST_NONE;
	ast_in_call = 0;
	if (!ast_capture || ast_desync)
		return;
	if (vtop->r2 != VT_CONST) {
		ast_desync = 1;
		return;
	}
	ast_set_op(ast_cur, inv, vtop->r);
	ast_set_type(ast_cur, inv, vtop->type.t, (uint64_t)(uintptr_t)vtop->type.ref);
	ast_set_ival(ast_cur, inv, (uint64_t)vtop->c.i);
	ast_set_sym(ast_cur, inv, (uint64_t)(uintptr_t)vtop->sym);
	if (ast_vn >= AST_VS_MAX) {
		ast_desync = 1;
		return;
	}
	ast_vs[ast_vn++] = inv;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel)
		ast_desync = 1;
}

void ast_hook_call_effect_end(void) {
	if (ast_call_pending == AST_NONE)
		return;
	AstLocal inv = ast_call_pending;
	ast_call_pending = AST_NONE;
	ast_in_call = 0;
	if (!ast_capture || ast_desync)
		return;
	ast_set_type(ast_cur, inv, VT_VOID, 0);
	ast_add_child(ast_cur, ast_cur_bb, inv);
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	if (ast_vn != rel)
		ast_desync = 1;
}

void ast_hook_vswap(void) {
	if (!ast_capture || ast_desync || ast_in_op || ast_in_call)
		return;
	if (ast_vn < 2) {
		ast_desync = 1;
		return;
	}
	AstLocal t = ast_vs[ast_vn - 1];
	ast_vs[ast_vn - 1] = ast_vs[ast_vn - 2];
	ast_vs[ast_vn - 2] = t;
}

void ast_hook_vpop(void) {
	if (!ast_capture || ast_desync || ast_in_op || ast_in_call)
		return;
	if (ast_vn < 1) {
		ast_desync = 1;
		return;
	}
	AstLocal top = ast_vs[ast_vn - 1];
	uint16_t tk = ast_kind(ast_cur, top);
	if ((tk == AST_Invoke || tk == AST_Unary) &&
			ast_parent(ast_cur, top) == AST_NONE)
		ast_add_child(ast_cur, ast_cur_bb, top);
	ast_vn--;
}

void ast_hook_vstore(void) {
	if (!ast_active)
		return;
	int model = ast_in_op == 0 && ast_capture && !ast_desync && !ast_in_call;
	ast_in_op++;
	if (!model)
		return;
	int rel = (int)(vtop - vstack + 1) - ast_base_depth;
	int agg_store = (vtop->type.t & VT_BTYPE) == VT_STRUCT &&
									(vtop[-1].type.t & VT_BTYPE) == VT_STRUCT;
	int bf_store = (vtop[-1].type.t & VT_BITFIELD) &&
								 (vtop[-1].type.t & VT_BTYPE) != VT_STRUCT && !ast_bad_type(vtop->type.t);
	if (ast_vn != rel || ast_vn < 2 ||
			((ast_bad_type(vtop->type.t) || ast_bad_type(vtop[-1].type.t)) &&
			 !agg_store && !bf_store)) {
		ast_desync = 1;
		return;
	}
	ast_finalize_leaf(ast_vs[ast_vn - 2], vtop - 1);
	ast_finalize_leaf(ast_vs[ast_vn - 1], vtop);
	AstLocal value = ast_vs[ast_vn - 1];
	AstLocal lval = ast_vs[ast_vn - 2];
	AstLocal st = ast_node(ast_cur, AST_Store);
	ast_add_child(ast_cur, st, lval);
	ast_add_child(ast_cur, st, value);
	ast_add_child(ast_cur, ast_cur_bb, st);
	ast_vs[ast_vn - 2] = value;
	ast_vn--;
}

void ast_hook_vstore_end(void) {
	if (!ast_active || ast_in_op == 0)
		return;
	ast_in_op--;
	if (ast_in_op == 0 && ast_capture && !ast_desync && !ast_in_call) {
		int rel = (int)(vtop - vstack + 1) - ast_base_depth;
		if (ast_vn != rel)
			ast_desync = 1;
	}
}

void ast_hook_ret_expr_done(void) {
	ast_ret_val = AST_NONE;
	if (!ast_capture || ast_in_call)
		return;
	ast_in_call = 1;
	if (!ast_desync && ast_vn == 1) {
		ast_finalize_leaf(ast_vs[0], vtop);
		ast_ret_val = ast_vs[0];
		ast_vn = 0;
	} else {
		ast_desync = 1;
	}
}

void ast_hook_return(int has_val) {
	ast_last_return = AST_NONE;
	if (!ast_active)
		return;
	if (!has_val || ast_ret_val == AST_NONE) {
		ast_bail = 1;
		return;
	}
	AstLocal bb = ast_cur_bb;
	AstLocal ret = ast_node(ast_cur, AST_Return);
	ast_add_child(ast_cur, ret, ast_ret_val);
	ast_add_child(ast_cur, bb, ret);
	ast_ret_val = AST_NONE;
	ast_last_return = ret;
}

void ast_hook_return_jmp(int jumps) {
	if (!ast_active)
		return;
	if (ast_last_return != AST_NONE)
		ast_set_op(ast_cur, ast_last_return, jumps ? 1 : 0);
	ast_last_return = AST_NONE;
	ast_in_call = 0;
	if (!ast_desync && !ast_bail) {
		int rel = (int)(vtop - vstack + 1) - ast_base_depth;
		if (ast_vn != rel)
			ast_desync = 1;
	}
}

void ast_hook_implicit_return(void) {
	if (!ast_active)
		return;
	ast_capture = 0;
	AstLocal bb = ast_cur_bb;
	AstLocal ret = ast_node(ast_cur, AST_Return);
	AstLocal lit = ast_node(ast_cur, AST_Literal);
	ast_set_op(ast_cur, lit, VT_CONST);
	ast_set_ival(ast_cur, lit, 0);
	ast_set_type(ast_cur, lit, VT_INT, 0);
	ast_add_child(ast_cur, ret, lit);
	ast_add_child(ast_cur, bb, ret);
}

#if MCC_CONFIG_OPTIMIZER && defined(MCC_TARGET_X86_64)
#define AST_PROMO_MAX 5
static const int ast_promo_caller[3] = {10, 9, 8};
static const int ast_promo_callee[5] = {3, 12, 13, 14, 15};
static const int ast_promo_xmm[2] = {22, 23};
#define AST_PROMO_SLOTS (AST_PROMO_MAX * 8)
static int ast_promo_off[AST_PROMO_SLOTS];
static int ast_promo_typ[AST_PROMO_SLOTS];
static int ast_promo_reg[AST_PROMO_SLOTS];
static int ast_promo_n;
static int ast_promo_callful;
static int ast_promo_regpool_at(int i) {
	return ast_promo_reg[i];
}
#endif

#if MCC_CONFIG_OPTIMIZER
static void ast_replay_value(AstArena *a, AstLocal n);
static void ast_replay_bb(AstArena *a, AstLocal bb);
static int ast_local_is_readonly(AstArena *a, int off);
#define AST_INLINE_MAX 512
#define AST_INLINE_MAX_PARAMS 6
static struct AstInlineFn {
	void *sym;
	AstArena *ast;
	int frame_size;
	int nparams;
	int param_off[AST_INLINE_MAX_PARAMS];
	int param_typ[AST_INLINE_MAX_PARAMS];
	void *param_ref[AST_INLINE_MAX_PARAMS];
	int param_size[AST_INLINE_MAX_PARAMS];
	int param_stack[AST_INLINE_MAX_PARAMS];
	int graftable;
} ast_inline_pool[AST_INLINE_MAX];
static int ast_inline_n;

static struct AstReemitFn {
	Sym *sym;
	AstArena *ast;
	int inline_n_at_gen;
} ast_reemit_pool[AST_INLINE_MAX];
static int ast_reemit_n;

static int ast_inline_cap_np;
static int ast_inline_cap_off[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_typ[AST_INLINE_MAX_PARAMS];
static void *ast_inline_cap_ref[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_size[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_stack[AST_INLINE_MAX_PARAMS];
static int ast_inline_cap_ok;

static AstArena *ast_inline_lookup(void *sym) {
	for (int i = 0; i < ast_inline_n; i++)
		if (ast_inline_pool[i].sym == sym)
			return ast_inline_pool[i].ast;
	return NULL;
}

static int ast_fn_inlinable(AstArena *a, Sym *sym) {
	if (!ast_inline_env || ast_bail || ast_desync)
		return 0;
	if (!(sym->type.t & VT_STATIC))
		return 0;
	if (sym->type.ref->f.func_type != FUNC_NEW)
		return 0;
	if (sym->type.ref->f.func_noinl)
		return 0;
	AstLocal nn = ast_count(a);
	if (nn == 0 || nn > ast_inline_node_limit)
		return 0;
	for (AstLocal n = 0; n < nn; n++) {
		uint16_t k = ast_kind(a, n);
		if (k == AST_Unary &&
				(ast_op(a, n) == AST_OP_VLA || ast_op(a, n) == AST_OP_VLA_RESTORE))
			return 0;
		if (k == AST_Invoke) {
			AstLocal ce = ast_first_child(a, n);
			void *cs = ce != AST_NONE ? (void *)(uintptr_t)ast_sym(a, ce) : NULL;
			if (cs) {
				const char *cn = get_tok_str(((Sym *)cs)->v, NULL);
				if (cn && strstr(cn, "setjmp"))
					return 0;
			}
		}
	}
	return 1;
}

static void ast_inline_capture(Sym *fnsym) {
	ast_inline_cap_np = 0;
	ast_inline_cap_ok = 0;
	if (!ast_inline_env)
		return;
	int n = 0;
	for (Sym *p = fnsym->type.ref->next; p; p = p->next) {
		int v = p->v & ~SYM_FIELD;
		if (v < TOK_IDENT || n >= AST_INLINE_MAX_PARAMS)
			return;
		Sym *ls = sym_find(v);
		if (!ls || (ls->r & VT_VALMASK) != VT_LOCAL)
			return;
		int bt = ls->type.t & VT_BTYPE;
		if ((bt != VT_INT && bt != VT_LLONG && bt != VT_PTR &&
				 bt != VT_FLOAT && bt != VT_DOUBLE && bt != VT_STRUCT) ||
				(ls->type.t & (VT_ARRAY | VT_VLA)))
			return;
		if (bt == VT_STRUCT && is_complex_type(&ls->type))
			return;
		if (bt == VT_PTR && ls->type.ref &&
				((((Sym *)ls->type.ref)->type.t & VT_VLA) ||
				 ((Sym *)ls->type.ref)->vla_array_str))
			return;
		int palign, psize = type_size(&ls->type, &palign);
		if (psize < 0)
			return;
		ast_inline_cap_off[n] = (int)ls->c;
		ast_inline_cap_typ[n] = ls->type.t;
		ast_inline_cap_ref[n] = ls->type.ref;
		ast_inline_cap_size[n] = psize;
		ast_inline_cap_stack[n] = (int)ls->c >= 0;
		n++;
	}
	ast_inline_cap_np = n;
	ast_inline_cap_ok = 1;
}

static int ast_inline_graftable(AstArena *a) {
	AstLocal root = ast_root(a);
	if (ast_kind(a, root) != AST_BasicBlock)
		return 0;
	AstLocal nn = ast_count(a);
	int totret = 0;
	for (AstLocal n = 0; n < nn; n++) {
		uint16_t k = ast_kind(a, n);
		if (k == AST_Return) {
			totret++;
			if (ast_nchild(a, n) < 1)
				return 0;
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
static int ast_fn_faithful;
static int ast_fn_tco;
static CType ast_graft_rt;
static int ast_inline_ret_sym;
static int ast_inline_ret_slot;
static void *ast_inline_stack[AST_INLINE_MAX_DEPTH];
static int ast_graft_budget;
static int ast_inline_depth;

static int ast_inline_graft(AstArena *a, AstLocal n) {
	if (!ast_inline_active)
		return 0;
	AstLocal cref = ast_child(a, n, 0);
	if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
		return 0;
	void *csym = (void *)(uintptr_t)ast_sym(a, cref);
	struct AstInlineFn *e = NULL;
	for (int i = 0; i < ast_inline_n; i++)
		if (ast_inline_pool[i].sym == csym) {
			e = &ast_inline_pool[i];
			break;
		}
	if (!e || !e->graftable)
		return 0;
	int nargs = (int)ast_nchild(a, n) - 1;
	int hidden = ((ast_type_t(a, n) & VT_BTYPE) == VT_STRUCT && nargs == e->nparams + 1) ? 1 : 0;
	if (nargs - hidden != e->nparams)
		return 0;
	if (ast_inline_depth >= ast_inline_depth_max)
		return 0;
	for (int i = 0; i < ast_inline_depth; i++)
		if (ast_inline_stack[i] == csym)
			return 0;
	if (ast_graft_budget < (int)ast_count(e->ast))
		return 0;
	if (ast_graft_limit >= 0 && ast_graft_total >= ast_graft_limit)
		return 0;
	ast_graft_total++;
	ast_graft_budget -= (int)ast_count(e->ast);
	ast_inline_stack[ast_inline_depth++] = csym;
	int hi = 0;
	for (int i = 0; i < e->nparams; i++) {
		CType pt;
		pt.t = e->param_typ[i];
		pt.ref = (Sym *)e->param_ref[i];
		int pa, ps = type_size(&pt, &pa);
		if (ps < 1)
			ps = 1;
		if (e->param_off[i] + ps > hi)
			hi = e->param_off[i] + ps;
	}
	int bias = hi > 0 ? ((loc - hi) & -16) : loc;
	loc = bias - e->frame_size;
	int nsub = 0, suboff[AST_INLINE_MAX_PARAMS];
	SValue subval[AST_INLINE_MAX_PARAMS];
	for (int i = 0; i < e->nparams; i++) {
		int dst = e->param_off[i] + bias;
		AstLocal arg = ast_child(a, n, hidden + i + 1);
		AstLocal cbase = arg;
		while (ast_kind(a, cbase) == AST_Convert && ast_nchild(a, cbase) == 1)
			cbase = ast_first_child(a, cbase);
		int pbt = e->param_typ[i] & VT_BTYPE, cbt = ast_type_t(a, cbase) & VT_BTYPE;
		if (ast_templates_env && ast_kind(a, cbase) == AST_Literal &&
				(ast_op(a, cbase) & VT_VALMASK) == VT_CONST && !(ast_op(a, cbase) & VT_SYM) &&
				(pbt == VT_INT || pbt == VT_LLONG) && (cbt == VT_INT || cbt == VT_LLONG) &&
				ast_local_is_readonly(e->ast, e->param_off[i])) {
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
		SValue slot;
		memset(&slot, 0, sizeof slot);
		slot.type.t = e->param_typ[i];
		slot.type.ref = e->param_ref[i];
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
	ast_graft_rt.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
	int ral, rsz = type_size(&ast_graft_rt, &ral);
	if (rsz < 1)
		rsz = 8;
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
	if (ast_replay_dump) {
		fprintf(stderr, "[ast-inline] grafted %s\n", get_tok_str(((Sym *)csym)->v, NULL));
		if (nsub)
			fprintf(stderr, "[ast-inline] specialized %s (%d const arg%s)\n",
							get_tok_str(((Sym *)csym)->v, NULL), nsub, nsub == 1 ? "" : "s");
	}
	return 1;
}

static int ast_has_graftable_call(AstArena *a) {
	if (!ast_inline_env)
		return 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Invoke)
			continue;
		AstLocal cref = ast_child(a, n, 0);
		if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
			continue;
		void *csym = (void *)(uintptr_t)ast_sym(a, cref);
		for (int i = 0; i < ast_inline_n; i++)
			if (ast_inline_pool[i].sym == csym && ast_inline_pool[i].graftable)
				return 1;
	}
	return 0;
}

static int ast_inline_retain(AstArena *a, Sym *sym) {
	if (!ast_fn_inlinable(a, sym) || ast_inline_n >= AST_INLINE_MAX ||
			ast_inline_lookup(sym))
		return 0;
	struct AstInlineFn *e = &ast_inline_pool[ast_inline_n++];
	e->sym = sym;
	e->ast = a;
	e->frame_size = loc < 0 ? -loc : 0;
	e->nparams = ast_inline_cap_np;
	for (int i = 0; i < ast_inline_cap_np; i++) {
		e->param_off[i] = ast_inline_cap_off[i];
		e->param_typ[i] = ast_inline_cap_typ[i];
		e->param_ref[i] = ast_inline_cap_ref[i];
		e->param_size[i] = ast_inline_cap_size[i];
		e->param_stack[i] = ast_inline_cap_stack[i];
	}
	e->graftable = ast_inline_cap_ok && !ast_fn_tco && ast_inline_graftable(a);
	if (ast_replay_dump)
		fprintf(stderr, "[ast-inline] candidate %s (%d nodes, %d params, frame %d, %s)\n",
						get_tok_str(sym->v, NULL), (int)ast_count(a), e->nparams, e->frame_size,
						e->graftable ? "graftable" : "retained-only");
	return 1;
}

static int ast_reemit_retain(AstArena *a, Sym *sym) {
	if (!ast_inline_env || ast_bail || ast_desync || ast_reemit_poison ||
			ast_reemit_n >= AST_INLINE_MAX)
		return 0;
	AstLocal nn = ast_count(a);
	int has_static_call = 0;
	for (AstLocal n = 0; n < nn && !has_static_call; n++) {
		if (ast_kind(a, n) != AST_Invoke)
			continue;
		AstLocal ce = ast_first_child(a, n);
		void *cs = ce != AST_NONE ? (void *)(uintptr_t)ast_sym(a, ce) : NULL;
		if (cs && (((Sym *)cs)->type.t & VT_STATIC) &&
				(((Sym *)cs)->type.t & VT_BTYPE) == VT_FUNC)
			has_static_call = 1;
	}
	if (!has_static_call)
		return 0;
	ast_reemit_pool[ast_reemit_n].sym = sym;
	ast_reemit_pool[ast_reemit_n].ast = a;
	ast_reemit_pool[ast_reemit_n].inline_n_at_gen = ast_inline_n;
	ast_reemit_n++;
	return 1;
}

static int ast_reemit_has_forward(struct AstReemitFn *f) {
	AstArena *a = f->ast;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Invoke)
			continue;
		AstLocal ce = ast_first_child(a, n);
		void *cs = ce != AST_NONE ? (void *)(uintptr_t)ast_sym(a, ce) : NULL;
		if (!cs)
			continue;
		for (int i = f->inline_n_at_gen; i < ast_inline_n; i++)
			if (ast_inline_pool[i].sym == cs && ast_inline_pool[i].graftable)
				return 1;
	}
	return 0;
}
static int ast_ref_is_local_off(AstArena *a, AstLocal n, int off) {
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		return 0;
	int r = ast_op(a, n);
	return (r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM) &&
				 (int)(int64_t)ast_ival(a, n) == off;
}

static int ast_local_is_readonly_scan(AstArena *a, int off) {
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		uint16_t k = ast_kind(a, n);
		if (k == AST_Store && ast_ref_is_local_off(a, ast_child(a, n, 0), off))
			return 0;
		if (k == AST_Unary && ast_ref_is_local_off(a, ast_first_child(a, n), off))
			return 0;
	}
	return 1;
}

/*
 * Side-car def/use projection (rollout step 1). One O(n) sweep records, per
 * VT_LOCAL slot offset, whether it is written (Store target or any Unary over an
 * lvalue local ref) and whether it escapes (address/member Unary over the slot).
 * The two whole-arena scanners (ast_local_is_readonly, ast_cprop_escapes) then
 * answer from the table in O(k) over distinct slots instead of O(n) per call.
 * The table is rebuilt lazily when a->epoch changes; every arena mutator bumps
 * epoch, so a query never sees a stale answer. On overflow it falls back to the
 * scan. Pure accelerator: it must agree with the scanners bit-for-bit, which the
 * MCC_CONFIG_AST_SHADOW build asserts on every query.
 */
/*
 * M5 const-data visibility side-car (roadmap step M5, first bounded/byte-neutral step).
 * Initialized static/global data is written into Section->data at PARSE time, OUTSIDE
 * the per-function AST capture window (init_putv / decl_initializer_alloc, mccgen.c), so
 * it is never recorded as AstKind nodes and a pass cannot yet rewrite it. This side-car
 * records one entry per emitted object — (section, offset, size, is_rodata) — read-only,
 * changing no bytes, mirroring the def/use side-car (ast_du_*) below. It makes the const-
 * data footprint queryable (a diagnostic today; M4 data-size scoring / M6 datacomp later);
 * bringing the bytes *under* the AST for rewrite (a data AstKind + re-emit pass) is the
 * next, non-neutral step this visibility unblocks.
 */
#define AST_DATA_CAP 4096
typedef struct {
	void *sec;
	long off;
	long size;
	int is_ro;
} AstDataRec;
static AstDataRec ast_data_recs[AST_DATA_CAP];
static int ast_data_n;
static long ast_data_total_ro; /* total .rodata (const) bytes */
static long ast_data_total_rw; /* total .data (mutable init) bytes */

/* M6 datacomp candidate identification (read-only): the visibility side-car searches a
 * codec CHAIN (combo_pipeline_search, mcccombo.h — the same combo_run permutation engine
 * the -O4 gate search rides) over each object's just-emitted bytes to estimate how much a
 * datacomp pass would save. A chain (e.g. lzss-then-rle) can beat any single codec, so the
 * estimate is tighter than a best-of-3 pack, and the winning chain is exactly the recipe
 * M6's rewrite step would emit. This changes no emitted bytes — it only measures — and
 * answers "which const objects are worth compressing, and with what codec chain?". Objects
 * below AST_DATA_PACKMIN are skipped (codec headers dominate); above AST_DATA_PACKMAX the
 * estimate is skipped to bound the ping-pong scratch buffers. */
#define AST_DATA_PACKMIN 32
#define AST_DATA_PACKMAX 8192
#define AST_DATA_PIPE_DEPTH 3
static unsigned char ast_data_pipe_a[AST_DATA_PACKMAX * 2];
static unsigned char ast_data_pipe_b[AST_DATA_PACKMAX * 2];
static unsigned char ast_data_pipe_c[AST_DATA_PACKMAX * 2]; /* holds the compressed blob */
static int ast_data_ncompressible; /* objects whose best chain packs to <50%, round-trip OK */
static long ast_data_saved;        /* estimated bytes a datacomp pass could reclaim */
static int ast_data_nlossy;        /* chains that FAILED to round-trip (codec bug signal) */
static int ast_data_nzerobss;      /* all-zero writable objects (belong in .bss, not .data) */
static long ast_data_zerobytes;    /* disk bytes reclaimable by moving them to .bss (NOBITS) */

/* Detect an all-zero object emitted into a writable PROGBITS section (.data). C11 6.7.9:
 * a static object with an all-zero initializer is identical to one with none, so it belongs
 * in .bss (NOBITS — zero disk bytes) rather than occupying `size` zero bytes on disk. This
 * is the read-only ANALYSIS half of a future zero-init-placement optimization (mirrors the
 * M6 estimate→rewrite split): it quantifies the reclaimable disk without moving anything.
 * .rodata all-zero objects are excluded — const zeros are a separate placement question. */
int ast_data_all_zero(void *sec, long off, long size) {
	const unsigned char *bytes = ((Section *)sec)->data + off;
	long i;
	for (i = 0; i < size; i++)
		if (bytes[i])
			return 0;
	return 1;
}

/* Content pool for -fmerge-constants-style string-literal sharing. Keyed on the exact bytes
 * plus (size, align) so a wide literal never aliases a byte literal and a reused slot always
 * satisfies the dup's alignment. Linear probe with a hash prefilter — bounded by AST_STRPOOL_CAP
 * per TU. Offsets index rodata_section->data, valid for the whole TU (rodata only grows). */
#define AST_STRPOOL_CAP 8192
typedef struct {
	uint32_t hash;
	long addr;
	long size;
	int align;
} AstStrRec;
static AstStrRec ast_strpool[AST_STRPOOL_CAP];

static uint32_t ast_str_hash(const unsigned char *b, long n) {
	uint32_t h = 2166136261u;
	long i;
	for (i = 0; i < n; i++) {
		h ^= b[i];
		h *= 16777619u;
	}
	return h;
}

long ast_strpool_find_or_add(void *sec, long addr, long size, int align) {
	unsigned char *data = ((Section *)sec)->data;
	const unsigned char *bytes = data + addr;
	uint32_t h = ast_str_hash(bytes, size);
	int i;
	/* Reuse the exact bytes OR a suffix of a longer pooled literal (C11 6.5.2.5 footnote
	 * permits sharing literals with overlapping representations). A pooled entry r covers
	 * this one if this literal's bytes equal r's last `size` bytes AND that interior offset
	 * meets this literal's alignment — then references (symbol + addend) re-home cleanly into
	 * the middle of r. Exact match is the r->size == size case (off == r->addr). */
	for (i = 0; i < ast_strpool_n; i++) {
		AstStrRec *r = &ast_strpool[i];
		long off;
		if (r->size < size)
			continue;
		off = r->addr + (r->size - size);
		if (off % align != 0)
			continue;
		if ((r->size == size ? r->hash == h : 1) &&
				memcmp(data + off, bytes, (size_t)size) == 0) {
			MCC_TRACE("strpool %s addr=%ld size=%ld -> shared=%ld (in %ld@%ld)\n",
								off == r->addr ? "exact" : "suffix", addr, size, off, r->size, r->addr);
			return off;
		}
	}
	if (ast_strpool_n < AST_STRPOOL_CAP) {
		ast_strpool[ast_strpool_n].hash = h;
		ast_strpool[ast_strpool_n].addr = addr;
		ast_strpool[ast_strpool_n].size = size;
		ast_strpool[ast_strpool_n].align = align;
		ast_strpool_n++;
	}
	return -1;
}

static void ast_data_zero_check(void *sec, long off, long size, int is_ro) {
	if (is_ro || size <= 0)
		return;
	if (!ast_data_all_zero(sec, off, size))
		return;
	ast_data_nzerobss++;
	ast_data_zerobytes += size;
	MCC_TRACE("data zero off=%ld size=%ld (.bss-movable; reclaimable=%ld)\n", off, size,
						ast_data_zerobytes);
	if (ast_data_report_env)
		fprintf(stderr,
						"mcc: const-data:   ^ all-zero .data off=%ld size=%ld (belongs in .bss; reclaimable=%ld)\n",
						off, size, ast_data_zerobytes);
}

/* Verify the winning chain actually decodes back to the exact source bytes. A datacomp
 * rewrite that trusted a lossy chain would silently miscompile, so a candidate is only
 * counted if compress→decompress is bit-exact. Doubles as a decoder bug-hunt: every const
 * object in every compiled TU exercises the chosen dec() path against a known original. */
static int ast_data_roundtrips(const unsigned char *bytes, long size, const ComboBest *best) {
	unsigned char *comp, *back;
	long clen, blen;
	clen = combo_pipe_apply(best->sel, best->k, bytes, size, ast_data_pipe_a, ast_data_pipe_b,
													sizeof ast_data_pipe_a, &comp);
	if (clen < 0 || clen > (long)sizeof ast_data_pipe_c)
		return 0;
	memcpy(ast_data_pipe_c, comp, (size_t)clen);
	blen = combo_pipe_unapply(best->sel, best->k, ast_data_pipe_c, clen, ast_data_pipe_a,
														ast_data_pipe_b, sizeof ast_data_pipe_a, &back);
	return blen == size && memcmp(back, bytes, (size_t)size) == 0;
}

static void ast_data_estimate(void *sec, long off, long size, int is_ro) {
	const unsigned char *bytes;
	ComboBest best;
	int i;
	if (size < AST_DATA_PACKMIN || size > AST_DATA_PACKMAX)
		return;
	bytes = ((Section *)sec)->data + off;
	if (!combo_pipeline_search(bytes, size, AST_DATA_PIPE_DEPTH, ast_data_pipe_a,
														 ast_data_pipe_b, sizeof ast_data_pipe_a, &best))
		return;
	MCC_TRACE("data pack off=%ld size=%ld k=%d packed=%ld evaluated=%ld (%s)\n", off, size,
						best.k, best.score, best.evaluated, is_ro ? "rodata" : "data");
	if (best.score <= 0 || best.score * 2 >= size)
		return;
	if (!ast_data_roundtrips(bytes, size, &best)) {
		ast_data_nlossy++;
		MCC_TRACE("data pack off=%ld size=%ld LOSSY chain (rejected, codec bug?)\n", off, size);
		if (ast_data_report_env)
			fprintf(stderr, "mcc: const-data:   ^ chain did NOT round-trip; rejected (off=%ld)\n", off);
		return;
	}
	ast_data_ncompressible++;
	ast_data_saved += size - best.score;
	if (ast_data_report_env) {
		fprintf(stderr, "mcc: const-data:   ^ compressible %ld->%ld chain=", size, best.score);
		for (i = 0; i < best.k; i++)
			fprintf(stderr, "%s%s", i ? "+" : "", combo_codecs[best.sel[i]].name);
		fprintf(stderr, " (M6 candidate, round-trip OK; est saved=%ld)\n", ast_data_saved);
	}
}

void ast_hook_data(void *sec, long off, long size, int is_ro) {
	if (size <= 0)
		return;
	if (is_ro)
		ast_data_total_ro += size;
	else
		ast_data_total_rw += size;
	if (ast_data_n < AST_DATA_CAP) {
		ast_data_recs[ast_data_n].sec = sec;
		ast_data_recs[ast_data_n].off = off;
		ast_data_recs[ast_data_n].size = size;
		ast_data_recs[ast_data_n].is_ro = is_ro;
		ast_data_n++;
	}
	MCC_TRACE("data emit sec=%p off=%ld size=%ld %s (ro=%ld rw=%ld)\n", sec, off, size,
						is_ro ? "rodata" : "data", ast_data_total_ro, ast_data_total_rw);
	if (ast_data_report_env)
		fprintf(stderr, "mcc: const-data: %-6s off=%-8ld size=%-6ld (running ro=%ld rw=%ld)\n",
						is_ro ? "rodata" : "data", off, size, ast_data_total_ro, ast_data_total_rw);
	ast_data_estimate(sec, off, size, is_ro);
	ast_data_zero_check(sec, off, size, is_ro);
}

#define AST_DU_CAP 2048
#define AST_DU_WRITTEN 1u
#define AST_DU_ESCAPED 2u
static const AstArena *ast_du_arena;
static uint64_t ast_du_epoch;
static int ast_du_state; /* 0 = must build, 1 = valid, -1 = overflowed */
static int ast_du_n;
static int ast_du_off[AST_DU_CAP];
static uint8_t ast_du_flags[AST_DU_CAP];

static uint8_t *ast_du_find(int off, int create) {
	for (int i = 0; i < ast_du_n; i++)
		if (ast_du_off[i] == off)
			return &ast_du_flags[i];
	if (!create || ast_du_n >= AST_DU_CAP)
		return NULL;
	ast_du_off[ast_du_n] = off;
	ast_du_flags[ast_du_n] = 0;
	return &ast_du_flags[ast_du_n++];
}

static void ast_du_build(const AstArena *a) {
	ast_du_n = 0;
	ast_du_state = 1;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		uint16_t k = ast_kind(a, n);
		AstLocal c;
		int r, off;
		uint8_t *f;
		if (k == AST_Store) {
			c = ast_first_child(a, n);
			if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
				continue;
			r = ast_op(a, c);
			if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
				continue;
			f = ast_du_find((int)(int64_t)ast_ival(a, c), 1);
			if (!f) {
				ast_du_state = -1;
				return;
			}
			*f |= AST_DU_WRITTEN;
		} else if (k == AST_Unary) {
			c = ast_first_child(a, n);
			if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
				continue;
			r = ast_op(a, c);
			off = (int)(int64_t)ast_ival(a, c);
			if ((r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM)) {
				f = ast_du_find(off, 1);
				if (!f) {
					ast_du_state = -1;
					return;
				}
				*f |= AST_DU_WRITTEN;
			}
			int op = ast_op(a, n);
			if ((op == AST_OP_ADDR || op == AST_OP_MEMBER ||
					 op == AST_OP_MEMBER_ARROW) &&
					(r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
				f = ast_du_find(off, 1);
				if (!f) {
					ast_du_state = -1;
					return;
				}
				*f |= AST_DU_ESCAPED;
			}
		}
	}
}

static void ast_du_sync(const AstArena *a) {
	if (ast_du_state && ast_du_arena == a && ast_du_epoch == a->epoch)
		return;
	ast_du_arena = a;
	ast_du_epoch = a->epoch;
	ast_du_build(a);
}

static void ast_du_invalidate(const AstArena *a) {
	if (ast_du_arena == a) {
		ast_du_arena = NULL;
		ast_du_state = 0;
	}
}

static unsigned ast_du_slot_flags(const AstArena *a, int off) {
	ast_du_sync(a);
	uint8_t *f = ast_du_find(off, 0);
	return f ? *f : 0u;
}

#if MCC_CONFIG_AST_SHADOW
static void ast_du_diverge(const char *q, int off, int tab, int scan) {
	fprintf(stderr,
					"mcc: AST side-car divergence: %s(off=%d) table=%d scan=%d\n", q,
					off, tab, scan);
	abort();
}
#endif

/*
 * Per-node property memos (rollout step 2). The four monotone subtree predicates
 * (ast_ident_pure, ast_cprop_safe, ast_sccp_has_label, ast_cse_regpure) are pure
 * functions of the subtree rooted at a node (its fields plus the stable volatile
 * bits of any referenced Sym), so each node's verdict is cached in a per-predicate
 * byte array (0 unknown, 1 true, 2 false) and reused in O(1). The whole memo is
 * cleared whenever a->epoch changes (any mutation) and grown to a->count on
 * demand. Shadow build asserts each memoized answer equals a fresh recompute.
 */
enum {
	AST_MEMO_PURE,
	AST_MEMO_CPROPSAFE,
	AST_MEMO_HASLABEL,
	AST_MEMO_REGPURE,
	AST_MEMO_PRED_COUNT
};
static const AstArena *ast_memo_arena;
static uint64_t ast_memo_epoch;
static int ast_memo_cap;
static int8_t *ast_memo[AST_MEMO_PRED_COUNT];

static void ast_memo_sync(const AstArena *a) {
	int cnt = (int)ast_count(a);
	if (ast_memo_arena == a && ast_memo_epoch == a->epoch && ast_memo_cap >= cnt)
		return;
	if (ast_memo_cap < cnt) {
		int ncap = cnt ? cnt : 1;
		for (int i = 0; i < AST_MEMO_PRED_COUNT; i++)
			ast_memo[i] =
					mcc_realloc(ast_memo[i], (size_t)ncap * sizeof *ast_memo[i]);
		ast_memo_cap = ncap;
	}
	for (int i = 0; i < AST_MEMO_PRED_COUNT; i++)
		if (ast_memo[i])
			memset(ast_memo[i], 0, (size_t)ast_memo_cap * sizeof *ast_memo[i]);
	ast_memo_arena = a;
	ast_memo_epoch = a->epoch;
}

static void ast_memo_invalidate(const AstArena *a) {
	if (ast_memo_arena == a) {
		ast_memo_arena = NULL;
		ast_memo_epoch = 0;
	}
}

#if MCC_CONFIG_AST_SHADOW
static void ast_memo_diverge(const char *q, AstLocal n, int memo, int scan) {
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
		if (m && n < (AstLocal)ast_memo_cap && m[n]) {            \
			int r = m[n] == 1;                                      \
			AST_MEMO_SHADOW(NAME, n, r);                            \
			return r;                                               \
		}                                                         \
		int r = ast_##NAME##_compute(a, n);                       \
		if (m && n < (AstLocal)ast_memo_cap)                      \
			m[n] = r ? 1 : 2;                                       \
		return r;                                                 \
	}

AST_MEMO_QUERY(ident_pure, AST_MEMO_PURE)
AST_MEMO_QUERY(cprop_safe, AST_MEMO_CPROPSAFE)
AST_MEMO_QUERY(sccp_has_label, AST_MEMO_HASLABEL)
AST_MEMO_QUERY(cse_regpure, AST_MEMO_REGPURE)

/*
 * Structural subtree hash (rollout step 3). h[n] folds the exact ast_ident_same
 * tuple (kind, op, type_t, type_ref, ival, fbits, sym, nchild) with the ordered
 * child hashes, so structurally-equal subtrees always hash equal. Used as a
 * collision-proof fast reject: h[x] != h[y] proves the subtrees differ in O(1);
 * on a hash match ast_ident_same falls through to the full ast_ident_same_scan
 * (confirm-on-fire), so a collision can never make it report a false equality.
 * The side-car is filled lazily per node and cleared when a->epoch changes.
 */
static const AstArena *ast_hash_arena;
static uint64_t ast_hash_epoch;
static int ast_hash_cap;
static uint64_t *ast_hash;
static uint8_t *ast_hash_done;

static void ast_hash_sync(const AstArena *a) {
	int cnt = (int)ast_count(a);
	if (ast_hash_arena == a && ast_hash_epoch == a->epoch && ast_hash_cap >= cnt)
		return;
	if (ast_hash_cap < cnt) {
		int ncap = cnt ? cnt : 1;
		ast_hash = mcc_realloc(ast_hash, (size_t)ncap * sizeof *ast_hash);
		ast_hash_done = mcc_realloc(ast_hash_done, (size_t)ncap * sizeof *ast_hash_done);
		ast_hash_cap = ncap;
	}
	if (ast_hash_done)
		memset(ast_hash_done, 0, (size_t)ast_hash_cap);
	ast_hash_arena = a;
	ast_hash_epoch = a->epoch;
}

static void ast_hash_invalidate(const AstArena *a) {
	if (ast_hash_arena == a) {
		ast_hash_arena = NULL;
		ast_hash_epoch = 0;
	}
}

static uint64_t ast_hash_mix(uint64_t h, uint64_t v) {
	h ^= v;
	h *= 0x100000001b3ull;
	return h;
}

static uint64_t ast_hash_of(const AstArena *a, AstLocal n) {
	if (n >= (AstLocal)ast_hash_cap)
		return 0;
	if (ast_hash_done[n])
		return ast_hash[n];
	uint64_t v = 0xcbf29ce484222325ull;
	v = ast_hash_mix(v, a->kind[n]);
	v = ast_hash_mix(v, (uint64_t)(uint32_t)a->op[n]);
	v = ast_hash_mix(v, (uint64_t)(uint32_t)a->type_t[n]);
	v = ast_hash_mix(v, a->type_ref[n]);
	v = ast_hash_mix(v, a->ival[n]);
	v = ast_hash_mix(v, a->fbits[n]);
	v = ast_hash_mix(v, a->sym[n]);
	v = ast_hash_mix(v, a->nchild[n]);
	for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c])
		v = ast_hash_mix(v, ast_hash_of(a, c));
	ast_hash[n] = v;
	ast_hash_done[n] = 1;
	return v;
}

static int ast_local_is_readonly(AstArena *a, int off) {
	int r;
	ast_du_sync(a);
	if (ast_du_state < 0)
		r = ast_local_is_readonly_scan(a, off);
	else
		r = (ast_du_slot_flags(a, off) & AST_DU_WRITTEN) ? 0 : 1;
#if MCC_CONFIG_AST_SHADOW
	int s = ast_local_is_readonly_scan(a, off);
	if (r != s)
		ast_du_diverge("readonly", off, r, s);
#endif
	return r;
}
#endif

#if MCC_CONFIG_OPTIMIZER && defined(MCC_TARGET_X86_64)
static int ast_promo_reg_of(AstArena *a, AstLocal n) {
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		return -1;
	int r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
		return -1;
	int off = (int)(int64_t)ast_ival(a, n);
	for (int i = 0; i < ast_promo_n; i++)
		if (ast_promo_off[i] == off)
			return ast_promo_reg[i];
	return -1;
}

static int ast_subtree_refs_local(AstArena *a, AstLocal n, int off) {
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Ref) {
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && (r & VT_LVAL) && !(r & VT_SYM) &&
				(int)(int64_t)ast_ival(a, n) == off)
			return 1;
	}
	uint32_t nc = ast_nchild(a, n);
	for (uint32_t i = 0; i < nc; i++)
		if (ast_subtree_refs_local(a, ast_child(a, n, i), off))
			return 1;
	return 0;
}

static void ast_promo_weigh(AstArena *a, AstLocal n, int depth, const int *coff, int nc,
														int *cweight) {
	if (n == AST_NONE)
		return;
	uint16_t k = ast_kind(a, n);
	if (k == AST_Ref) {
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int off = (int)(int64_t)ast_ival(a, n);
			for (int j = 0; j < nc; j++)
				if (coff[j] == off) {
					cweight[j] += 1 << (depth < 12 ? depth : 12);
					break;
				}
		}
	}
	int cd = (k == AST_If && ast_op(a, n) == 2) ? depth + 1 : depth;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_promo_weigh(a, c, cd, coff, nc, cweight);
}

static void ast_subtree_span(AstArena *a, AstLocal n, int *lo, int *hi) {
	if (n == AST_NONE)
		return;
	if ((int)n < *lo)
		*lo = (int)n;
	if ((int)n > *hi)
		*hi = (int)n;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_subtree_span(a, c, lo, hi);
}

static int ast_plan_promotion(AstArena *a) {
	ast_promo_n = 0;
	ast_promo_callful = 0;
	if (!ast_promote_env || ast_func_has_asm)
		return 0;
	AstLocal nn = ast_count(a);
	int has_call = 0, has_vla = 0, has_goto = 0, has_loop = 0;
	for (AstLocal n = 0; n < nn; n++) {
		uint16_t k = ast_kind(a, n);
		if (k == AST_Invoke)
			has_call = 1;
		else if (k == AST_Unary && ast_op(a, n) == AST_OP_VLA)
			has_vla = 1;
		else if (k == AST_Jump && (ast_op(a, n) == 4 || ast_op(a, n) == 5))
			has_goto = 1;
		else if (k == AST_BasicBlock) {
			for (AstLocal s = ast_first_child(a, n); s != AST_NONE;
					 s = ast_next_sib(a, s)) {
				int so = ast_op(a, s);
				if (ast_kind(a, s) == AST_If &&
						(so == 2 || so == 3 || so == 4 || so == 5))
					has_loop = 1;
			}
		}
	}
	if (has_vla)
		return 0;
	if (has_call && (ast_no_callful_env || ast_no_callful_promo))
		return 0;
	int coff[AST_PROMO_MAX * 8], ctyp[AST_PROMO_MAX * 8], cpoison[AST_PROMO_MAX * 8];
	int cweight[AST_PROMO_MAX * 8];
	int nc = 0;
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Ref)
			continue;
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
			continue;
		int off = (int)(int64_t)ast_ival(a, n);
		int tt = ast_type_t(a, n);
		int bt = tt & VT_BTYPE;
		int scalar = (bt == VT_INT || bt == VT_LLONG || bt == VT_PTR ||
									bt == VT_FLOAT || bt == VT_DOUBLE) &&
								 !(tt & (VT_ARRAY | VT_BITFIELD | VT_VOLATILE));
		int j;
		for (j = 0; j < nc; j++)
			if (coff[j] == off)
				break;
		if (j == nc) {
			if (nc >= (int)(sizeof coff / sizeof *coff))
				continue;
			coff[nc] = off, ctyp[nc] = ast_type_t(a, n), cpoison[nc] = 0, nc++;
		}
		if (!scalar)
			cpoison[j] = 1;
		else if (!(ctyp[j] & VT_BTYPE))
			ctyp[j] = ast_type_t(a, n);
	}
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Unary)
			continue;
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
			continue;
		int r = ast_op(a, c);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			continue;
		int off = (int)(int64_t)ast_ival(a, c);
		int sz = 0;
		if (ast_op(a, n) == AST_OP_ADDR) {
			CType ct;
			ct.t = ast_type_t(a, n);
			ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			if ((ct.t & VT_BTYPE) == VT_PTR && ct.ref) {
				int al;
				sz = type_size(&ct.ref->type, &al);
			}
		}
		for (int j = 0; j < nc; j++)
			if (coff[j] == off || (sz > 0 && coff[j] >= off && coff[j] < off + sz))
				cpoison[j] = 1;
	}
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Ref)
			continue;
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM) || (r & VT_LVAL))
			continue;
		int off = (int)(int64_t)ast_ival(a, n);
		CType ct;
		ct.t = ast_type_t(a, n);
		ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		int al, sz = 8;
		if ((ct.t & VT_BTYPE) == VT_PTR && ct.ref)
			sz = type_size(&ct.ref->type, &al);
		else if ((ct.t & VT_BTYPE) == VT_STRUCT || (ct.t & VT_ARRAY))
			sz = type_size(&ct, &al);
		if (sz <= 0)
			sz = 8;
		for (int j = 0; j < nc; j++)
			if (coff[j] >= off && coff[j] < off + sz)
				cpoison[j] = 1;
	}
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Ref)
			continue;
		int r = ast_op(a, n), t = ast_type_t(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			continue;
		if (!(t & VT_ARRAY) && (t & VT_BTYPE) != VT_STRUCT)
			continue;
		CType ct;
		ct.t = t;
		ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		int al, size = type_size(&ct, &al);
		if (size <= 0)
			size = 8;
		int base = (int)(int64_t)ast_ival(a, n);
		for (int j = 0; j < nc; j++)
			if (coff[j] >= base && coff[j] < base + size)
				cpoison[j] = 1;
	}
	for (int j = 0; j < nc; j++)
		for (int t = 0; t < ast_ltemp_n; t++)
			if (coff[j] == ast_ltemp_off[t]) {
				cpoison[j] = 1;
				break;
			}
	for (int j = 0; j < nc; j++)
		cweight[j] = 0;
	ast_promo_weigh(a, ast_root(a), 0, coff, nc, cweight);
	ast_promo_callful = has_call;
	const int *gp_pool = has_call ? ast_promo_callee : ast_promo_caller;
	int gp_max = has_call ? (int)(sizeof ast_promo_callee / sizeof *ast_promo_callee)
												: (int)(sizeof ast_promo_caller / sizeof *ast_promo_caller);
	int xmm_max = has_call ? 0 : (int)(sizeof ast_promo_xmm / sizeof *ast_promo_xmm);
	int gp_n = 0, xmm_n = 0;
	if (ast_color_env && !has_goto) {
		int cfirst[AST_PROMO_MAX * 8], clast[AST_PROMO_MAX * 8];
		int cdeffirst[AST_PROMO_MAX * 8];
		for (int j = 0; j < nc; j++)
			cfirst[j] = -1, clast[j] = -1, cdeffirst[j] = 0;
		for (AstLocal n = 0; n < nn; n++) {
			if (ast_kind(a, n) != AST_Ref)
				continue;
			int r = ast_op(a, n);
			if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
				continue;
			int off = (int)(int64_t)ast_ival(a, n);
			for (int j = 0; j < nc; j++)
				if (coff[j] == off) {
					if (cfirst[j] < 0)
						cfirst[j] = (int)n;
					clast[j] = (int)n;
					break;
				}
		}
		for (AstLocal n = 0; n < nn; n++) {
			if (ast_kind(a, n) != AST_Store)
				continue;
			AstLocal tgt = ast_child(a, n, 0);
			if (tgt == AST_NONE || ast_kind(a, tgt) != AST_Ref)
				continue;
			int off = (int)(int64_t)ast_ival(a, tgt);
			for (int j = 0; j < nc; j++)
				if (coff[j] == off && cfirst[j] == (int)tgt)
					cdeffirst[j] = 1;
		}
		int lo[AST_PROMO_MAX * 8], hi[AST_PROMO_MAX * 8];
		for (int j = 0; j < nc; j++) {
			lo[j] = cdeffirst[j] ? cfirst[j] : 0;
			hi[j] = clast[j];
		}
		if (has_loop) {
			for (AstLocal n = 0; n < nn; n++) {
				if (ast_kind(a, n) != AST_BasicBlock)
					continue;
				for (AstLocal s = ast_first_child(a, n); s != AST_NONE;
						 s = ast_next_sib(a, s)) {
					int so = ast_op(a, s);
					if (ast_kind(a, s) != AST_If ||
							!(so == 2 || so == 3 || so == 4 || so == 5))
						continue;
					int llo = (int)s, lhi = (int)s;
					ast_subtree_span(a, s, &llo, &lhi);
					for (int j = 0; j < nc; j++) {
						if (cfirst[j] < 0)
							continue;
						if (clast[j] < llo || cfirst[j] > lhi)
							continue;
						if (llo < lo[j])
							lo[j] = llo;
						if (lhi > hi[j])
							hi[j] = lhi;
					}
				}
			}
		}
		int careg[AST_PROMO_MAX * 8];
		for (int j = 0; j < nc; j++)
			careg[j] = -1;
		for (int cls = 0; cls < 2; cls++) {
			const int *pool = cls == 0 ? gp_pool : ast_promo_xmm;
			int kcol = cls == 0 ? gp_max : xmm_max;
			int idx[AST_COLOR_MAX], m = 0;
			for (int j = 0; j < nc && m < AST_COLOR_MAX; j++) {
				if (cpoison[j] || coff[j] >= 0 || cfirst[j] < 0)
					continue;
				if ((cls == 0) == (is_float(ctyp[j]) != 0))
					continue;
				idx[m++] = j;
			}
			uint64_t adj[AST_COLOR_MAX];
			int cost[AST_COLOR_MAX], col[AST_COLOR_MAX];
			for (int i = 0; i < m; i++) {
				adj[i] = 0;
				cost[i] = cweight[idx[i]];
			}
			for (int i = 0; i < m; i++)
				for (int j = i + 1; j < m; j++) {
					int a1 = idx[i], b1 = idx[j];
					int disjoint = (hi[a1] < lo[b1]) || (hi[b1] < lo[a1]);
					if (!disjoint) {
						adj[i] |= (uint64_t)1 << j;
						adj[j] |= (uint64_t)1 << i;
					}
				}
			ast_color_graph(m, adj, cost, kcol, col);
			for (int i = 0; i < m; i++)
				if (col[i] >= 0)
					careg[idx[i]] = pool[col[i]];
		}
		int ord[AST_PROMO_MAX * 8], no = 0;
		for (int j = 0; j < nc; j++)
			if (careg[j] >= 0)
				ord[no++] = j;
		for (int i = 0; i < no; i++)
			for (int j = i + 1; j < no; j++)
				if (lo[ord[j]] < lo[ord[i]]) {
					int t = ord[i];
					ord[i] = ord[j];
					ord[j] = t;
				}
		for (int i = 0; i < no; i++) {
			ast_promo_off[ast_promo_n] = coff[ord[i]];
			ast_promo_typ[ast_promo_n] = ctyp[ord[i]];
			ast_promo_reg[ast_promo_n] = careg[ord[i]];
			ast_promo_n++;
		}
		return ast_promo_n;
	}
	int colorable[AST_PROMO_MAX * 8];
	for (int j = 0; j < nc; j++)
		colorable[j] = 1;
	if (ast_color_env) {
		for (int cls = 0; cls < 2; cls++) {
			int idx[AST_COLOR_MAX], m = 0;
			int kcol = cls == 0 ? gp_max : xmm_max;
			for (int j = 0; j < nc && m < AST_COLOR_MAX; j++) {
				if (cpoison[j] || coff[j] >= 0)
					continue;
				if ((cls == 0) == (is_float(ctyp[j]) != 0))
					continue;
				idx[m++] = j;
			}
			uint64_t adj[AST_COLOR_MAX];
			int cost[AST_COLOR_MAX], col[AST_COLOR_MAX];
			uint64_t full = (m < 64) ? ((uint64_t)1 << m) - 1 : ~(uint64_t)0;
			for (int i = 0; i < m; i++) {
				adj[i] = full & ~((uint64_t)1 << i);
				cost[i] = cweight[idx[i]];
			}
			ast_color_graph(m, adj, cost, kcol, col);
			for (int i = 0; i < m; i++)
				if (col[i] < 0)
					colorable[idx[i]] = 0;
		}
	}
	for (;;) {
		int best = -1;
		for (int j = 0; j < nc; j++) {
			if (cpoison[j] || coff[j] >= 0 || !colorable[j])
				continue;
			if (is_float(ctyp[j]) ? (xmm_n >= xmm_max) : (gp_n >= gp_max))
				continue;
			if (best < 0 || cweight[j] > cweight[best])
				best = j;
		}
		if (best < 0)
			break;
		ast_promo_off[ast_promo_n] = coff[best];
		ast_promo_typ[ast_promo_n] = ctyp[best];
		ast_promo_reg[ast_promo_n] =
				is_float(ctyp[best]) ? ast_promo_xmm[xmm_n++] : gp_pool[gp_n++];
		ast_promo_n++;
		cpoison[best] = 1;
	}
	return ast_promo_n;
}

static int ast_promo_save_loc;

static void ast_promo_save_sv(SValue *sv, int i) {
	memset(sv, 0, sizeof *sv);
	sv->type.t = VT_LLONG;
	sv->r = VT_LOCAL | VT_LVAL;
	sv->r2 = VT_CONST;
	sv->c.i = ast_promo_save_loc + 8 * i;
}

static void ast_promo_write(int reg, CType *ct) {
	gen_cast(ct);
	if (reg_classes[reg]) {
		ast_pinned_regs &= ~(1u << reg);
		gv(reg_classes[reg]);
		ast_pinned_regs |= (1u << reg);
	} else {
		gv(MCC_RC_INT);
		load(reg, vtop);
		vtop->r = reg;
		vtop->r2 = VT_CONST;
		vtop->c.i = 0;
		vtop->sym = NULL;
	}
}

static void ast_promo_entry_init(void) {
	if (ast_promo_callful) {
		SValue sv;
		loc = (loc - 8 * ast_promo_n) & -8;
		ast_promo_save_loc = loc;
		for (int i = 0; i < ast_promo_n; i++) {
			ast_promo_save_sv(&sv, i);
			store(ast_promo_reg[i], &sv);
		}
	}
	for (int i = 0; i < ast_promo_n; i++) {
		int reg = ast_promo_reg[i];
		int shared = 0;
		for (int p = 0; p < i; p++)
			if (ast_promo_reg[p] == reg) {
				shared = 1;
				break;
			}
		if (shared)
			continue;
		SValue sv;
		memset(&sv, 0, sizeof sv);
		sv.type.t = ast_promo_typ[i];
		sv.r = VT_LOCAL | VT_LVAL;
		sv.r2 = VT_CONST;
		sv.c.i = ast_promo_off[i];
		vpushv(&sv);
		ast_pinned_regs &= ~(1u << reg);
		if (reg_classes[reg])
			gv(reg_classes[reg]);
		else
			load(reg, vtop);
		ast_pinned_regs |= (1u << reg);
		vpop();
	}
}

static void ast_promo_exit_restore(void) {
	SValue sv;
	if (!ast_promo_callful)
		return;
	for (int i = ast_promo_n - 1; i >= 0; i--) {
		ast_promo_save_sv(&sv, i);
		load(ast_promo_reg[i], &sv);
	}
}
#else
static int ast_plan_promotion(AstArena *a) {
	(void)a;
	return 0;
}
static int ast_promo_reg_of(AstArena *a, AstLocal n) {
	(void)a;
	(void)n;
	return -1;
}
static void ast_promo_entry_init(void) {
}
static void ast_promo_exit_restore(void) {
}
static int ast_promo_n;
static int ast_promo_callful;
static int ast_promo_regpool_at(int i) {
	(void)i;
	return 0;
}
#endif

static void ast_error_sink(void *opaque, const char *msg) {
	(void)opaque;
	(void)msg;
}

static void ast_replay_value(AstArena *a, AstLocal n) {
	switch (ast_kind(a, n)) {
	case AST_Literal:
	case AST_Ref: {
		SValue sv;
		memset(&sv, 0, sizeof sv);
		sv.type.t = ast_type_t(a, n);
		sv.type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		sv.r = (unsigned short)ast_op(a, n);
		sv.r2 = VT_CONST;
		sv.c.i = ast_ival(a, n);
		sv.sym = (Sym *)(uintptr_t)ast_sym(a, n);
		if ((sv.r & VT_VALMASK) == VT_LOCAL && !(sv.r & VT_SYM) &&
				(ast_inline_bias || ast_argsub_n)) {
			int off = (int)sv.c.i, subst = 0;
			for (int j = 0; j < ast_argsub_n; j++)
				if (ast_argsub_off[j] == off) {
					sv = ast_argsub_val[j];
					subst = 1;
					break;
				}
			if (!subst)
				sv.c.i += ast_inline_bias;
		}
		if (ast_promo_n && !ast_in_graft) {
			int preg = ast_promo_reg_of(a, n);
			if (preg >= 0) {
				sv.r = (unsigned short)preg;
				sv.c.i = 0;
				sv.sym = NULL;
			}
		}
		vpushv(&sv);
		break;
	}
	case AST_Binary: {
		int bop = ast_op(a, n);
		if (bop == TOK_LAND || bop == TOK_LOR) {
			int i = bop == TOK_LAND, t = 0;
			uint32_t nc = ast_nchild(a, n), k;
			for (k = 0; k < nc; k++) {
				ast_replay_value(a, ast_child(a, n, k));
				save_regs(1);
				if (k + 1 < nc)
					t = gvtst(i, t);
			}
			gvtst_set(i, t);
			break;
		}
		ast_replay_value(a, ast_child(a, n, 0));
		ast_replay_value(a, ast_child(a, n, 1));
		gen_op(bop);
		break;
	}
	case AST_Convert: {
		ast_replay_value(a, ast_child(a, n, 0));
		CType ct;
		ct.t = ast_type_t(a, n);
		ct.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		gen_cast(&ct);
		break;
	}
	case AST_Unary: {
		int uop = ast_op(a, n);
		ast_replay_value(a, ast_child(a, n, 0));
		if (uop == AST_OP_ADDR) {
			gaddrof();
			vtop->type.t = ast_type_t(a, n);
			vtop->type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		} else if (uop == AST_OP_MEMBER || uop == AST_OP_MEMBER_ARROW) {
			if (uop == AST_OP_MEMBER_ARROW)
				indir();
			gaddrof();
			vtop->type = char_pointer_type;
			vpushi((int)ast_ival(a, n));
			gen_op('+');
			CType mt;
			mt.t = ast_type_t(a, n);
			mt.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			vtop->type = mt;
			if (!(mt.t & VT_ARRAY))
				vtop->r |= VT_LVAL | (int)ast_fbits(a, n);
		} else if (uop == AST_OP_IMAG) {
			gen_imaginary_complex((int)ast_ival(a, n));
		} else {
			inc((int)ast_ival(a, n), uop);
		}
		break;
	}
	case AST_Load:
		ast_replay_value(a, ast_child(a, n, 0));
		indir();
		break;
	case AST_If: {
		SValue sv;
		CType type;
		int tt, u, rc, r1, r2;
		ast_replay_value(a, ast_child(a, n, 0));
		save_regs(1);
		tt = gvtst(1, 0);
		ast_replay_value(a, ast_child(a, n, 1));
		sv = *vtop;
		vtop--;
		u = gjmp(0);
		gsym(tt);
		ast_replay_value(a, ast_child(a, n, 2));
		combine_types(&type, &sv, vtop, '?');
		gen_cast(&type);
		rc = MCC_RC_TYPE(type.t);
		if (USING_TWO_WORDS(type.t))
			rc = MCC_RC_RET(type.t);
		r2 = gv(rc);
		tt = gjmp(0);
		gsym(u);
		*vtop = sv;
		gen_cast(&type);
		r1 = gv(rc);
		move_reg(r2, r1, type.t);
		vtop->r = r2;
		gsym(tt);
		break;
	}
	case AST_Invoke: {
		if (ast_inline_graft(a, n))
			break;
		uint32_t nc = ast_nchild(a, n);
		for (uint32_t i = 0; i < nc; i++)
			ast_replay_value(a, ast_child(a, n, i));
		vcheck_cmp();
		gfunc_call((int)nc - 1);
		if (ast_type_t(a, n) == VT_VOID)
			break;
		if ((ast_type_t(a, n) & VT_BTYPE) == VT_STRUCT) {
			CType rt;
			SValue ret;
			int ret_nregs, regsize, ret_align, r, nn, size, align, addr, offset;
			rt.t = ast_type_t(a, n);
			rt.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
			{
				CType rtmp;
				int rax, rsx, rnx;
				rnx = gfunc_sret(&rt, 0, &rtmp, &rax, &rsx);
				if (rnx <= 0) {
					int sal, ssz = type_size(&rt, &sal);
#ifdef MCC_TARGET_ARM64
					if (ssz < 16)
						while (ssz & (ssz - 1))
							ssz = (ssz | (ssz - 1)) + 1;
#endif
					ast_alloc_loc(ssz, sal);
					SValue sv;
					memset(&sv, 0, sizeof sv);
					sv.type.t = ast_type_t(a, n);
					sv.type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
					sv.r = (unsigned short)ast_op(a, n);
					sv.r2 = VT_CONST;
					sv.c.i = ast_ival(a, n);
					sv.sym = (Sym *)(uintptr_t)ast_sym(a, n);
					vpushv(&sv);
#if defined(MCC_TARGET_RISCV64) || (defined(MCC_TARGET_X86_64) && !defined(MCC_TARGET_PE))
					if (rnx < 0)
						arch_transfer_ret_regs(1);
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
			while (nn > 1) {
				int rc = reg_classes[ret.r] & ~(MCC_RC_INT | MCC_RC_FLOAT);
				rc <<= --nn;
				for (r = 0; r < MCC_NB_REGS; ++r)
					if (reg_classes[r] & rc)
						break;
				vsetc(&ret.type, r, &ret.c);
			}
			vsetc(&ret.type, ret.r, &ret.c);
			vtop->r2 = ret.r2;
			size = type_size(&rt, &align);
			size = (size + regsize - 1) & -regsize;
			if (ret_align > align)
				align = ret_align;
			loc = ast_alloc_loc(size, align);
			addr = loc;
			offset = 0;
			for (;;) {
				vset(&ret.type, VT_LOCAL | VT_LVAL, addr + offset);
				vswap();
				vstore();
				vtop--;
				if (--ret_nregs == 0)
					break;
				offset += regsize;
			}
			vset(&rt, VT_LOCAL | VT_LVAL, addr);
			vtop->r |= VT_NONLVAL;
			break;
		}
		SValue sv;
		memset(&sv, 0, sizeof sv);
		sv.type.t = ast_type_t(a, n);
		sv.type.ref = (Sym *)(uintptr_t)ast_type_ref(a, n);
		sv.r = (unsigned short)ast_op(a, n);
		sv.r2 = VT_CONST;
		sv.c.i = ast_ival(a, n);
		sv.sym = (Sym *)(uintptr_t)ast_sym(a, n);
		vpushv(&sv);
		break;
	}
	default:
		break;
	}
}

static struct ast_rp_label *ast_rp_label_get(int v) {
	for (int i = ast_rp_label_floor; i < ast_rp_nlabel; i++)
		if (ast_rp_labels[i].v == v)
			return &ast_rp_labels[i];
	if (ast_rp_nlabel == ast_rp_caplabel) {
		ast_rp_caplabel = ast_rp_caplabel ? ast_rp_caplabel * 2 : 8;
		ast_rp_labels =
				mcc_realloc(ast_rp_labels, ast_rp_caplabel * sizeof *ast_rp_labels);
	}
	struct ast_rp_label *l = &ast_rp_labels[ast_rp_nlabel++];
	l->v = v;
	l->jind = l->jnext = l->defined = 0;
	return l;
}

static void ast_replay_bb(AstArena *a, AstLocal bb) {
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE;
			 s = ast_next_sib(a, s)) {
		switch (ast_kind(a, s)) {
		case AST_Store: {
#if MCC_CONFIG_OPTIMIZER && defined(MCC_TARGET_X86_64)
			int preg = (ast_promo_n && !ast_in_graft)
										 ? ast_promo_reg_of(a, ast_child(a, s, 0))
										 : -1;
			if (preg >= 0) {
				ast_replay_value(a, ast_child(a, s, 1));
				CType tct;
				tct.t = ast_type_t(a, ast_child(a, s, 0)) & ~(VT_ARRAY | VT_VLA);
				tct.ref = (Sym *)(uintptr_t)ast_type_ref(a, ast_child(a, s, 0));
				ast_promo_write(preg, &tct);
				vpop();
				break;
			}
#endif
			ast_replay_value(a, ast_child(a, s, 0));
			ast_replay_value(a, ast_child(a, s, 1));
			vstore();
			vpop();
			break;
		}
		case AST_BasicBlock:
			ast_replay_bb(a, s);
			break;
		case AST_Invoke:
			ast_replay_value(a, s);
			if (ast_type_t(a, s) != VT_VOID)
				vpop();
			break;
		case AST_Unary:
			if (ast_op(a, s) == AST_OP_VLA) {
				CType vt;
				vt.t = ast_type_t(a, s);
				vt.ref = (Sym *)(uintptr_t)ast_type_ref(a, s);
				int addr = (int)(int64_t)ast_ival(a, s);
				int locorig = (int)(int64_t)ast_sym(a, s);
				int al;
				if (ast_fbits(a, s))
					gen_vla_sp_save(locorig);
				vpush_type_size(&vt, &al);
				gen_vla_alloc(&vt, al);
				gen_vla_sp_save(addr);
				break;
			}
			if (ast_op(a, s) == AST_OP_VLA_RESTORE) {
				gen_vla_sp_restore((int)(int64_t)ast_ival(a, s));
				break;
			}
			ast_replay_value(a, s);
			vpop();
			break;
		case AST_Jump:
			if (ast_op(a, s) == 4) {
				struct ast_rp_label *l = ast_rp_label_get((int)ast_ival(a, s));
				gsym(l->jnext);
				l->jnext = 0;
				l->jind = gind();
				l->defined = 1;
			} else if (ast_op(a, s) == 5) {
				struct ast_rp_label *l = ast_rp_label_get((int)ast_ival(a, s));
				if (l->defined)
					gjmp_addr(l->jind);
				else
					l->jnext = gjmp(l->jnext);
			} else if (ast_op(a, s) == 2) {
				if (ast_rp_switch) {
					struct case_t *cr = mcc_malloc(sizeof(struct case_t));
					cr->v1 = (int64_t)ast_ival(a, s);
					cr->v2 = (int64_t)ast_fbits(a, s);
					cr->ind = gind();
					cr->line = 0;
					dynarray_add(&ast_rp_switch->p, &ast_rp_switch->n, cr);
				}
			} else if (ast_op(a, s) == 3) {
				if (ast_rp_switch)
					ast_rp_switch->def_sym = gind();
			} else if (ast_op(a, s) == 1) {
				if (ast_rp_csym)
					*ast_rp_csym = gjmp(*ast_rp_csym);
			} else {
				if (ast_rp_bsym)
					*ast_rp_bsym = gjmp(*ast_rp_bsym);
			}
			break;
		case AST_If: {
			if (ast_op(a, s) == 6) {
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
					gsym_addr(d, sw->def_sym);
				else
					gsym(d);
				gsym(a2);
				cur_switch = sw->prev;
				dynarray_reset(&sw->p, &sw->n);
				mcc_free(sw);
				break;
			}
			if (ast_op(a, s) == 2) {
				int dd = gind();
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
			if (ast_op(a, s) == 4) {
				int dd = gind();
				int aa = 0, bb = 0;
				int *sb = ast_rp_bsym, *sc = ast_rp_csym;
				ast_rp_bsym = &aa;
				ast_rp_csym = &bb;
				ast_replay_bb(a, ast_child(a, s, 0));
				ast_rp_bsym = sb;
				ast_rp_csym = sc;
				gsym(bb);
				ast_replay_value(a, ast_child(a, s, 1));
				int cc = gvtst(0, 0);
				gsym_addr(cc, dd);
				gsym(aa);
				break;
			}
			if (ast_op(a, s) == 5) {
				int cc = gind();
				int dd = cc;
				AstLocal incrbb = ast_child(a, s, 0);
				if (incrbb != AST_NONE && ast_first_child(a, incrbb) != AST_NONE) {
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
			if (ast_op(a, s) == 3) {
				int cc = gind();
				ast_replay_value(a, ast_child(a, s, 0));
				int aa = gvtst(1, 0);
				int dd = cc;
				AstLocal incrbb = ast_child(a, s, 1);
				if (incrbb != AST_NONE && ast_first_child(a, incrbb) != AST_NONE) {
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
					(vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
				int truthy = vtop->c.i != 0;
				vtop--;
				AstLocal taken = truthy ? ast_child(a, s, 1) : ast_child(a, s, 2);
				if (taken != AST_NONE)
					ast_replay_bb(a, taken);
				break;
			}
			int aa = gvtst(1, 0);
			ast_replay_bb(a, ast_child(a, s, 1));
			AstLocal elsebb = ast_child(a, s, 2);
			if (elsebb != AST_NONE) {
				int dd = gjmp(0);
				gsym(aa);
				ast_replay_bb(a, elsebb);
				gsym(dd);
			} else {
				gsym(aa);
			}
			break;
		}
		case AST_Return: {
			AstLocal v = ast_first_child(a, s);
			if (ast_in_graft) {
				if (v != AST_NONE) {
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
					ast_inline_ret_sym = gjmp(ast_inline_ret_sym);
				break;
			}
			if (v != AST_NONE) {
				ast_replay_value(a, v);
				gen_assign_cast(&func_vt);
				int rloc = (int)(int64_t)ast_ival(a, s);
				if (rloc)
					gen_vla_sp_restore(rloc);
				gfunc_return(&func_vt);
			}
			if (ast_op(a, s) == 1)
				rsym = gjmp(rsym);
			break;
		}
		default:
			break;
		}
	}
}

static void ast_replay_body(AstArena *a) {
	ast_replay_bb(a, ast_root(a));
}

static uint64_t ast_fold_eval(int op, int tt, uint64_t l1, uint64_t l2,
															int *ok) {
	int shm = ((tt & VT_BTYPE) == VT_LLONG) ? 63 : 31;
	switch (op) {
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
		if (l2 == 0) {
			*ok = 0;
			return 0;
		}
		return gen_opic_sdiv(l1, l2);
	case '%':
		if (l2 == 0) {
			*ok = 0;
			return 0;
		}
		return l1 - l2 * gen_opic_sdiv(l1, l2);
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

static void ast_fold_rec(AstArena *a, AstLocal n) {
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_fold_rec(a, c);
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		return;
	int op = ast_op(a, n);
	switch (op) {
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
		break;
	default:
		return;
	}
	AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
	if (ast_kind(a, x) != AST_Literal || ast_kind(a, y) != AST_Literal)
		return;
	int tt = ast_type_t(a, x);
	if (ast_bad_type(tt) || ast_bad_type(ast_type_t(a, y)))
		return;
	if (is_float(tt) || is_float(ast_type_t(a, y)))
		return;
	uint64_t l1 = value64(ast_ival(a, x), tt);
	uint64_t l2 = value64(ast_ival(a, y), ast_type_t(a, y));
	int ok = 1;
	uint64_t r = ast_fold_eval(op, tt, l1, l2, &ok);
	if (!ok)
		return;
	ast_set_kind(a, n, AST_Literal);
	ast_clear_children(a, n);
	ast_set_op(a, n, ast_op(a, x) | (ast_op(a, y) & VT_NONCONST));
	ast_set_type(a, n, tt, ast_type_ref(a, x));
	ast_set_ival(a, n, value64(r, tt));
	ast_set_sym(a, n, 0);
	ast_tmpl_folds++;
}

static void ast_run_templates(AstArena *a) {
	ast_tmpl_folds = 0;
	ast_fold_rec(a, ast_root(a));
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
};

static uint64_t ast_bfold_mul128(uint64_t a, uint64_t b, uint64_t *lo) {
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

static int ast_bfold_sq_gt(uint64_t s, uint64_t hi, uint64_t lo) {
	uint64_t sqlo, sqhi = ast_bfold_mul128(s, s, &sqlo);
	return sqhi > hi || (sqhi == hi && sqlo > lo);
}

static double ast_bfold_sqrt(double x) {
	uint64_t ix;
	memcpy(&ix, &x, sizeof ix);
	if (ix == 0 || ix == 0x8000000000000000ull || ix == 0x7ff0000000000000ull)
		return x;
	int e = (int)(ix >> 52) & 0x7ff;
	uint64_t m = ix & 0x000fffffffffffffull;
	int E;
	if (e == 0) {
		E = -1074;
		while (m < (1ull << 52)) {
			m <<= 1;
			E--;
		}
	} else {
		E = e - 1075;
		m |= 1ull << 52;
	}
	if (E & 1) {
		m <<= 1;
		E--;
	}
	double dm = (double)m, r;
	uint64_t g;
	memcpy(&g, &dm, sizeof g);
	g = (g >> 1) + 0x1ff8000000000000ull;
	memcpy(&r, &g, sizeof r);
	for (int i = 0; i < 5; i++)
		r = 0.5 * (r + dm / r);
	uint64_t S = (uint64_t)(r * 67108864.0);
	uint64_t hi = m >> 10, lo = m << 54;
	if (S < (1ull << 52))
		S = 1ull << 52;
	if (S > (1ull << 53) - 1)
		S = (1ull << 53) - 1;
	while (!ast_bfold_sq_gt(2 * S + 1, hi, lo))
		S++;
	while (ast_bfold_sq_gt(2 * S - 1, hi, lo))
		S--;
	uint64_t rbits = ((uint64_t)(E / 2 - 26 + 52 + 1023) << 52) + (S - (1ull << 52));
	memcpy(&r, &rbits, sizeof r);
	return r;
}

static double ast_bfold_trunc(double x) {
	if (x >= 9007199254740992.0 || x <= -9007199254740992.0)
		return x;
	uint64_t ix, it;
	double t = (double)(int64_t)x;
	memcpy(&ix, &x, sizeof ix);
	memcpy(&it, &t, sizeof it);
	it |= ix & 0x8000000000000000ull;
	memcpy(&t, &it, sizeof it);
	return t;
}

static double ast_bfold_floor(double x) {
	double t = ast_bfold_trunc(x);
	return t > x ? t - 1.0 : t;
}

static double ast_bfold_ceil(double x) {
	double t = ast_bfold_trunc(x);
	return t < x ? t + 1.0 : t;
}

static double ast_bfold_round(double x) {
	double t = ast_bfold_trunc(x);
	double d = x - t;
	if (d >= 0.5)
		return t + 1.0;
	if (d <= -0.5)
		return t - 1.0;
	return t;
}

static int ast_bfold_eval_f(int id, uint32_t b0, uint32_t b1, uint64_t *out) {
	float x0, x1, r;
	uint32_t rb;
	memcpy(&x0, &b0, sizeof x0);
	memcpy(&x1, &b1, sizeof x1);
	if (x0 != x0 || x1 != x1)
		return 0;
	switch (id) {
	case 0:
		if (x0 < 0)
			return 0;
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
	default:
		if (x0 == 0 && x1 == 0 && ((b0 ^ b1) >> 31))
			return 0;
		r = id == 6 ? (x0 < x1 ? x0 : x1) : (x0 > x1 ? x0 : x1);
		break;
	}
	memcpy(&rb, &r, sizeof rb);
	*out = rb;
	return 1;
}

static int ast_bfold_eval_d(int id, uint64_t b0, uint64_t b1, uint64_t *out) {
	double x0, x1, r;
	memcpy(&x0, &b0, sizeof x0);
	memcpy(&x1, &b1, sizeof x1);
	if (x0 != x0 || x1 != x1)
		return 0;
	switch (id) {
	case 0:
		if (x0 < 0)
			return 0;
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
	default:
		if (x0 == 0 && x1 == 0 && ((b0 ^ b1) >> 63))
			return 0;
		r = id == 6 ? (x0 < x1 ? x0 : x1) : (x0 > x1 ? x0 : x1);
		break;
	}
	memcpy(out, &r, sizeof r);
	return 1;
}

static AstLocal ast_bfold_arg(AstArena *a, AstLocal arg, int bt) {
	while (ast_kind(a, arg) == AST_Convert && ast_nchild(a, arg) == 1 &&
				 (ast_type_t(a, arg) & VT_BTYPE) == bt)
		arg = ast_first_child(a, arg);
	if (ast_kind(a, arg) != AST_Literal ||
			(ast_op(a, arg) & (VT_VALMASK | VT_LVAL | VT_SYM | VT_NONCONST)) != VT_CONST ||
			(ast_type_t(a, arg) & VT_BTYPE) != bt)
		return AST_NONE;
	return arg;
}

static int ast_bfold_run(AstArena *a) {
	int folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Invoke)
			continue;
		AstLocal cref = ast_first_child(a, n);
		if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
			continue;
		Sym *cs = (Sym *)(uintptr_t)ast_sym(a, cref);
		if (!cs || (cs->type.t & VT_BTYPE) != VT_FUNC || (cs->type.t & VT_STATIC))
			continue;
		ElfSym *es = elfsym(cs);
		if (es && es->st_shndx != SHN_UNDEF)
			continue;
		const char *nm = get_tok_str(cs->v, NULL);
		int nfn = (int)(sizeof ast_bfold_tab / sizeof *ast_bfold_tab);
		int bi;
		for (bi = 0; bi < nfn; bi++)
			if (!strcmp(nm, ast_bfold_tab[bi].name))
				break;
		if (bi == nfn)
			continue;
		int bt = ast_bfold_tab[bi].flt ? VT_FLOAT : VT_DOUBLE;
		int nargs = ast_bfold_tab[bi].nargs;
		if ((ast_type_t(a, n) & VT_BTYPE) != bt ||
				(int)ast_nchild(a, n) != nargs + 1)
			continue;
		uint64_t ab[2] = {0, 0};
		int i;
		for (i = 0; i < nargs; i++) {
			AstLocal lit = ast_bfold_arg(a, ast_child(a, n, i + 1), bt);
			if (lit == AST_NONE)
				break;
			ab[i] = ast_ival(a, lit);
		}
		if (i < nargs)
			continue;
		uint64_t res;
		int ok = ast_bfold_tab[bi].flt
								 ? ast_bfold_eval_f(ast_bfold_tab[bi].id, (uint32_t)ab[0],
																		(uint32_t)ab[1], &res)
								 : ast_bfold_eval_d(ast_bfold_tab[bi].id, ab[0], ab[1], &res);
		if (!ok)
			continue;
		MCC_TRACE("bfold %s id=%d nargs=%d flt=%d res=0x%llx\n", nm,
							(int)ast_bfold_tab[bi].id, nargs, (int)ast_bfold_tab[bi].flt,
							(unsigned long long)res);
		ast_set_kind(a, n, AST_Literal);
		ast_clear_children(a, n);
		ast_set_op(a, n, VT_CONST);
		ast_set_type(a, n, bt, 0);
		ast_set_ival(a, n, res);
		ast_set_sym(a, n, 0);
		folds++;
	}
	return folds;
}

static int ast_ident_intt(int tt) {
	if (tt & VT_BITFIELD)
		return 0;
	switch (tt & VT_BTYPE) {
	case VT_BYTE:
	case VT_SHORT:
	case VT_INT:
	case VT_LLONG:
	case VT_BOOL:
		return 1;
	}
	return 0;
}

static int ast_ident_common(int t1, int t2) {
	if ((t1 & VT_BTYPE) == VT_LLONG || (t2 & VT_BTYPE) == VT_LLONG) {
		if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED) ||
				(t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_LLONG | VT_UNSIGNED))
			return VT_LLONG | VT_UNSIGNED;
		return VT_LLONG;
	}
	if ((t1 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED) ||
			(t2 & (VT_BTYPE | VT_UNSIGNED)) == (VT_INT | VT_UNSIGNED))
		return VT_INT | VT_UNSIGNED;
	return VT_INT;
}

static int ast_ident_keep(int lt, int xt) {
	return ast_ident_common(lt, xt) == ast_ident_common(xt, xt);
}

static int ast_ident_etype(AstArena *a, AstLocal n, int *tt, uint64_t *ref) {
	switch (ast_kind(a, n)) {
	case AST_Literal:
	case AST_Ref:
	case AST_Convert:
	case AST_Invoke:
		*tt = ast_type_t(a, n);
		*ref = ast_type_ref(a, n);
		return 1;
	case AST_Unary:
		switch (ast_op(a, n)) {
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
			return 0;
		if ((ct & VT_BTYPE) != VT_PTR || !cref)
			return 0;
		Sym *ps = (Sym *)(uintptr_t)cref;
		*tt = ps->type.t;
		*ref = (uint64_t)(uintptr_t)ps->type.ref;
		return 1;
	}
	case AST_Binary: {
		int op = ast_op(a, n);
		switch (op) {
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
			return 0;
		int t1, t2;
		uint64_t r1, r2;
		if (!ast_ident_etype(a, ast_child(a, n, 0), &t1, &r1))
			return 0;
		if (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR) {
			if (!ast_ident_intt(t1))
				return 0;
			*tt = ast_ident_common(t1, t1);
			*ref = 0;
			return 1;
		}
		if (!ast_ident_etype(a, ast_child(a, n, 1), &t2, &r2))
			return 0;
		int p1 = (t1 & VT_BTYPE) == VT_PTR, p2 = (t2 & VT_BTYPE) == VT_PTR;
		if (p1 || p2) {
			if (op == '-' && p1 && p2) {
				*tt = VT_PTRDIFF_T;
				*ref = 0;
				return 1;
			}
			if ((op != '+' && op != '-') || (p2 && op == '-'))
				return 0;
			*tt = (p1 ? t1 : t2) & ~(VT_ARRAY | VT_VLA);
			*ref = p1 ? r1 : r2;
			return 1;
		}
		switch (op) {
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
		if (!ast_ident_intt(t1) || !ast_ident_intt(t2))
			return 0;
		*tt = ast_ident_common(t1, t2);
		*ref = 0;
		return 1;
	}
	}
	return 0;
}

static int ast_ident_pure_compute(AstArena *a, AstLocal n) {
	if (ast_type_t(a, n) & VT_VOLATILE)
		return 0;
	switch (ast_kind(a, n)) {
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
			return 0;
		if ((ct & VT_BTYPE) != VT_PTR || !cref)
			return 0;
		if (((Sym *)(uintptr_t)cref)->type.t & VT_VOLATILE)
			return 0;
		break;
	}
	case AST_Unary:
		switch (ast_op(a, n)) {
		case AST_OP_ADDR:
		case AST_OP_MEMBER:
		case AST_OP_MEMBER_ARROW:
			break;
		default:
			return 0;
		}
		break;
	case AST_Binary:
		switch (ast_op(a, n)) {
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
		if (!ast_ident_pure(a, c))
			return 0;
	return 1;
}

static int ast_ident_same_scan(const AstArena *a, AstLocal x, AstLocal y) {
	if (a->kind[x] != a->kind[y] || a->op[x] != a->op[y] ||
			a->type_t[x] != a->type_t[y] || a->type_ref[x] != a->type_ref[y] ||
			a->ival[x] != a->ival[y] || a->fbits[x] != a->fbits[y] ||
			a->sym[x] != a->sym[y] || a->nchild[x] != a->nchild[y])
		return 0;
	AstLocal cx = a->first_child[x], cy = a->first_child[y];
	for (; cx != AST_NONE; cx = a->next_sib[cx], cy = a->next_sib[cy])
		if (!ast_ident_same_scan(a, cx, cy))
			return 0;
	return 1;
}

static int ast_ident_same(const AstArena *a, AstLocal x, AstLocal y) {
	ast_hash_sync(a);
	if (x < (AstLocal)ast_hash_cap && y < (AstLocal)ast_hash_cap &&
			ast_hash_of(a, x) != ast_hash_of(a, y)) {
#if MCC_CONFIG_AST_SHADOW
		if (ast_ident_same_scan(a, x, y)) {
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

static int ast_ident_cval(AstArena *a, AstLocal n, int *tt, uint64_t *v) {
	if (ast_kind(a, n) != AST_Literal)
		return 0;
	if ((ast_op(a, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
		return 0;
	int t = ast_type_t(a, n);
	if (!ast_ident_intt(t))
		return 0;
	*tt = t;
	*v = value64(ast_ival(a, n), t);
	return 1;
}

static int ast_ident_m1(uint64_t v, int ct) {
	if ((ct & VT_BTYPE) == VT_LLONG)
		return v == ~(uint64_t)0;
	return (uint32_t)v == 0xffffffffu;
}

static int ast_ident_leaf(AstArena *a, AstLocal n) {
	uint16_t k = ast_kind(a, n);
	return k == AST_Literal || k == AST_Ref;
}

static void ast_ident_adopt(AstArena *a, AstLocal n, AstLocal x) {
	a->epoch++;
	a->kind[n] = a->kind[x];
	a->op[n] = a->op[x];
	a->type_t[n] = a->type_t[x];
	a->type_ref[n] = a->type_ref[x];
	a->ival[n] = a->ival[x];
	a->fbits[n] = a->fbits[x];
	a->sym[n] = a->sym[x];
	a->cst[n] = a->cst[x];
	a->first_child[n] = a->first_child[x];
	a->last_child[n] = a->last_child[x];
	a->nchild[n] = a->nchild[x];
	for (AstLocal c = a->first_child[n]; c != AST_NONE; c = a->next_sib[c])
		a->parent[c] = n;
	a->first_child[x] = AST_NONE;
	a->last_child[x] = AST_NONE;
	a->nchild[x] = 0;
}

static void ast_ident_setlit(AstArena *a, AstLocal n, int tt, uint64_t v) {
	ast_set_kind(a, n, AST_Literal);
	ast_clear_children(a, n);
	ast_set_op(a, n, VT_CONST | VT_NONCONST);
	ast_set_type(a, n, tt, 0);
	ast_set_ival(a, n, value64(v, tt));
	ast_set_sym(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_cst(a, n, 0);
}

static int ast_ii_width(int tt) {
	switch (tt & VT_BTYPE) {
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

static int ast_ident_convert(AstArena *a, AstLocal n) {
	AstLocal c, x;
	int tt, ct, xt;
	if (ast_kind(a, n) != AST_Convert || ast_nchild(a, n) != 1)
		return 0;
	tt = ast_type_t(a, n);
	if (!ast_ident_intt(tt) || (tt & VT_VOLATILE))
		return 0;
	c = ast_first_child(a, n);
	ct = ast_type_t(a, c);
	if (!ast_ident_intt(ct) || (ct & VT_VOLATILE))
		return 0;
	if ((tt & (VT_BTYPE | VT_UNSIGNED)) == (ct & (VT_BTYPE | VT_UNSIGNED))) {
		ast_ident_adopt(a, n, c);
		return 2;
	}
	if (ast_kind(a, c) == AST_Convert && ast_nchild(a, c) == 1) {
		x = ast_first_child(a, c);
		xt = ast_type_t(a, x);
		if (ast_ident_intt(xt) && !(xt & VT_VOLATILE) &&
				(tt & (VT_BTYPE | VT_UNSIGNED)) == (xt & (VT_BTYPE | VT_UNSIGNED)) &&
				(ct & VT_UNSIGNED) == (xt & VT_UNSIGNED) &&
				ast_ii_width(ct) >= ast_ii_width(tt)) {
			ast_ident_adopt(a, n, x);
			return 2;
		}
	}
	return 0;
}

static int ast_ident_node(AstArena *a, AstLocal n) {
	int cr = ast_ident_convert(a, n);
	if (cr)
		return cr;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		return 0;
	int op = ast_op(a, n);
	AstLocal x = ast_child(a, n, 0), y = ast_child(a, n, 1);
	int tx, ty;
	uint64_t rx, ry;
	if (!ast_ident_etype(a, x, &tx, &rx) || !ast_ident_etype(a, y, &ty, &ry))
		return 0;
	if (!ast_ident_intt(tx) || !ast_ident_intt(ty))
		return 0;
	int lt;
	uint64_t lv;
	int ct = ast_ident_common(tx, ty);
	switch (op) {
	case TOK_SHL:
	case TOK_SHR:
	case TOK_SAR:
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 0) {
			ast_ident_adopt(a, n, x);
			return 1;
		}
		return 0;
	case '+':
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 0 && ast_ident_keep(lt, tx)) {
			ast_ident_adopt(a, n, x);
			return 1;
		}
		if (ast_ident_cval(a, x, &lt, &lv) && lv == 0 && ast_ident_keep(lt, ty)) {
			ast_ident_adopt(a, n, y);
			return 1;
		}
		return 0;
	case '-':
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 0 && ast_ident_keep(lt, tx)) {
			ast_ident_adopt(a, n, x);
			return 1;
		}
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) {
			ast_ident_setlit(a, n, ct, 0);
			return 2;
		}
		return 0;
	case '/':
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 1 && ast_ident_keep(lt, tx)) {
			ast_ident_adopt(a, n, x);
			return 1;
		}
		return 0;
	case '*':
		if (ast_ident_cval(a, y, &lt, &lv)) {
			if (lv == 1 && ast_ident_keep(lt, tx)) {
				ast_ident_adopt(a, n, x);
				return 1;
			}
			if (lv == 0 && ast_ident_pure(a, x)) {
				int sig = ast_ident_leaf(a, x) ? 1 : 2;
				ast_ident_setlit(a, n, ct, 0);
				return sig;
			}
		}
		if (ast_ident_cval(a, x, &lt, &lv)) {
			if (lv == 1 && ast_ident_keep(lt, ty)) {
				ast_ident_adopt(a, n, y);
				return 1;
			}
			if (lv == 0 && ast_ident_pure(a, y)) {
				int sig = ast_ident_leaf(a, y) ? 1 : 2;
				ast_ident_setlit(a, n, ct, 0);
				return sig;
			}
		}
		return 0;
	case '&':
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) {
			ast_ident_adopt(a, n, x);
			return 2;
		}
		if (ast_ident_cval(a, y, &lt, &lv)) {
			if (ast_ident_m1(lv, ct) && ast_ident_keep(lt, tx)) {
				ast_ident_adopt(a, n, x);
				return 1;
			}
			if (lv == 0 && ast_ident_pure(a, x)) {
				int sig = ast_ident_leaf(a, x) ? 1 : 2;
				ast_ident_setlit(a, n, ct, 0);
				return sig;
			}
		}
		if (ast_ident_cval(a, x, &lt, &lv)) {
			if (ast_ident_m1(lv, ct) && ast_ident_keep(lt, ty)) {
				ast_ident_adopt(a, n, y);
				return 1;
			}
			if (lv == 0 && ast_ident_pure(a, y)) {
				int sig = ast_ident_leaf(a, y) ? 1 : 2;
				ast_ident_setlit(a, n, ct, 0);
				return sig;
			}
		}
		return 0;
	case '|':
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) {
			ast_ident_adopt(a, n, x);
			return 2;
		}
		if (ast_ident_cval(a, y, &lt, &lv)) {
			if (lv == 0 && ast_ident_keep(lt, tx)) {
				ast_ident_adopt(a, n, x);
				return 1;
			}
			if (ast_ident_m1(lv, ct) && ast_ident_pure(a, x)) {
				int sig = ast_ident_leaf(a, x) ? 1 : 2;
				ast_ident_setlit(a, n, ct, ~(uint64_t)0);
				return sig;
			}
		}
		if (ast_ident_cval(a, x, &lt, &lv)) {
			if (lv == 0 && ast_ident_keep(lt, ty)) {
				ast_ident_adopt(a, n, y);
				return 1;
			}
			if (ast_ident_m1(lv, ct) && ast_ident_pure(a, y)) {
				int sig = ast_ident_leaf(a, y) ? 1 : 2;
				ast_ident_setlit(a, n, ct, ~(uint64_t)0);
				return sig;
			}
		}
		return 0;
	case '^':
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) {
			ast_ident_setlit(a, n, ct, 0);
			return 2;
		}
		if (ast_ident_cval(a, y, &lt, &lv) && lv == 0 && ast_ident_keep(lt, tx)) {
			ast_ident_adopt(a, n, x);
			return 1;
		}
		if (ast_ident_cval(a, x, &lt, &lv) && lv == 0 && ast_ident_keep(lt, ty)) {
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
		/* V-ident(c): `x OP x` folds to a compile-time 0/1 for any relational OP and any
		 * pure integer x (the operands are integer — the ast_ident_intt gate at entry — so
		 * there is no float NaN concern). `<`,`>`,`!=` are always 0; `<=`,`>=`,`==` always 1;
		 * signed/unsigned is immaterial when comparing a value to itself. A relational node
		 * stores no result type (type_t==0); the C result type is int (VT_INT). */
		if (ast_ident_same(a, x, y) && ast_ident_pure(a, x)) {
			int one = (op == TOK_EQ || op == TOK_LE || op == TOK_GE || op == TOK_ULE ||
								 op == TOK_UGE);
			ast_ident_setlit(a, n, VT_INT, one ? 1u : 0u);
			return 2;
		}
		/* V-ident(c): unsigned range against 0 — the unsigned minimum is 0, so for any
		 * unsigned u: `u >= 0` and `0 <= u` are always 1, `u < 0` and `0 > u` always 0.
		 * The relational op is the SIGNED token (TOK_GE/LT/LE/GT); the comparison is
		 * unsigned iff the operand type carries VT_UNSIGNED (checked on the non-zero side).
		 * The discarded operand must be pure. Signed `x >= 0` is left alone (value-dependent). */
		if (op == TOK_GE && (tx & VT_UNSIGNED) && ast_ident_cval(a, y, &lt, &lv) &&
				lv == 0 && ast_ident_pure(a, x)) {
			ast_ident_setlit(a, n, VT_INT, 1u); /* u >= 0 */
			return 2;
		}
		if (op == TOK_LT && (tx & VT_UNSIGNED) && ast_ident_cval(a, y, &lt, &lv) &&
				lv == 0 && ast_ident_pure(a, x)) {
			ast_ident_setlit(a, n, VT_INT, 0u); /* u < 0 */
			return 2;
		}
		if (op == TOK_LE && (ty & VT_UNSIGNED) && ast_ident_cval(a, x, &lt, &lv) &&
				lv == 0 && ast_ident_pure(a, y)) {
			ast_ident_setlit(a, n, VT_INT, 1u); /* 0 <= u */
			return 2;
		}
		if (op == TOK_GT && (ty & VT_UNSIGNED) && ast_ident_cval(a, x, &lt, &lv) &&
				lv == 0 && ast_ident_pure(a, y)) {
			ast_ident_setlit(a, n, VT_INT, 0u); /* 0 > u */
			return 2;
		}
		return 0;
	}
	return 0;
}

static int ast_ident_folds;

static int ast_ident_rec(AstArena *a, AstLocal n) {
	int sig = 0;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		sig += ast_ident_rec(a, c);
	int r = ast_ident_node(a, n);
	if (r) {
		ast_ident_folds++;
		if (r == 2)
			sig++;
	}
	return sig;
}

static int ast_ident_run(AstArena *a) {
	int sig = 0, folds;
	ast_ident_folds = 0;
	do {
		folds = ast_ident_folds;
		sig += ast_ident_rec(a, ast_root(a));
	} while (ast_ident_folds != folds);
	return sig;
}

static int ast_narrow_bs(int t) {
	return t & (VT_BTYPE | VT_UNSIGNED);
}

static int ast_narrow_class(AstArena *a, AstLocal op, int tt) {
	int ot;
	uint64_t oref;
	if (!ast_ident_etype(a, op, &ot, &oref) || !ast_ident_intt(ot) ||
			(ot & VT_VOLATILE))
		return -1;
	if (ast_narrow_bs(ot) == ast_narrow_bs(tt))
		return 0;
	if (ast_kind(a, op) == AST_Convert && ast_nchild(a, op) == 1) {
		AstLocal inner = ast_first_child(a, op);
		int it;
		uint64_t iref;
		if (ast_ident_etype(a, inner, &it, &iref) && ast_ident_intt(it) &&
				!(it & VT_VOLATILE) && ast_narrow_bs(it) == ast_narrow_bs(tt) &&
				(ot & VT_UNSIGNED) == (it & VT_UNSIGNED) &&
				ast_ii_width(ot) >= ast_ii_width(tt))
			return 1;
	}
	{
		int lt;
		uint64_t lv;
		if (ast_ident_cval(a, op, &lt, &lv))
			return 2;
	}
	return 3;
}

static AstLocal ast_narrow_make(AstArena *a, AstLocal op, int tt, int cls) {
	switch (cls) {
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

static int ast_narrow_binop(int op) {
	switch (op) {
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

static int ast_narrow_binary(AstArena *a, AstLocal bin, int tt) {
	if (ast_kind(a, bin) != AST_Binary || ast_nchild(a, bin) != 2)
		return 0;
	if (!ast_ident_intt(tt) || (tt & VT_VOLATILE) || ast_ii_width(tt) < 4)
		return 0;
	int op = ast_op(a, bin);
	if (!ast_narrow_binop(op))
		return 0;
	int wt;
	uint64_t wref;
	if (!ast_ident_etype(a, bin, &wt, &wref) || !ast_ident_intt(wt) ||
			(wt & VT_VOLATILE) || ast_ii_width(wt) <= ast_ii_width(tt))
		return 0;
	AstLocal op0 = ast_child(a, bin, 0), op1 = ast_child(a, bin, 1);
	int c0 = ast_narrow_class(a, op0, tt);
	int c1 = ast_narrow_class(a, op1, tt);
	if (c0 < 0 || c1 < 0)
		return 0;
	if (c0 != 0 && c0 != 1 && c1 != 0 && c1 != 1)
		return 0;
	AstLocal na = ast_narrow_make(a, op0, tt, c0);
	AstLocal nb = ast_narrow_make(a, op1, tt, c1);
	ast_set_type(a, bin, tt, 0);
	ast_clear_children(a, bin);
	ast_add_child(a, bin, na);
	ast_add_child(a, bin, nb);
	return 1;
}

static int ast_narrow_node(AstArena *a, AstLocal n) {
	uint16_t k = ast_kind(a, n);
	if (k == AST_Convert && ast_nchild(a, n) == 1) {
		AstLocal bin = ast_first_child(a, n);
		if (ast_narrow_binary(a, bin, ast_type_t(a, n))) {
			ast_ident_adopt(a, n, bin);
			return 1;
		}
		return 0;
	}
	if (k == AST_Store && ast_nchild(a, n) == 2) {
		AstLocal lval = ast_child(a, n, 0), rval = ast_child(a, n, 1);
		int tt;
		uint64_t tref;
		if (!ast_ident_etype(a, lval, &tt, &tref))
			return 0;
		return ast_narrow_binary(a, rval, tt);
	}
	return 0;
}

static int ast_narrow_rec(AstArena *a, AstLocal n) {
	int sig = 0;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		sig += ast_narrow_rec(a, c);
	sig += ast_narrow_node(a, n);
	return sig;
}

static int ast_narrow_run(AstArena *a) {
	int total = ast_narrow_rec(a, ast_root(a));
	/* V-narrow(a): iterate to a fixpoint (MCC_AST_NARROW_FIX) so a narrowing that
	 * exposes another — a freshly narrowed operand letting its parent narrow — is
	 * caught in one strategy invocation instead of leaving residue for the next
	 * whole-pipeline round. Default off → single post-order pass (byte-identical);
	 * a search knob when on (each pass is individually sound, so the fixpoint only
	 * removes more casts, never changes semantics). */
	if (ast_narrow_fix_env) {
		int sig;
		do {
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

static int ast_cprop_is_local(AstArena *a, AstLocal n, int *off, int *tt) {
	if (n == AST_NONE || ast_kind(a, n) != AST_Ref)
		return 0;
	int r = ast_op(a, n);
	if ((r & VT_VALMASK) != VT_LOCAL || !(r & VT_LVAL) || (r & VT_SYM))
		return 0;
	int t = ast_type_t(a, n);
	if (!ast_ident_intt(t) || (t & VT_VOLATILE))
		return 0;
	*off = (int)(int64_t)ast_ival(a, n);
	*tt = t;
	return 1;
}

static int ast_cprop_escapes_scan(AstArena *a, int off) {
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Unary)
			continue;
		int op = ast_op(a, n);
		if (op != AST_OP_ADDR && op != AST_OP_MEMBER && op != AST_OP_MEMBER_ARROW)
			continue;
		AstLocal c = ast_first_child(a, n);
		if (c == AST_NONE || ast_kind(a, c) != AST_Ref)
			continue;
		int r = ast_op(a, c);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM) &&
				(int)(int64_t)ast_ival(a, c) == off)
			return 1;
	}
	return 0;
}

static int ast_cprop_escapes(AstArena *a, int off) {
	int r;
	ast_du_sync(a);
	if (ast_du_state < 0)
		r = ast_cprop_escapes_scan(a, off);
	else
		r = (ast_du_slot_flags(a, off) & AST_DU_ESCAPED) ? 1 : 0;
#if MCC_CONFIG_AST_SHADOW
	int s = ast_cprop_escapes_scan(a, off);
	if (r != s)
		ast_du_diverge("escapes", off, r, s);
#endif
	return r;
}

static int ast_cprop_safe_compute(AstArena *a, AstLocal n) {
	if (ast_type_t(a, n) & VT_VOLATILE)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Literal:
	case AST_Ref:
	case AST_Load:
	case AST_Convert:
	case AST_Binary:
		break;
	case AST_Unary:
		switch (ast_op(a, n)) {
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
			return 0;
		break;
	default:
		return 0;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (!ast_cprop_safe(a, c))
			return 0;
	return 1;
}

static int ast_cprop_find(int off) {
	for (int i = 0; i < ast_cprop_kn; i++)
		if (ast_cprop_koff[i] == off)
			return i;
	return -1;
}

static void ast_cprop_kill(int off) {
	int i = ast_cprop_find(off);
	if (i < 0)
		return;
	ast_cprop_kn--;
	ast_cprop_koff[i] = ast_cprop_koff[ast_cprop_kn];
	ast_cprop_ktt[i] = ast_cprop_ktt[ast_cprop_kn];
	ast_cprop_kval[i] = ast_cprop_kval[ast_cprop_kn];
}

static void ast_cprop_gen(int off, int tt, uint64_t v) {
	int i = ast_cprop_find(off);
	if (i < 0) {
		if (ast_cprop_kn >= ast_cprop_window)
			return;
		i = ast_cprop_kn++;
		ast_cprop_koff[i] = off;
	}
	ast_cprop_ktt[i] = tt;
	ast_cprop_kval[i] = v;
}

static int ast_cprop_lval_op(int op) {
	switch (op) {
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

static void ast_cprop_rewrite(AstArena *a, AstLocal n, int lval) {
	if (n == AST_NONE)
		return;
	uint16_t k = ast_kind(a, n);
	if (k == AST_Ref && !lval) {
		int off, tt;
		if (ast_cprop_is_local(a, n, &off, &tt)) {
			int i = ast_cprop_find(off);
			if (i >= 0 && ast_cprop_ktt[i] == tt) {
				ast_ident_setlit(a, n, tt, ast_cprop_kval[i]);
				ast_cprop_folds++;
			}
		}
		return;
	}
	if (k == AST_Store) {
		ast_cprop_rewrite(a, ast_child(a, n, 0), 1);
		ast_cprop_rewrite(a, ast_child(a, n, 1), 0);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_cprop_rewrite(a, c, clval);
}

static void ast_cprop_block(AstArena *a, AstLocal bb) {
	ast_cprop_kn = 0;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) {
		uint16_t k = ast_kind(a, s);
		if (k == AST_Store) {
			AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
			if (!ast_cprop_safe(a, lval) || !ast_cprop_safe(a, val)) {
				ast_cprop_kn = 0;
				continue;
			}
			ast_cprop_rewrite(a, val, 0);
			ast_cprop_rewrite(a, lval, 1);
			int off, tt;
			if (ast_cprop_is_local(a, lval, &off, &tt) && !ast_cprop_escapes(a, off)) {
				int lt;
				uint64_t lv;
				if (ast_ident_cval(a, val, &lt, &lv) && lt == tt)
					ast_cprop_gen(off, tt, lv);
				else
					ast_cprop_kill(off);
			}
		} else if (k == AST_Return) {
			if (ast_cprop_safe(a, ast_first_child(a, s)))
				ast_cprop_rewrite(a, ast_first_child(a, s), 0);
			ast_cprop_kn = 0;
		} else if (k == AST_If && ast_op(a, s) == 0) {
			AstLocal cond = ast_child(a, s, 0);
			if (ast_cprop_safe(a, cond))
				ast_cprop_rewrite(a, cond, 0);
			ast_cprop_kn = 0;
		} else {
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

static void ast_cprop_state_save(AstCpropState *st) {
	st->kn = ast_cprop_kn;
	memcpy(st->koff, ast_cprop_koff, (size_t)ast_cprop_kn * sizeof(int));
	memcpy(st->ktt, ast_cprop_ktt, (size_t)ast_cprop_kn * sizeof(int));
	memcpy(st->kval, ast_cprop_kval, (size_t)ast_cprop_kn * sizeof(uint64_t));
}

static void ast_cprop_state_load(const AstCpropState *st) {
	ast_cprop_kn = st->kn;
	memcpy(ast_cprop_koff, st->koff, (size_t)st->kn * sizeof(int));
	memcpy(ast_cprop_ktt, st->ktt, (size_t)st->kn * sizeof(int));
	memcpy(ast_cprop_kval, st->kval, (size_t)st->kn * sizeof(uint64_t));
}

static void ast_cprop_state_meet(const AstCpropState *st) {
	int i = 0;
	while (i < ast_cprop_kn) {
		int j, keep = 0;
		for (j = 0; j < st->kn; j++)
			if (st->koff[j] == ast_cprop_koff[i]) {
				keep = st->ktt[j] == ast_cprop_ktt[i] &&
							 st->kval[j] == ast_cprop_kval[i];
				break;
			}
		if (keep)
			i++;
		else
			ast_cprop_kill(ast_cprop_koff[i]);
	}
}

static int ast_cprop_opaque(AstArena *a, AstLocal n) {
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Jump)
		return 1;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (ast_cprop_opaque(a, c))
			return 1;
	return 0;
}

static unsigned char *ast_cprop_vis;
static int ast_licm_written(AstArena *a, AstLocal n, int off);
static int ast_sccp_has_label(AstArena *a, AstLocal n);

static void ast_cprop_stmt(AstArena *a, AstLocal s);

static void ast_cprop_stmts(AstArena *a, AstLocal bb) {
	if (bb == AST_NONE || ast_kind(a, bb) != AST_BasicBlock)
		return;
	ast_cprop_vis[bb] = 1;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s))
		ast_cprop_stmt(a, s);
}

static int ast_cprop_arm_clean(AstArena *a, AstLocal n) {
	if (n == AST_NONE)
		return 1;
	uint16_t k = ast_kind(a, n);
	if (k == AST_Return || k == AST_Jump)
		return 0;
	if (k == AST_If && ast_op(a, n) >= 2 && ast_op(a, n) <= 6)
		return 0;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (!ast_cprop_arm_clean(a, c))
			return 0;
	return 1;
}

#define AST_CPROP_SWMAX 64

static int ast_cprop_switch_meet(AstArena *a, AstLocal sw) {
	AstLocal val = ast_child(a, sw, 0);
	AstLocal body = ast_child(a, sw, 1);
	if (body == AST_NONE || ast_kind(a, body) != AST_BasicBlock)
		return 0;
	if (val == AST_NONE || !ast_cprop_safe(a, val))
		return 0;
	if (ast_sccp_has_label(a, sw))
		return 0;
	AstLocal segstart[AST_CPROP_SWMAX], segend[AST_CPROP_SWMAX];
	int nseg = 0, has_default = 0, pending = 0, inbody = 0, first = 1;
	AstLocal cs = AST_NONE, ce = AST_NONE;
	for (AstLocal c = ast_first_child(a, body); c != AST_NONE;
			 c = ast_next_sib(a, c)) {
		uint16_t ck = ast_kind(a, c);
		int cop = ck == AST_Jump ? ast_op(a, c) : -1;
		if (cop == 2 || cop == 3) {
			if (cop == 3)
				has_default = 1;
			if (inbody)
				return 0;
			pending = 1;
		} else if (cop == 0) {
			if (nseg >= AST_CPROP_SWMAX)
				return 0;
			if (inbody) {
				segstart[nseg] = cs;
				segend[nseg] = ce;
				nseg++;
				inbody = 0;
			} else if (pending) {
				segstart[nseg] = AST_NONE;
				segend[nseg] = AST_NONE;
				nseg++;
			} else {
				return 0;
			}
			pending = 0;
		} else {
			if (cop >= 0 || ck == AST_Return)
				return 0;
			if (!ast_cprop_arm_clean(a, c))
				return 0;
			if (!inbody) {
				if (first || !pending)
					return 0;
				cs = c;
				inbody = 1;
			}
			ce = c;
		}
		first = 0;
	}
	if (inbody) {
		if (nseg >= AST_CPROP_SWMAX)
			return 0;
		segstart[nseg] = cs;
		segend[nseg] = ce;
		nseg++;
	} else if (pending) {
		if (nseg >= AST_CPROP_SWMAX)
			return 0;
		segstart[nseg] = AST_NONE;
		segend[nseg] = AST_NONE;
		nseg++;
	}
	if (!has_default)
		return 0;
	ast_cprop_vis[body] = 1;
	ast_cprop_rewrite(a, val, 0);
	AstCpropState in, acc;
	ast_cprop_state_save(&in);
	int have = 0;
	for (int i = 0; i < nseg; i++) {
		ast_cprop_state_load(&in);
		if (segstart[i] != AST_NONE)
			for (AstLocal s = segstart[i];; s = ast_next_sib(a, s)) {
				ast_cprop_stmt(a, s);
				if (s == segend[i])
					break;
			}
		if (!have) {
			ast_cprop_state_save(&acc);
			have = 1;
		} else {
			ast_cprop_state_meet(&acc);
			ast_cprop_state_save(&acc);
		}
	}
	if (have)
		ast_cprop_state_load(&acc);
	return 1;
}

static void ast_cprop_stmt(AstArena *a, AstLocal s) {
	uint16_t k = ast_kind(a, s);
	if (k == AST_Store) {
		AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
		if (!ast_cprop_safe(a, lval) || !ast_cprop_safe(a, val)) {
			ast_cprop_kn = 0;
			return;
		}
		ast_cprop_rewrite(a, val, 0);
		ast_cprop_rewrite(a, lval, 1);
		int off, tt;
		if (ast_cprop_is_local(a, lval, &off, &tt) && !ast_cprop_escapes(a, off)) {
			int lt;
			uint64_t lv;
			if (ast_ident_cval(a, val, &lt, &lv) && lt == tt)
				ast_cprop_gen(off, tt, lv);
			else
				ast_cprop_kill(off);
		}
	} else if (k == AST_Return) {
		if (ast_first_child(a, s) != AST_NONE &&
				ast_cprop_safe(a, ast_first_child(a, s)))
			ast_cprop_rewrite(a, ast_first_child(a, s), 0);
		ast_cprop_kn = 0;
	} else if (k == AST_BasicBlock) {
		ast_cprop_stmts(a, s);
	} else if (k == AST_If && ast_op(a, s) == 0) {
		AstLocal cond = ast_child(a, s, 0);
		AstLocal tb = ast_child(a, s, 1), eb = ast_child(a, s, 2);
		if (cond != AST_NONE && ast_cprop_safe(a, cond))
			ast_cprop_rewrite(a, cond, 0);
		if (ast_cprop_opaque(a, tb) || ast_cprop_opaque(a, eb)) {
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
	} else if (k == AST_If && ast_op(a, s) >= 2 && ast_op(a, s) <= 6) {
		if (ast_op(a, s) == 6 && ast_cprop_switch_meet(a, s))
			return;
		for (int i = 0; i < ast_cprop_kn;)
			if (ast_licm_written(a, s, ast_cprop_koff[i]))
				ast_cprop_kill(ast_cprop_koff[i]);
			else
				i++;
		if (ast_sccp_has_label(a, s)) {
			ast_cprop_kn = 0;
			return;
		}
		AstCpropState in;
		ast_cprop_state_save(&in);
		for (AstLocal c = ast_first_child(a, s); c != AST_NONE;
				 c = ast_next_sib(a, c)) {
			if (ast_kind(a, c) == AST_BasicBlock) {
				ast_cprop_stmts(a, c);
				ast_cprop_state_load(&in);
			} else if (ast_cprop_safe(a, c)) {
				ast_cprop_rewrite(a, c, 0);
			}
		}
	} else if (k == AST_Invoke && ast_call_window_env) {
		for (int i = 0; i < ast_cprop_kn;)
			if (ast_cprop_escapes(a, ast_cprop_koff[i]) ||
					ast_licm_written(a, s, ast_cprop_koff[i]))
				ast_cprop_kill(ast_cprop_koff[i]);
			else
				i++;
	} else {
		ast_cprop_kn = 0;
	}
}

static int ast_cprop_run(AstArena *a) {
	ast_cprop_folds = 0;
	AstLocal nn = ast_count(a);
	if (ast_cprop_join_env && nn) {
		ast_cprop_vis = mcc_mallocz(nn);
		ast_cprop_kn = 0;
		ast_cprop_stmts(a, ast_root(a));
		for (AstLocal n = 0; n < nn; n++)
			if (ast_kind(a, n) == AST_BasicBlock && !ast_cprop_vis[n])
				ast_cprop_block(a, n);
		mcc_free(ast_cprop_vis);
		ast_cprop_vis = NULL;
		return ast_cprop_folds;
	}
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_BasicBlock)
			ast_cprop_block(a, n);
	return ast_cprop_folds;
}

#define AST_DSE_MAX 128
static int ast_dse_koff[AST_DSE_MAX];
static AstLocal ast_dse_kstore[AST_DSE_MAX];
static int ast_dse_kn;
static int ast_dse_folds;

static int ast_dse_find(int off) {
	for (int i = 0; i < ast_dse_kn; i++)
		if (ast_dse_koff[i] == off)
			return i;
	return -1;
}

static void ast_dse_kill(int off) {
	int i = ast_dse_find(off);
	if (i < 0)
		return;
	ast_dse_kn--;
	ast_dse_koff[i] = ast_dse_koff[ast_dse_kn];
	ast_dse_kstore[i] = ast_dse_kstore[ast_dse_kn];
}

static void ast_dse_kill_reads(AstArena *a, AstLocal n) {
	if (n == AST_NONE)
		return;
	if (ast_kind(a, n) == AST_Ref) {
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM))
			ast_dse_kill((int)(int64_t)ast_ival(a, n));
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_dse_kill_reads(a, c);
}

static void ast_dse_gen(int off, AstLocal store) {
	int i = ast_dse_find(off);
	if (i < 0) {
		if (ast_dse_kn >= AST_DSE_MAX)
			return;
		i = ast_dse_kn++;
		ast_dse_koff[i] = off;
	}
	ast_dse_kstore[i] = store;
}

static void ast_dse_block(AstArena *a, AstLocal bb) {
	ast_dse_kn = 0;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) {
		if (ast_kind(a, s) != AST_Store) {
			/* V-dse: a bare call statement cannot write a NON-ESCAPING tracked local (its
			 * address never escaped, so no pointer — global or argument — can reach it) and
			 * reads it only via its own arguments. So instead of the conservative full reset,
			 * kill only the locals the call reads (kill_reads scans the arg subtree; a nested
			 * store to a local reads its Ref lval → also killed, staying conservative) and
			 * keep the rest of the dead-store tracking. Correct by the same escape analysis
			 * DSE already relies on. Off by default (MCC_AST_DSE_CALL) → full reset, byte-
			 * identical; a search-explorable knob when on. Control-flow / asm / other kinds
			 * still reset — only AST_Invoke (a call) is seen through. */
			if (ast_dse_call_env && ast_kind(a, s) == AST_Invoke) {
				ast_dse_kill_reads(a, s);
				continue;
			}
			ast_dse_kn = 0;
			continue;
		}
		AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
		ast_dse_kill_reads(a, val);
		if (!ast_cprop_safe(a, val)) {
			ast_dse_kn = 0;
			continue;
		}
		int off, tt;
		if (ast_cprop_is_local(a, lval, &off, &tt) && !ast_cprop_escapes(a, off)) {
			int i = ast_dse_find(off);
			if (i >= 0) {
				ast_set_kind(a, ast_dse_kstore[i], AST_Poison);
				ast_clear_children(a, ast_dse_kstore[i]);
				ast_dse_folds++;
			}
			ast_dse_gen(off, s);
		} else {
			ast_dse_kn = 0;
		}
	}
}

static int ast_dse_run(AstArena *a) {
	ast_dse_folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_BasicBlock)
			ast_dse_block(a, n);
	return ast_dse_folds;
}

static int ast_sccp_folds;

static int ast_sccp_has_label_compute(AstArena *a, AstLocal n) {
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Jump && ast_op(a, n) == 4)
		return 1;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (ast_sccp_has_label(a, c))
			return 1;
	return 0;
}

/* One scan: fold every constant-condition two-arm If to its taken arm. Returns the
 * number folded this scan. */
static int ast_sccp_scan(AstArena *a) {
	int folded = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_If || ast_op(a, n) != 0)
			continue;
		AstLocal cond = ast_child(a, n, 0);
		int tt;
		uint64_t v;
		if (!ast_ident_cval(a, cond, &tt, &v))
			continue;
		AstLocal thenbb = ast_child(a, n, 1);
		AstLocal elsebb = ast_child(a, n, 2);
		AstLocal taken = v ? thenbb : elsebb;
		AstLocal dead = v ? elsebb : thenbb;
		if (ast_sccp_has_label(a, dead))
			continue;
		if (taken == AST_NONE) {
			ast_set_kind(a, n, AST_Poison);
			ast_clear_children(a, n);
		} else {
			ast_set_kind(a, n, AST_BasicBlock);
			ast_clear_children(a, n);
			ast_add_child(a, n, taken);
		}
		folded++;
	}
	return folded;
}

static int ast_sccp_run(AstArena *a) {
	ast_sccp_folds = ast_sccp_scan(a);
	/* V-sccp: fuse cprop+sccp to a fixpoint (MCC_AST_SCCP_FIX). Folding a constant
	 * branch can expose new constants (a value written only on the removed arm becomes
	 * provably constant); cprop propagates them, sccp folds the newly-constant branches,
	 * and so on. Correct-by-construction and terminating: cprop only *adds* constants and
	 * sccp only *removes* dead branches — both monotonic, neither reverts the other, so
	 * the loop converges (bounded by node count). Default off → single scan (byte-
	 * identical); a search knob when on. */
	if (ast_sccp_fix_env) {
		for (;;) {
			int c = ast_cprop_run(a);
			int s = ast_sccp_scan(a);
			ast_sccp_folds += s;
			if (c == 0 && s == 0)
				break;
		}
	}
	return ast_sccp_folds;
}

static int ast_jt_folds;

static int ast_jt_arm_empty(AstArena *a, AstLocal arm) {
	return arm == AST_NONE ||
				 (ast_kind(a, arm) == AST_BasicBlock && ast_nchild(a, arm) == 0);
}

static int ast_jt_run(AstArena *a) {
	ast_jt_folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_If || ast_op(a, n) != 0)
			continue;
		AstLocal cond = ast_child(a, n, 0);
		if (cond == AST_NONE || !ast_ident_pure(a, cond))
			continue;
		AstLocal thenbb = ast_child(a, n, 1);
		AstLocal elsebb = ast_child(a, n, 2);
		if (ast_jt_arm_empty(a, thenbb) && ast_jt_arm_empty(a, elsebb)) {
			ast_set_kind(a, n, AST_Poison);
			ast_clear_children(a, n);
			ast_jt_folds++;
			continue;
		}
		if (thenbb != AST_NONE && elsebb != AST_NONE &&
				ast_kind(a, thenbb) == AST_BasicBlock &&
				ast_kind(a, elsebb) == AST_BasicBlock &&
				!ast_sccp_has_label(a, thenbb) && !ast_sccp_has_label(a, elsebb) &&
				ast_ident_same(a, thenbb, elsebb)) {
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

static int ast_tco_reads_off(AstArena *a, AstLocal n, int off) {
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Ref) {
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM) &&
				(int)(int64_t)ast_ival(a, n) == off)
			return 1;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (ast_tco_reads_off(a, c, off))
			return 1;
	return 0;
}

static int ast_tco_run(AstArena *a, Sym *fsym) {
	ast_tco_folds = 0;
	if (!fsym || !fsym->type.ref)
		return 0;
	if (fsym->type.ref->f.func_type != FUNC_NEW)
		return 0;
	int poff[AST_TCO_MAXP], ptt[AST_TCO_MAXP];
	uint64_t pref[AST_TCO_MAXP];
	int np = 0;
	for (Sym *p = fsym->type.ref->next; p; p = p->next) {
		int v = p->v & ~SYM_FIELD;
		if (v < TOK_IDENT || np >= ast_tco_maxp)
			return 0;
		Sym *ls = sym_find(v);
		if (!ls)
			return 0;
		if ((ls->r & VT_VALMASK) != VT_LOCAL || !(ls->r & VT_LVAL) || (ls->r & VT_SYM))
			return 0;
		int t = ls->type.t;
		/* V-tco(c): pointer params. The param store/reload below is type-generic
		 * (ast_set_type with the captured ptt/pref), so a pointer stores/reloads exactly
		 * like an integer of the same width — accept VT_PTR under MCC_AST_TCO_PTR (default
		 * off → int-only, byte-identical). Arrays/VLAs/volatile still excluded. */
		if ((!ast_ident_intt(t) &&
				 !(ast_tco_ptr_env && (t & VT_BTYPE) == VT_PTR)) ||
				(t & VT_VOLATILE) || (t & (VT_ARRAY | VT_VLA)))
			return 0;
		poff[np] = (int)ls->c;
		ptt[np] = t;
		pref[np] = (uint64_t)(uintptr_t)ls->type.ref;
		np++;
	}
	if (np < 1)
		return 0;
	for (int i = 0; i < np; i++)
		if (ast_cprop_escapes(a, poff[i]))
			return 0;

	int converted = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal r = 0; r < nn; r++) {
		if (ast_kind(a, r) != AST_Return)
			continue;
		AstLocal inv = ast_first_child(a, r);
		if (inv == AST_NONE || ast_kind(a, inv) != AST_Invoke)
			continue;
		if ((int)ast_nchild(a, inv) != np + 1)
			continue;
		AstLocal cref = ast_child(a, inv, 0);
		if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
			continue;
		if (!(ast_op(a, cref) & VT_SYM) ||
				(void *)(uintptr_t)ast_sym(a, cref) != (void *)fsym)
			continue;
		AstLocal arg[AST_TCO_MAXP];
		int ok = 1;
		for (int i = 0; i < np; i++) {
			arg[i] = ast_child(a, inv, i + 1);
			if (!ast_cprop_safe(a, arg[i])) {
				ok = 0;
				break;
			}
		}
		if (!ok)
			continue;
		int need[AST_TCO_MAXP], emitted[AST_TCO_MAXP], order[AST_TCO_MAXP];
		int nwrite = 0;
		for (int i = 0; i < np; i++) {
			need[i] = 1;
			if (ast_kind(a, arg[i]) == AST_Ref) {
				int rr = ast_op(a, arg[i]);
				if ((rr & VT_VALMASK) == VT_LOCAL && !(rr & VT_SYM) &&
						(int)(int64_t)ast_ival(a, arg[i]) == poff[i])
					need[i] = 0;
			}
			emitted[i] = !need[i];
			if (need[i])
				nwrite++;
		}
		int no = 0, cyc = 0;
		while (no < nwrite) {
			int pick = -1;
			for (int i = 0; i < np; i++) {
				if (emitted[i])
					continue;
				int blocked = 0;
				for (int k = 0; k < np; k++) {
					if (k == i || emitted[k] || !need[k])
						continue;
					if (ast_tco_reads_off(a, arg[k], poff[i])) {
						blocked = 1;
						break;
					}
				}
				if (!blocked) {
					pick = i;
					break;
				}
			}
			if (pick < 0) {
				cyc = 1;
				break;
			}
			emitted[pick] = 1;
			order[no++] = pick;
		}
		if (cyc)
			continue;
		ast_set_kind(a, r, AST_BasicBlock);
		ast_clear_children(a, r);
		for (int oi = 0; oi < no; oi++) {
			int i = order[oi];
			AstLocal lref = ast_node(a, AST_Ref);
			ast_set_op(a, lref, VT_LOCAL | VT_LVAL);
			ast_set_ival(a, lref, (uint64_t)poff[i]);
			ast_set_type(a, lref, ptt[i], pref[i]);
			AstLocal cvt = ast_node(a, AST_Convert);
			ast_set_type(a, cvt, ptt[i], pref[i]);
			ast_add_child(a, cvt, arg[i]);
			AstLocal st = ast_node(a, AST_Store);
			ast_add_child(a, st, lref);
			ast_add_child(a, st, cvt);
			ast_add_child(a, r, st);
		}
		AstLocal jmp = ast_node(a, AST_Jump);
		ast_set_op(a, jmp, 5);
		ast_set_ival(a, jmp, (uint64_t)(unsigned)AST_TCO_LABEL);
		ast_add_child(a, r, jmp);
		converted++;
	}
	if (!converted)
		return 0;

	AstLocal root = ast_root(a);
	AstLocal lbl = ast_node(a, AST_Jump);
	ast_set_op(a, lbl, 4);
	ast_set_ival(a, lbl, (uint64_t)(unsigned)AST_TCO_LABEL);
	AstLocal first = ast_first_child(a, root);
	ast_clear_children(a, root);
	ast_add_child(a, root, lbl);
	for (AstLocal c = first; c != AST_NONE;) {
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

static int ast_cse_regpure_compute(AstArena *a, AstLocal n) {
	int t = ast_type_t(a, n);
	if (t & VT_VOLATILE)
		return 0;
	switch (ast_kind(a, n)) {
	case AST_Literal:
		return 1;
	case AST_Ref: {
		int r = ast_op(a, n);
		if ((r & VT_VALMASK) != VT_LOCAL || (r & VT_SYM))
			return 0;
		if ((t & VT_BITFIELD) || !ast_ident_intt(t))
			return 0;
		return 1;
	}
	case AST_Convert:
		if (t & VT_BITFIELD)
			return 0;
		break;
	case AST_Binary:
		switch (ast_op(a, n)) {
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
		if (!ast_cse_regpure(a, c))
			return 0;
	return 1;
}

static void ast_cse_setref(AstArena *a, AstLocal n, AstLocal ref) {
	a->epoch++;
	ast_clear_children(a, n);
	a->kind[n] = AST_Ref;
	a->op[n] = a->op[ref];
	a->type_t[n] = a->type_t[ref];
	a->type_ref[n] = a->type_ref[ref];
	a->ival[n] = a->ival[ref];
	a->fbits[n] = a->fbits[ref];
	a->sym[n] = a->sym[ref];
	a->cst[n] = a->cst[ref];
}

/* V-cse(b): commutative-aware match. `a OP b` and `b OP a` compute the same value
 * for a commutative OP (+ * & | ^ on int or float — IEEE add/mul are commutative), so
 * reusing the first's cached result for the second is correct. Only the top-level
 * commutative pair is reordered; deeper structure still needs exact ast_ident_same.
 * Off by default (MCC_AST_CSE_COMM) → exact match only, byte-identical. */
static int ast_cse_commutative_op(int op) {
	return op == '+' || op == '*' || op == '&' || op == '|' || op == '^';
}

static int ast_cse_same(AstArena *a, AstLocal x, AstLocal y) {
	if (ast_ident_same(a, x, y))
		return 1;
	if (ast_cse_comm_env && ast_kind(a, x) == AST_Binary &&
			ast_kind(a, y) == AST_Binary && ast_nchild(a, x) == 2 &&
			ast_nchild(a, y) == 2 && ast_op(a, x) == ast_op(a, y) &&
			ast_cse_commutative_op(ast_op(a, x)) && ast_type_t(a, x) == ast_type_t(a, y)) {
		AstLocal x0 = ast_child(a, x, 0), x1 = ast_child(a, x, 1);
		AstLocal y0 = ast_child(a, y, 0), y1 = ast_child(a, y, 1);
		return ast_ident_same(a, x0, y1) && ast_ident_same(a, x1, y0);
	}
	return 0;
}

static int ast_cse_try_match(AstArena *a, AstLocal n) {
	for (int i = 0; i < ast_cse_n; i++) {
		if (ast_cse_expr[i] == n)
			continue;
		if (ast_cse_same(a, ast_cse_expr[i], n)) {
			ast_cse_setref(a, n, ast_cse_ref[i]);
			ast_cse_folds++;
			return 1;
		}
	}
	return 0;
}

static void ast_cse_subst(AstArena *a, AstLocal n, int lval) {
	if (n == AST_NONE)
		return;
	uint16_t k = ast_kind(a, n);
	if (!lval && ast_cse_try_match(a, n))
		return;
	if (k == AST_Store) {
		ast_cse_subst(a, ast_child(a, n, 0), 1);
		ast_cse_subst(a, ast_child(a, n, 1), 0);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_cse_subst(a, c, clval);
}

static void ast_cse_kill(AstArena *a, int off) {
	for (int i = 0; i < ast_cse_n;) {
		if (ast_cse_off[i] == off || ast_tco_reads_off(a, ast_cse_expr[i], off)) {
			ast_cse_n--;
			ast_cse_expr[i] = ast_cse_expr[ast_cse_n];
			ast_cse_ref[i] = ast_cse_ref[ast_cse_n];
			ast_cse_off[i] = ast_cse_off[ast_cse_n];
		} else {
			i++;
		}
	}
}

static int ast_licm_folds;

static int ast_licm_written(AstArena *a, AstLocal n, int off) {
	if (n == AST_NONE)
		return 0;
	uint16_t k = ast_kind(a, n);
	if (k == AST_Store && ast_ref_is_local_off(a, ast_child(a, n, 0), off))
		return 1;
	if (k == AST_Unary && ast_ref_is_local_off(a, ast_first_child(a, n), off))
		return 1;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (ast_licm_written(a, c, off))
			return 1;
	return 0;
}

static int ast_licm_operands_ok(AstArena *a, AstLocal loop, AstLocal e) {
	if (e == AST_NONE)
		return 1;
	int off, tt;
	if (ast_cprop_is_local(a, e, &off, &tt)) {
		if (ast_cprop_escapes(a, off))
			return 0;
		if (ast_licm_written(a, loop, off))
			return 0;
	}
	for (AstLocal c = ast_first_child(a, e); c != AST_NONE; c = ast_next_sib(a, c))
		if (!ast_licm_operands_ok(a, loop, c))
			return 0;
	return 1;
}

static void ast_licm_subst(AstArena *a, AstLocal n, AstLocal e, AstLocal ref,
													 int lval) {
	if (n == AST_NONE)
		return;
	uint16_t k = ast_kind(a, n);
	if (!lval && n != e && ast_ident_same(a, e, n)) {
		ast_cse_setref(a, n, ref);
		ast_licm_folds++;
		return;
	}
	if (k == AST_Store) {
		ast_licm_subst(a, ast_child(a, n, 0), e, ref, 1);
		ast_licm_subst(a, ast_child(a, n, 1), e, ref, 0);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_licm_subst(a, c, e, ref, clval);
}

static int ast_licm_is_loop(AstArena *a, AstLocal s) {
	if (ast_kind(a, s) != AST_If)
		return 0;
	int op = ast_op(a, s);
	return op == 2 || op == 3 || op == 4 || op == 5;
}

static long ast_cost_score(AstArena *a) {
	AstLocal nn = ast_count(a), n, p;
	int nodes = (int)nn, calls = 0, maxdepth = 0;
	for (n = 0; n < nn; n++) {
		if (ast_kind(a, n) == AST_Invoke)
			calls++;
		if (ast_licm_is_loop(a, n)) {
			int d = 1;
			for (p = ast_parent(a, n); p != AST_NONE; p = ast_parent(a, p))
				if (ast_licm_is_loop(a, p))
					d++;
			if (d > maxdepth)
				maxdepth = d;
		}
	}
	return (long)nodes * (maxdepth + 1) * (calls + 1);
}

static void ast_fn_cost(AstArena *a, const char *fn) {
	AstLocal nn = ast_count(a), n, p;
	int nodes = (int)nn, calls = 0, maxdepth = 0;
	for (n = 0; n < nn; n++) {
		if (ast_kind(a, n) == AST_Invoke)
			calls++;
		if (ast_licm_is_loop(a, n)) {
			int d = 1;
			for (p = ast_parent(a, n); p != AST_NONE; p = ast_parent(a, p))
				if (ast_licm_is_loop(a, p))
					d++;
			if (d > maxdepth)
				maxdepth = d;
		}
	}
	fprintf(stderr, "ast-cost: %s nodes=%d loopdepth=%d calls=%d score=%ld\n", fn,
					nodes, maxdepth, calls, ast_cost_score(a));
}

static int ast_bf_eqkey(AstArena *a, AstLocal n, AstLocal *key) {
	AstLocal l, r;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != TOK_EQ ||
			ast_nchild(a, n) != 2)
		return 0;
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) {
		*key = l;
		return 1;
	}
	if (ast_kind(a, l) == AST_Literal) {
		*key = r;
		return 1;
	}
	return 0;
}

static int ast_bf_cond_key(AstArena *a, AstLocal n, AstLocal *key) {
	AstLocal cond;
	if (ast_kind(a, n) != AST_If || ast_op(a, n) != 0)
		return 0;
	cond = ast_first_child(a, n);
	return cond != AST_NONE && ast_bf_eqkey(a, cond, key);
}

static void ast_bf_report(AstArena *a, const char *fn) {
	AstLocal nn = ast_count(a), n, m;
	for (n = 0; n < nn; n++) {
		AstLocal key, k2;
		int cnt = 0, dup = 0;
		if (!ast_bf_cond_key(a, n, &key))
			continue;
		for (m = 0; m < nn; m++) {
			if (!ast_bf_cond_key(a, m, &k2) || !ast_ident_same(a, key, k2))
				continue;
			if (m < n) {
				dup = 1;
				break;
			}
			cnt++;
		}
		if (!dup && cnt >= 3)
			fprintf(stderr, "bitflag-candidate: %s cluster=%d\n", fn, cnt);
	}
}

static int ast_bf_folds;

static AstLocal ast_dup_sub(AstArena *a, AstLocal n) {
	AstLocal d = ast_node(a, ast_kind(a, n));
	ast_set_op(a, d, ast_op(a, n));
	ast_set_type(a, d, ast_type_t(a, n), ast_type_ref(a, n));
	ast_set_ival(a, d, ast_ival(a, n));
	ast_set_fbits(a, d, ast_fbits(a, n));
	ast_set_sym(a, d, ast_sym(a, n));
	ast_set_cst(a, d, ast_cst(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		ast_add_child(a, d, ast_dup_sub(a, c));
	return d;
}

static int ast_bf_cmpconst(AstArena *a, AstLocal n, int cmpop, AstLocal *key,
													 uint64_t *v) {
	AstLocal l, r, k, c;
	int ct, kt;
	uint64_t cv, kref;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != cmpop ||
			ast_nchild(a, n) != 2)
		return 0;
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) {
		k = l;
		c = r;
	} else if (ast_kind(a, l) == AST_Literal) {
		k = r;
		c = l;
	} else {
		return 0;
	}
	if (!ast_ident_cval(a, c, &ct, &cv))
		return 0;
	if (!ast_ident_etype(a, k, &kt, &kref) || !ast_ident_intt(kt))
		return 0;
	if (!ast_ident_pure(a, k))
		return 0;
	*key = k;
	*v = cv;
	return 1;
}

static int ast_bf_eqconst(AstArena *a, AstLocal n, AstLocal *key, uint64_t *v) {
	return ast_bf_cmpconst(a, n, TOK_EQ, key, v);
}

#define AST_BF_MAXVALS 256

static int ast_bf_cond_parse_op(AstArena *a, AstLocal cond, int joinop,
																int cmpop, AstLocal *key, uint64_t *vals,
																int *cnt) {
	AstLocal k;
	uint64_t v;
	if (ast_bf_cmpconst(a, cond, cmpop, &k, &v)) {
		if (*key == AST_NONE)
			*key = k;
		else if (!ast_ident_same(a, *key, k))
			return 0;
		if (*cnt >= AST_BF_MAXVALS)
			return 0;
		vals[(*cnt)++] = v;
		return 1;
	}
	if (ast_kind(a, cond) != AST_Binary || ast_op(a, cond) != joinop ||
			ast_nchild(a, cond) < 2)
		return 0;
	for (AstLocal c = ast_first_child(a, cond); c != AST_NONE;
			 c = ast_next_sib(a, c)) {
		if (!ast_bf_cmpconst(a, c, cmpop, &k, &v))
			return 0;
		if (*key == AST_NONE)
			*key = k;
		else if (!ast_ident_same(a, *key, k))
			return 0;
		if (*cnt >= AST_BF_MAXVALS)
			return 0;
		vals[(*cnt)++] = v;
	}
	return 1;
}

static int ast_bf_cond_parse(AstArena *a, AstLocal cond, AstLocal *key,
														 uint64_t *vals, int *cnt) {
	return ast_bf_cond_parse_op(a, cond, TOK_LOR, TOK_EQ, key, vals, cnt);
}

static int ast_bf_window(const uint64_t *vals, int cnt, uint64_t *mask,
												 uint64_t *base) {
	int i;
	int64_t b = (int64_t)vals[0];
	uint64_t m = 0;
	for (i = 1; i < cnt; i++)
		if ((int64_t)vals[i] < b)
			b = (int64_t)vals[i];
	for (i = 0; i < cnt; i++) {
		uint64_t d = vals[i] - (uint64_t)b;
		if (d > 63)
			return 0;
		m |= (uint64_t)1 << d;
	}
	*mask = m;
	*base = (uint64_t)b;
	return 1;
}

static int ast_bf_has_label(AstArena *a, AstLocal n) {
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Jump) {
		int op = ast_op(a, n);
		if (op == 2 || op == 3 || op == 4)
			return 1;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE;
			 c = ast_next_sib(a, c))
		if (ast_bf_has_label(a, c))
			return 1;
	return 0;
}

static AstLocal ast_bf_lit(AstArena *a, int tt, uint64_t v) {
	AstLocal n = ast_node(a, AST_Literal);
	ast_set_op(a, n, VT_CONST);
	ast_set_type(a, n, tt, 0);
	ast_set_ival(a, n, v);
	return n;
}

static AstLocal ast_bf_bin(AstArena *a, int op, int tt, AstLocal l, AstLocal r) {
	AstLocal n = ast_node(a, AST_Binary);
	ast_set_op(a, n, op);
	ast_set_type(a, n, tt, 0);
	ast_add_child(a, n, l);
	ast_add_child(a, n, r);
	return n;
}

static AstLocal ast_bf_ucast(AstArena *a, int tt, AstLocal key) {
	AstLocal n = ast_node(a, AST_Convert);
	ast_set_type(a, n, tt, 0);
	ast_add_child(a, n, ast_dup_sub(a, key));
	return n;
}

static AstLocal ast_bf_keyexpr(AstArena *a, int kw, AstLocal key,
															 uint64_t base) {
	AstLocal u = ast_bf_ucast(a, kw, key);
	if (base == 0)
		return u;
	return ast_bf_bin(a, '-', kw, u, ast_bf_lit(a, kw, base));
}

static AstLocal ast_bf_build(AstArena *a, AstLocal key, uint64_t mask,
														 uint64_t base) {
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

static void ast_bf_drop(AstArena *a, AstLocal n) {
	ast_set_kind(a, n, AST_Poison);
	ast_clear_children(a, n);
}

static int ast_bf_try_if(AstArena *a, AstLocal s) {
	AstLocal key = AST_NONE, drop[64];
	uint64_t vals[AST_BF_MAXVALS], mask = 0, base = 0;
	int cnt = 0, ndrop = 0;
	AstLocal cond0 = ast_child(a, s, 0);
	AstLocal thenbb = ast_child(a, s, 1);
	if (thenbb == AST_NONE || ast_kind(a, thenbb) != AST_BasicBlock)
		return 0;
	if (!ast_bf_cond_parse(a, cond0, &key, vals, &cnt))
		return 0;
	AstLocal tail = ast_child(a, s, 2);
	for (;;) {
		if (tail == AST_NONE || ast_kind(a, tail) != AST_BasicBlock ||
				ast_nchild(a, tail) != 1 || ndrop > 60)
			break;
		AstLocal inner = ast_first_child(a, tail);
		if (ast_kind(a, inner) != AST_If || ast_op(a, inner) != 0)
			break;
		AstLocal icond = ast_child(a, inner, 0);
		AstLocal ithen = ast_child(a, inner, 1);
		if (ithen == AST_NONE || !ast_ident_same(a, thenbb, ithen))
			break;
		if (ast_bf_has_label(a, icond) || ast_bf_has_label(a, ithen))
			break;
		AstLocal k2 = key;
		int c2 = cnt;
		if (!ast_bf_cond_parse(a, icond, &k2, vals, &c2))
			break;
		key = k2;
		cnt = c2;
		drop[ndrop++] = tail;
		drop[ndrop++] = inner;
		drop[ndrop++] = icond;
		tail = ast_child(a, inner, 2);
	}
	if (cnt < ast_bitflag_min || key == AST_NONE)
		return 0;
	if (!ast_bf_window(vals, cnt, &mask, &base))
		return 0;
	AstLocal cond = ast_bf_build(a, key, mask, base);
	ast_clear_children(a, s);
	ast_add_child(a, s, cond);
	ast_add_child(a, s, thenbb);
	if (tail != AST_NONE)
		ast_add_child(a, s, tail);
	ast_bf_drop(a, cond0);
	for (int i = 0; i < ndrop; i++)
		ast_bf_drop(a, drop[i]);
	return 1;
}

static int ast_bf_try_ifne(AstArena *a, AstLocal s) {
	AstLocal key = AST_NONE, drop[64];
	uint64_t vals[AST_BF_MAXVALS], mask = 0, base = 0;
	int cnt = 0, ndrop = 0;
	if (ast_child(a, s, 2) != AST_NONE)
		return 0;
	AstLocal cond0 = ast_child(a, s, 0);
	if (ast_bf_has_label(a, cond0))
		return 0;
	if (!ast_bf_cond_parse_op(a, cond0, TOK_LAND, TOK_NE, &key, vals, &cnt))
		return 0;
	AstLocal body = ast_child(a, s, 1);
	for (;;) {
		if (body == AST_NONE || ast_kind(a, body) != AST_BasicBlock ||
				ast_nchild(a, body) != 1 || ndrop > 60)
			break;
		AstLocal inner = ast_first_child(a, body);
		if (ast_kind(a, inner) != AST_If || ast_op(a, inner) != 0)
			break;
		if (ast_child(a, inner, 2) != AST_NONE)
			break;
		AstLocal icond = ast_child(a, inner, 0);
		if (ast_bf_has_label(a, icond))
			break;
		AstLocal k2 = key;
		int c2 = cnt;
		if (!ast_bf_cond_parse_op(a, icond, TOK_LAND, TOK_NE, &k2, vals, &c2))
			break;
		key = k2;
		cnt = c2;
		drop[ndrop++] = body;
		drop[ndrop++] = inner;
		drop[ndrop++] = icond;
		body = ast_child(a, inner, 1);
	}
	if (cnt < ast_bitflag_min || key == AST_NONE || body == AST_NONE)
		return 0;
	if (!ast_bf_window(vals, cnt, &mask, &base))
		return 0;
	AstLocal member = ast_bf_build(a, key, mask, base);
	AstLocal cond = ast_bf_bin(a, '^', VT_INT, member, ast_bf_lit(a, VT_INT, 1));
	ast_clear_children(a, s);
	ast_add_child(a, s, cond);
	ast_add_child(a, s, body);
	ast_bf_drop(a, cond0);
	for (int i = 0; i < ndrop; i++)
		ast_bf_drop(a, drop[i]);
	return 1;
}

static int ast_bf_try_lor(AstArena *a, AstLocal n) {
	AstLocal key = AST_NONE;
	uint64_t vals[AST_BF_MAXVALS], mask = 0, base = 0;
	int cnt = 0;
	if (!ast_bf_cond_parse(a, n, &key, vals, &cnt))
		return 0;
	if (cnt < ast_bitflag_min || key == AST_NONE)
		return 0;
	if (!ast_bf_window(vals, cnt, &mask, &base))
		return 0;
	AstLocal res = ast_bf_build(a, key, mask, base);
	AstLocal bit = ast_first_child(a, res);
	AstLocal guard = ast_next_sib(a, bit);
	ast_bf_drop(a, res);
	ast_set_op(a, n, '&');
	ast_set_type(a, n, VT_INT, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, bit);
	ast_add_child(a, n, guard);
	return 1;
}

static int ast_bf_try_land(AstArena *a, AstLocal n) {
	AstLocal key = AST_NONE;
	uint64_t vals[AST_BF_MAXVALS], mask = 0, base = 0;
	int cnt = 0;
	if (!ast_bf_cond_parse_op(a, n, TOK_LAND, TOK_NE, &key, vals, &cnt))
		return 0;
	if (cnt < ast_bitflag_min || key == AST_NONE)
		return 0;
	if (!ast_bf_window(vals, cnt, &mask, &base))
		return 0;
	AstLocal member = ast_bf_build(a, key, mask, base);
	ast_set_op(a, n, '^');
	ast_set_type(a, n, VT_INT, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, member);
	ast_add_child(a, n, ast_bf_lit(a, VT_INT, 1));
	return 1;
}

static int ast_bf_run(AstArena *a) {
	ast_bf_folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_If && ast_op(a, n) == 0)
			ast_bf_folds += ast_bf_try_if(a, n);
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_If && ast_op(a, n) == 0)
			ast_bf_folds += ast_bf_try_ifne(a, n);
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LOR)
			ast_bf_folds += ast_bf_try_lor(a, n);
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LAND)
			ast_bf_folds += ast_bf_try_land(a, n);
	return ast_bf_folds;
}

/* V-bf(a) range-predicate fold: rewrite a signed range test `lo <= x && x <= hi`
 * (as a condition, so the `&&` is a TOK_LAND AST_Binary node) into the branchless
 * `(unsigned)(x - lo) <= (hi - lo)`. Correct-by-construction for constant lo <= hi and a
 * pure integer key x (standard unsigned-subtract range identity); gcc emits this as
 * `sub; cmp; setbe`, mcc otherwise leaves two signed compares + two branches. */
static int ast_range_folds;

/* Parse one signed relational `x REL const` (literal on either side) into an inclusive
 * bound on the pure integer key: *is_lower=1 => x >= *bound, else x <= *bound. */
static int ast_range_bound(AstArena *a, AstLocal n, AstLocal *key, int64_t *bound,
													 int *is_lower) {
	int op = ast_op(a, n), eff, ct, kt, keyleft;
	AstLocal l, r, k, c;
	uint64_t cv, kref;
	int64_t cval;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		return 0;
	if (op != TOK_LE && op != TOK_GE && op != TOK_LT && op != TOK_GT)
		return 0;
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) {
		k = l;
		c = r;
		keyleft = 1;
	} else if (ast_kind(a, l) == AST_Literal) {
		k = r;
		c = l;
		keyleft = 0;
	} else {
		return 0;
	}
	if (!ast_ident_cval(a, c, &ct, &cv))
		return 0;
	if (!ast_ident_etype(a, k, &kt, &kref) || !ast_ident_intt(kt) || (kt & VT_UNSIGNED))
		return 0;
	if (!ast_ident_pure(a, k))
		return 0;
	cval = (int64_t)cv; /* value64 already sign-extended the signed literal */
	eff = op;
	if (!keyleft) /* `C REL x` mirrors to `x REL' C` */
		eff = op == TOK_LE ? TOK_GE : op == TOK_GE ? TOK_LE : op == TOK_LT ? TOK_GT : TOK_LT;
	switch (eff) {
	case TOK_LE:
		*is_lower = 0;
		*bound = cval;
		break;
	case TOK_LT:
		if (cval == INT64_MIN)
			return 0; /* x < INT64_MIN: empty half-range, and cval-1 would overflow */
		*is_lower = 0;
		*bound = cval - 1;
		break;
	case TOK_GE:
		*is_lower = 1;
		*bound = cval;
		break;
	case TOK_GT:
		if (cval == INT64_MAX)
			return 0;
		*is_lower = 1;
		*bound = cval + 1;
		break;
	default:
		return 0;
	}
	*key = k;
	return 1;
}

static int ast_range_try(AstArena *a, AstLocal n) {
	AstLocal c0 = ast_child(a, n, 0), c1 = ast_child(a, n, 1), k0, k1, key, kexpr, hlit;
	int64_t b0, b1, lo, hi;
	int low0, low1, kt, kw;
	uint64_t kref, span;
	if (!ast_range_bound(a, c0, &k0, &b0, &low0) ||
			!ast_range_bound(a, c1, &k1, &b1, &low1))
		return 0;
	if (low0 == low1 || !ast_ident_same(a, k0, k1)) /* need one lower + one upper on same x */
		return 0;
	key = k0;
	if (low0) {
		lo = b0;
		hi = b1;
	} else {
		lo = b1;
		hi = b0;
	}
	if (lo > hi) /* empty/inverted range: leave it (rare, and folds to a constant elsewhere) */
		return 0;
	ast_ident_etype(a, key, &kt, &kref);
	kw = (kt & VT_BTYPE) == VT_LLONG ? VT_LLONG | VT_UNSIGNED : VT_INT | VT_UNSIGNED;
	span = (uint64_t)hi - (uint64_t)lo; /* fits: modular width matches the key type */
	kexpr = ast_bf_keyexpr(a, kw, key, (uint64_t)lo);
	hlit = ast_bf_lit(a, kw, span);
	MCC_TRACE("range fold key=%u lo=%lld hi=%lld span=%llu kw=0x%x\n", (unsigned)key,
						(long long)lo, (long long)hi, (unsigned long long)span, kw);
	ast_set_op(a, n, TOK_ULE);
	ast_set_type(a, n, VT_INT, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, kexpr);
	ast_add_child(a, n, hlit);
	return 1;
}

/* OR/out-of-range form: parse a signed relational into a KEPT-range bound. `is_below=1`
 * means the term excludes values below the range (sets the kept lower bound); else it
 * excludes values above (sets the kept upper bound). Mirror of ast_range_bound. */
static int ast_range_bound_or(AstArena *a, AstLocal n, AstLocal *key, int64_t *bound,
															int *is_below) {
	int op = ast_op(a, n), eff, ct, kt, keyleft;
	AstLocal l, r, k, c;
	uint64_t cv, kref;
	int64_t cval;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		return 0;
	if (op != TOK_LE && op != TOK_GE && op != TOK_LT && op != TOK_GT)
		return 0;
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) {
		k = l;
		c = r;
		keyleft = 1;
	} else if (ast_kind(a, l) == AST_Literal) {
		k = r;
		c = l;
		keyleft = 0;
	} else {
		return 0;
	}
	if (!ast_ident_cval(a, c, &ct, &cv))
		return 0;
	if (!ast_ident_etype(a, k, &kt, &kref) || !ast_ident_intt(kt) || (kt & VT_UNSIGNED))
		return 0;
	if (!ast_ident_pure(a, k))
		return 0;
	cval = (int64_t)cv;
	eff = op;
	if (!keyleft)
		eff = op == TOK_LE ? TOK_GE : op == TOK_GE ? TOK_LE : op == TOK_LT ? TOK_GT : TOK_LT;
	switch (eff) {
	case TOK_LT: /* x<C excluded -> kept lo=C */
		*is_below = 1;
		*bound = cval;
		break;
	case TOK_LE: /* x<=C excluded -> kept lo=C+1 */
		if (cval == INT64_MAX)
			return 0;
		*is_below = 1;
		*bound = cval + 1;
		break;
	case TOK_GT: /* x>C excluded -> kept hi=C */
		*is_below = 0;
		*bound = cval;
		break;
	case TOK_GE: /* x>=C excluded -> kept hi=C-1 */
		if (cval == INT64_MIN)
			return 0;
		*is_below = 0;
		*bound = cval - 1;
		break;
	default:
		return 0;
	}
	*key = k;
	return 1;
}

/* `x < lo || x > hi` (an out-of-range/bounds check, TOK_LOR of two relationals) ->
 * `(unsigned)(x - lo) > (hi - lo)`. The complement of ast_range_try; same identity. */
static int ast_range_try_lor(AstArena *a, AstLocal n) {
	AstLocal c0 = ast_child(a, n, 0), c1 = ast_child(a, n, 1), k0, k1, key, kexpr, hlit;
	int64_t b0, b1, lo, hi;
	int bel0, bel1, kt, kw;
	uint64_t kref, span;
	if (!ast_range_bound_or(a, c0, &k0, &b0, &bel0) ||
			!ast_range_bound_or(a, c1, &k1, &b1, &bel1))
		return 0;
	if (bel0 == bel1 || !ast_ident_same(a, k0, k1))
		return 0;
	key = k0;
	if (bel0) {
		lo = b0;
		hi = b1;
	} else {
		lo = b1;
		hi = b0;
	}
	if (lo > hi)
		return 0;
	ast_ident_etype(a, key, &kt, &kref);
	kw = (kt & VT_BTYPE) == VT_LLONG ? VT_LLONG | VT_UNSIGNED : VT_INT | VT_UNSIGNED;
	span = (uint64_t)hi - (uint64_t)lo;
	kexpr = ast_bf_keyexpr(a, kw, key, (uint64_t)lo);
	hlit = ast_bf_lit(a, kw, span);
	MCC_TRACE("range fold(or) key=%u lo=%lld hi=%lld span=%llu kw=0x%x\n", (unsigned)key,
						(long long)lo, (long long)hi, (unsigned long long)span, kw);
	ast_set_op(a, n, TOK_UGT);
	ast_set_type(a, n, VT_INT, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, kexpr);
	ast_add_child(a, n, hlit);
	return 1;
}

static int ast_range_run(AstArena *a) {
	AstLocal nn = ast_count(a);
	ast_range_folds = 0;
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LAND &&
				ast_nchild(a, n) == 2)
			ast_range_folds += ast_range_try(a, n);
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LOR &&
				ast_nchild(a, n) == 2)
			ast_range_folds += ast_range_try_lor(a, n);
	return ast_range_folds;
}

/* Constant unsigned division/remainder strength reduction: `x / C` and `x % C` for a
 * 32-bit unsigned x and a non-power-of-2 constant C become a high-multiply + shift, using
 * the magic (M, s) proven exhaustively in tools/asttool.c:suite_magic. This first cut
 * handles the clean subset where the magic needs no overflow-add correction (`mag.a==0`),
 * so x is evaluated exactly once (in the mul-high) — no expensive duplication. The magic
 * arithmetic is trusted (selftested); the remaining risk is only this AST construction,
 * which the full M8 regimen validates. Signed division and the add-correction case are
 * deferred (they need a shared-quotient temp / duplication). */
static int ast_divmagic_folds;

static int ast_divmagic_try(AstArena *a, AstLocal n) {
	int op = ast_op(a, n), nt, ct, xt;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1), xu, prod, hi, hi32;
	uint64_t nref, xref, cv;
	uint32_t C;
	MccMagicU mag;
	const int U64 = VT_LLONG | VT_UNSIGNED, U32 = VT_INT | VT_UNSIGNED;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != U32)
		return 0; /* only 32-bit unsigned division */
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		return 0;
	C = (uint32_t)cv;
	if (C < 3 || (C & (C - 1)) == 0) /* skip 0/1/2 and powers of 2 (backend does shr/and) */
		return 0;
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != U32)
		return 0; /* dividend must be a genuine unsigned int (u64 cast zero-extends) */
	if (!ast_ident_pure(a, x))
		return 0;
	mag = mcc_magicu(C);
	if (mag.a && mag.s < 1) /* add-correction needs s>=1 for the (s-1) shift; skip degenerate */
		return 0;
	{
		AstLocal inner;
		uint64_t shamt;
		/* hi32 = (uint32)(((uint64)x * M) >> 32)  — the mul-high, mirrors mcc_divu_apply */
		xu = ast_bf_ucast(a, U64, x); /* (u64)x, dups x */
		prod = ast_bf_bin(a, '*', U64, xu, ast_bf_lit(a, U64, mag.M));
		hi = ast_bf_bin(a, TOK_SHR, U64, prod, ast_bf_lit(a, U64, 32));
		hi32 = ast_node(a, AST_Convert);
		ast_set_type(a, hi32, U32, 0);
		ast_add_child(a, hi32, hi);
		if (!mag.a) { /* quotient = hi32 >> s */
			inner = hi32;
			shamt = (uint64_t)mag.s;
		} else { /* add-correction: t = ((x - hi32) >> 1) + hi32; quotient = t >> (s-1) */
			AstLocal sub = ast_bf_bin(a, '-', U32, ast_dup_sub(a, x), hi32);
			AstLocal shr1 = ast_bf_bin(a, TOK_SHR, U32, sub, ast_bf_lit(a, U32, 1));
			inner = ast_bf_bin(a, '+', U32, shr1, ast_dup_sub(a, hi32)); /* dup -> 2x mul-high */
			shamt = (uint64_t)(mag.s - 1);
		}
		MCC_TRACE("divmagic %s C=%u M=0x%x s=%d add=%d\n", op == '/' ? "div" : "rem", C, mag.M,
							mag.s, mag.a);
		ast_set_type(a, n, U32, 0);
		ast_set_ival(a, n, 0);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_set_cst(a, n, 0);
		ast_clear_children(a, n);
		if (op == '/') { /* n := inner >> shamt */
			ast_set_op(a, n, TOK_SHR);
			ast_add_child(a, n, inner);
			ast_add_child(a, n, ast_bf_lit(a, U32, shamt));
		} else { /* x % C = x - (x/C)*C */
			AstLocal q = ast_bf_bin(a, TOK_SHR, U32, inner, ast_bf_lit(a, U32, shamt));
			AstLocal qC = ast_bf_bin(a, '*', U32, q, ast_bf_lit(a, U32, C));
			ast_set_op(a, n, '-');
			ast_add_child(a, n, ast_dup_sub(a, x));
			ast_add_child(a, n, qC);
		}
	}
	return 1;
}

/* Signed division/remainder by a positive power of two, which mcc otherwise lowers to a
 * `cltd; idiv` (~20-40 cyc) instead of gcc's cheap shift/and sequence. Correct-by-
 * construction round-toward-zero identity: `x / 2^k = (x + ((x>>31) & (2^k-1))) >> k`
 * (arithmetic shifts), and `x % 2^k = x - (x/2^k << k)`. Only the pure dividend x is
 * duplicated (a cheap re-load, no multiply), so unlike the general signed magic this needs
 * no shared-quotient temp. */
static int ast_divmagic_try_spow2(AstArena *a, AstLocal n) {
	int op = ast_op(a, n), nt, ct, xt, k, neg;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1), bias, sum;
	uint64_t nref, xref, cv, t, ac;
	int64_t C;
	const int S32 = VT_INT;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != VT_INT)
		return 0; /* signed 32-bit only */
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		return 0;
	C = (int64_t)cv;
	neg = C < 0;
	ac = (uint64_t)(neg ? -C : C); /* |C| */
	if (ac < 2 || (ac & (ac - 1)) != 0) /* |C| a power of two >= 2 (handles negative C too) */
		return 0;
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != VT_INT)
		return 0;
	if (!ast_ident_pure(a, x))
		return 0;
	for (k = 0, t = ac; t > 1; t >>= 1)
		k++;
	/* bias = (x >> 31) & (|C|-1); sum = x + bias */
	bias = ast_bf_bin(a, '&', S32,
										ast_bf_bin(a, TOK_SAR, S32, ast_dup_sub(a, x), ast_bf_lit(a, S32, 31)),
										ast_bf_lit(a, S32, ac - 1));
	sum = ast_bf_bin(a, '+', S32, ast_dup_sub(a, x), bias);
	MCC_TRACE("divmagic spow2 %s C=%lld k=%d neg=%d\n", op == '/' ? "div" : "rem", (long long)C,
						k, neg);
	if (op == '/') { /* q = sum >> k (arithmetic); x/(-2^k) = -q */
		ast_set_type(a, n, S32, 0);
		ast_set_ival(a, n, 0);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_set_cst(a, n, 0);
		ast_clear_children(a, n);
		if (neg) { /* n := 0 - (sum >> k) */
			AstLocal q = ast_bf_bin(a, TOK_SAR, S32, sum, ast_bf_lit(a, S32, (uint64_t)k));
			ast_set_op(a, n, '-');
			ast_add_child(a, n, ast_bf_lit(a, S32, 0));
			ast_add_child(a, n, q);
		} else { /* n := sum >> k */
			ast_set_op(a, n, TOK_SAR);
			ast_add_child(a, n, sum);
			ast_add_child(a, n, ast_bf_lit(a, S32, (uint64_t)k));
		}
	} else { /* n := x - ((sum >> k) << k)  — same result for -2^k as for 2^k (x%C == x%|C|) */
		AstLocal quot = ast_bf_bin(a, TOK_SAR, S32, sum, ast_bf_lit(a, S32, (uint64_t)k));
		AstLocal shl = ast_bf_bin(a, TOK_SHL, S32, quot, ast_bf_lit(a, S32, (uint64_t)k));
		AstLocal xdup = ast_dup_sub(a, x);
		ast_set_op(a, n, '-');
		ast_set_type(a, n, S32, 0);
		ast_set_ival(a, n, 0);
		ast_set_fbits(a, n, 0);
		ast_set_sym(a, n, 0);
		ast_set_cst(a, n, 0);
		ast_clear_children(a, n);
		ast_add_child(a, n, xdup);
		ast_add_child(a, n, shl);
	}
	return 1;
}

/* Signed non-power-of-two `x / C` and `x % C` via the (exhaustively selftested) signed
 * magic, mirroring mcc_divs_apply. The sign-bit correction reuses the shifted quotient, so
 * the mul-high is duplicated once (CSE runs earlier in the pipeline, so it isn't merged) — a
 * 2× multiply for `/` (3× for `% = x - (x/C)*C`), still a clear win over `idiv`. */
static int ast_divmagic_try_signed(AstArena *a, AstLocal n) {
	int op = ast_op(a, n), nt, ct, xt;
	AstLocal x = ast_child(a, n, 0), cnode = ast_child(a, n, 1);
	AstLocal Mi, xi, prod, hi, q0, q1, q2, cvt, signbit;
	uint64_t nref, xref, cv, ac;
	int64_t C;
	MccMagicS mag;
	const int S64 = VT_LLONG, S32 = VT_INT, U32 = VT_INT | VT_UNSIGNED;
	if (!ast_ident_etype(a, n, &nt, &nref) || (nt & (VT_BTYPE | VT_UNSIGNED)) != VT_INT)
		return 0; /* signed 32-bit */
	if (ast_kind(a, cnode) != AST_Literal || !ast_ident_cval(a, cnode, &ct, &cv))
		return 0;
	C = (int64_t)cv;
	if (C >= -1 && C <= 1)
		return 0;
	ac = (uint64_t)(C < 0 ? -C : C);
	if ((ac & (ac - 1)) == 0) /* power of two: pos handled by spow2, neg left as idiv */
		return 0;
	if (!ast_ident_etype(a, x, &xt, &xref) || (xt & (VT_BTYPE | VT_UNSIGNED)) != VT_INT)
		return 0;
	if (!ast_ident_pure(a, x))
		return 0;
	mag = mcc_magics((int32_t)C);
	/* q0 = (int32)((int64)M * (int64)x >> 32) */
	Mi = ast_bf_lit(a, S64, (uint64_t)(int64_t)mag.M);
	xi = ast_bf_ucast(a, S64, x); /* Convert to i64, dups x (sign-extends: signed dest) */
	prod = ast_bf_bin(a, '*', S64, Mi, xi);
	hi = ast_bf_bin(a, TOK_SAR, S64, prod, ast_bf_lit(a, S64, 32));
	q0 = ast_node(a, AST_Convert);
	ast_set_type(a, q0, S32, 0);
	ast_add_child(a, q0, hi);
	if (C > 0 && mag.M < 0)
		q1 = ast_bf_bin(a, '+', S32, q0, ast_dup_sub(a, x));
	else if (C < 0 && mag.M > 0)
		q1 = ast_bf_bin(a, '-', S32, q0, ast_dup_sub(a, x));
	else
		q1 = q0;
	q2 = ast_bf_bin(a, TOK_SAR, S32, q1, ast_bf_lit(a, S32, (uint64_t)mag.s));
	/* signbit = (uint32)q2 >> 31 (logical) */
	cvt = ast_node(a, AST_Convert);
	ast_set_type(a, cvt, U32, 0);
	ast_add_child(a, cvt, ast_dup_sub(a, q2));
	signbit = ast_bf_bin(a, TOK_SHR, U32, cvt, ast_bf_lit(a, U32, 31));
	MCC_TRACE("divmagic signed %s C=%lld M=0x%x s=%d\n", op == '/' ? "div" : "rem",
						(long long)C, (unsigned)mag.M, mag.s);
	ast_set_type(a, n, S32, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	if (op == '/') { /* n := q2 + signbit (the quotient) */
		ast_set_op(a, n, '+');
		ast_add_child(a, n, q2);
		ast_add_child(a, n, signbit);
	} else { /* n := x - (x/C)*C */
		AstLocal qexpr = ast_bf_bin(a, '+', S32, q2, signbit);
		AstLocal qC = ast_bf_bin(a, '*', S32, qexpr, ast_bf_lit(a, S32, (uint64_t)(uint32_t)C));
		ast_set_op(a, n, '-');
		ast_add_child(a, n, ast_dup_sub(a, x));
		ast_add_child(a, n, qC);
	}
	return 1;
}

static int ast_divmagic_run(AstArena *a) {
	AstLocal nn = ast_count(a);
	ast_divmagic_folds = 0;
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_Binary && (ast_op(a, n) == '/' || ast_op(a, n) == '%') &&
				ast_nchild(a, n) == 2) {
			int f = ast_divmagic_try(a, n); /* unsigned magic */
			if (!f)
				f = ast_divmagic_try_spow2(a, n); /* signed power-of-two */
			if (!f)
				f = ast_divmagic_try_signed(a, n); /* signed non-power-of-two division */
			ast_divmagic_folds += f;
		}
	return ast_divmagic_folds;
}

/* Branchless abs/-abs: mcc lowers `x < 0 ? -x : x` to a compare + branch; the identity
 * `abs(x) = (x ^ (x>>31)) - (x>>31)` (arithmetic shift) is branchless and target-independent,
 * duplicating only the pure x (cheap, no temp/cmov). A strict win over the branch. */
static int ast_abs_folds;

/* `0 - k` -> k, else AST_NONE. Unary minus is lowered to `0 - x` (mccgen `vpushi(0);gen_op('-')`). */
static AstLocal ast_abs_neg_of(AstArena *a, AstLocal n) {
	AstLocal l;
	int ct;
	uint64_t cv;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != '-' || ast_nchild(a, n) != 2)
		return AST_NONE;
	l = ast_child(a, n, 0);
	if (ast_kind(a, l) != AST_Literal || !ast_ident_cval(a, l, &ct, &cv) || cv != 0)
		return AST_NONE;
	return ast_child(a, n, 1);
}

/* Is n the integer literal 0? (for clamp-to-zero max(x,0)/min(x,0) recognition) */
static int ast_abs_is_zero(AstArena *a, AstLocal n) {
	int ct;
	uint64_t cv;
	return ast_kind(a, n) == AST_Literal && ast_ident_cval(a, n, &ct, &cv) && cv == 0;
}

/* `x REL 0` -> key + rel normalized onto x (LT/LE/GT/GE); literal must be exactly 0. */
static int ast_abs_cmp_zero(AstArena *a, AstLocal n, AstLocal *key, int *rel) {
	int op = ast_op(a, n), ct, keyleft;
	AstLocal l, r, k, c;
	uint64_t cv;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		return 0;
	if (op != TOK_LT && op != TOK_LE && op != TOK_GT && op != TOK_GE)
		return 0;
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal) {
		k = l;
		c = r;
		keyleft = 1;
	} else if (ast_kind(a, l) == AST_Literal) {
		k = r;
		c = l;
		keyleft = 0;
	} else {
		return 0;
	}
	if (!ast_ident_cval(a, c, &ct, &cv) || cv != 0)
		return 0;
	*rel = keyleft ? op
								 : op == TOK_LT ? TOK_GT : op == TOK_GT ? TOK_LT : op == TOK_LE ? TOK_GE : TOK_LE;
	*key = k;
	return 1;
}

/* `S(x) = x >> (width-1)` (arithmetic sign mask: 0 if x>=0, -1 if x<0). */
static AstLocal ast_abs_signmask(AstArena *a, AstLocal key, int ty, int sh) {
	return ast_bf_bin(a, TOK_SAR, ty, ast_dup_sub(a, key), ast_bf_lit(a, ty, (uint64_t)sh));
}

/* Recognize a signed `x REL 0 ? A : B` ternary and lower it branchlessly (32- or 64-bit):
 *  abs(x)      = (x^s) - s     -abs(x)     = s - (x^s)          (s = x>>(w-1))
 *  max(x,0)    = x - (x & s)   min(x,0)    = x & s
 * where {A,B} is {x,-x} (abs) or {x,0} (clamp). Only pure signed x is duplicated. */
static int ast_abs_try(AstArena *a, AstLocal n) {
	AstLocal cond, tval, fval, key, negval, posval;
	int rel, kt, neg_is_t, mode, ty, sh; /* mode: 0 abs, 1 -abs, 2 max(x,0), 3 min(x,0) */
	uint64_t kref;
	if (ast_kind(a, n) != AST_If || ast_op(a, n) != 5 || ast_nchild(a, n) != 3)
		return 0;
	cond = ast_child(a, n, 0);
	tval = ast_child(a, n, 1);
	fval = ast_child(a, n, 2);
	if (!ast_abs_cmp_zero(a, cond, &key, &rel))
		return 0;
	if (!ast_ident_etype(a, key, &kt, &kref) || (kt & VT_UNSIGNED))
		return 0; /* signed integer key */
	if ((kt & VT_BTYPE) == VT_INT) {
		ty = VT_INT;
		sh = 31;
	} else if ((kt & VT_BTYPE) == VT_LLONG) {
		ty = VT_LLONG;
		sh = 63;
	} else {
		return 0; /* 32- or 64-bit signed only */
	}
	if (!ast_ident_pure(a, key))
		return 0;
	/* which value is chosen when x < 0? */
	neg_is_t = (rel == TOK_LT || rel == TOK_LE);
	negval = neg_is_t ? tval : fval; /* value for x<0 */
	posval = neg_is_t ? fval : tval; /* value for x>=0 */
	{
		AstLocal nn = ast_abs_neg_of(a, negval), pn = ast_abs_neg_of(a, posval);
		if (nn != AST_NONE && ast_ident_same(a, nn, key) && ast_ident_same(a, posval, key))
			mode = 0; /* x<0 -> -x, x>=0 -> x  ==> abs */
		else if (pn != AST_NONE && ast_ident_same(a, pn, key) && ast_ident_same(a, negval, key))
			mode = 1; /* x<0 -> x, x>=0 -> -x  ==> -abs */
		else if (ast_abs_is_zero(a, negval) && ast_ident_same(a, posval, key))
			mode = 2; /* x<0 -> 0, x>=0 -> x  ==> max(x,0) */
		else if (ast_abs_is_zero(a, posval) && ast_ident_same(a, negval, key))
			mode = 3; /* x<0 -> x, x>=0 -> 0  ==> min(x,0) */
		else
			return 0;
	}
	MCC_TRACE("abs/clamp fold key=%u mode=%d ty=0x%x\n", (unsigned)key, mode, ty);
	ast_set_type(a, n, ty, 0);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_set_kind(a, n, AST_Binary); /* was AST_If (ternary) */
	ast_clear_children(a, n);
	if (mode == 0) { /* abs = (x^s) - s */
		ast_set_op(a, n, '-');
		ast_add_child(a, n,
									ast_bf_bin(a, '^', ty, ast_dup_sub(a, key), ast_abs_signmask(a, key, ty, sh)));
		ast_add_child(a, n, ast_abs_signmask(a, key, ty, sh));
	} else if (mode == 1) { /* -abs = s - (x^s) */
		ast_set_op(a, n, '-');
		ast_add_child(a, n, ast_abs_signmask(a, key, ty, sh));
		ast_add_child(a, n,
									ast_bf_bin(a, '^', ty, ast_dup_sub(a, key), ast_abs_signmask(a, key, ty, sh)));
	} else if (mode == 2) { /* max(x,0) = x - (x & s) */
		ast_set_op(a, n, '-');
		ast_add_child(a, n, ast_dup_sub(a, key));
		ast_add_child(a, n,
									ast_bf_bin(a, '&', ty, ast_dup_sub(a, key), ast_abs_signmask(a, key, ty, sh)));
	} else { /* min(x,0) = x & s */
		ast_set_op(a, n, '&');
		ast_add_child(a, n, ast_dup_sub(a, key));
		ast_add_child(a, n, ast_abs_signmask(a, key, ty, sh));
	}
	return 1;
}

static int ast_abs_run(AstArena *a) {
	AstLocal nn = ast_count(a);
	ast_abs_folds = 0;
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_If && ast_op(a, n) == 5 && ast_nchild(a, n) == 3) {
			ast_abs_folds += ast_abs_try(a, n);
		}
	return ast_abs_folds;
}

/* Constant reassociation: `(x OP c1) OP c2` -> `x OP combine(c1,c2)` for a same-op nest with
 * two integer-literal constants. mcc otherwise emits both ops (e.g. `shr;shr`, `and;and`).
 * Correct-by-construction: &/|/^ combine exactly; +/* are modular (machine-equivalent); shifts
 * combine only when the summed count stays below the type width (else the double shift ≠ a
 * single one). x is duplicated so it must be pure. */
static int ast_reassoc_folds;

static int ast_reassoc_try(AstArena *a, AstLocal n) {
	int op = ast_op(a, n), c1t, c2t, xt, nt, iop, additive, result_op;
	AstLocal inner, c2n, x, c1n;
	uint64_t c1v, c2v, combined, xref, nref, width;
	if (op != '&' && op != '|' && op != '^' && op != '+' && op != '-' && op != '*' &&
			op != TOK_SHL && op != TOK_SHR && op != TOK_SAR)
		return 0;
	if (ast_nchild(a, n) != 2)
		return 0;
	/* Find the outer constant (c2) and the inner subtree. For a commutative op the constant
	 * may be on either side (`3 + (x+5)`); for `-`/shifts it must be the right operand. */
	{
		AstLocal ch0 = ast_child(a, n, 0), ch1 = ast_child(a, n, 1);
		int comm = (op == '+' || op == '*' || op == '&' || op == '|' || op == '^');
		if (ast_kind(a, ch1) == AST_Literal && ast_ident_cval(a, ch1, &c2t, &c2v))
			inner = ch0;
		else if (comm && ast_kind(a, ch0) == AST_Literal && ast_ident_cval(a, ch0, &c2t, &c2v))
			inner = ch1;
		else
			return 0;
	}
	(void)c2n;
	if (ast_kind(a, inner) != AST_Binary || ast_nchild(a, inner) != 2)
		return 0;
	/* +/- are one additive group: allow mixing (`(x+c1)-c2`); other ops need a same-op nest. */
	iop = ast_op(a, inner);
	additive = (op == '+' || op == '-');
	if (additive ? (iop != '+' && iop != '-') : (iop != op))
		return 0;
	/* extract the inner's constant (c1) + the variable x, const on either side if commutative. */
	{
		AstLocal ich0 = ast_child(a, inner, 0), ich1 = ast_child(a, inner, 1);
		int icomm = (iop == '+' || iop == '*' || iop == '&' || iop == '|' || iop == '^');
		if (ast_kind(a, ich1) == AST_Literal && ast_ident_cval(a, ich1, &c1t, &c1v))
			x = ich0;
		else if (icomm && ast_kind(a, ich0) == AST_Literal && ast_ident_cval(a, ich0, &c1t, &c1v))
			x = ich1;
		else
			return 0;
	}
	(void)c1n;
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_intt(nt))
		return 0;
	if (!ast_ident_etype(a, x, &xt, &xref) || !ast_ident_pure(a, x))
		return 0;
	width = (nt & VT_BTYPE) == VT_LLONG ? 64 : 32;
	result_op = op;
	if (additive) { /* net offset; always emit `x + net` (backend turns +neg into sub) */
		combined = (iop == '+' ? c1v : (uint64_t)(0 - c1v)) +
							 (op == '+' ? c2v : (uint64_t)(0 - c2v));
		result_op = '+';
	} else
		switch (op) {
		case '&': combined = c1v & c2v; break;
		case '|': combined = c1v | c2v; break;
		case '^': combined = c1v ^ c2v; break;
		case '*': combined = c1v * c2v; break;
		default: /* shifts: only when the combined count stays in range */
			if (c1v >= width || c2v >= width || c1v + c2v >= width)
				return 0;
			combined = c1v + c2v;
			break;
		}
	MCC_TRACE("reassoc op=%d iop=%d c1=%llu c2=%llu -> op=%d %llu\n", op, iop,
						(unsigned long long)c1v, (unsigned long long)c2v, result_op,
						(unsigned long long)combined);
	ast_set_op(a, n, result_op);
	ast_set_type(a, n, nt, nref);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, ast_dup_sub(a, x));
	ast_add_child(a, n, ast_bf_lit(a, nt, combined));
	return 1;
}

/* `(x << c) >> c` for UNSIGNED (logical shifts) clears the top c bits -> `x & (~0 >> c)`.
 * mcc otherwise emits two shifts. Only unsigned: for signed the arithmetic `>>` sign-extends
 * (not a mask). Same shift count, c in (0,width). x duplicated so must be pure. */
static int ast_reassoc_shlshr(AstArena *a, AstLocal n) {
	int nt, ct, ict, xt;
	AstLocal inner, c2n, x, c1n;
	uint64_t c1v, c2v, nref, xref, width, mask;
	/* the parser emits TOK_SAR ('>') for `>>`; logical-vs-arithmetic is the operand TYPE, so
	 * the VT_UNSIGNED check below is what makes this a logical (mask-equivalent) shift. */
	if (ast_op(a, n) != TOK_SAR || ast_nchild(a, n) != 2)
		return 0;
	inner = ast_child(a, n, 0);
	c2n = ast_child(a, n, 1);
	if (ast_kind(a, c2n) != AST_Literal || !ast_ident_cval(a, c2n, &ct, &c2v))
		return 0;
	if (ast_kind(a, inner) != AST_Binary || ast_op(a, inner) != TOK_SHL ||
			ast_nchild(a, inner) != 2)
		return 0;
	x = ast_child(a, inner, 0);
	c1n = ast_child(a, inner, 1);
	if (ast_kind(a, c1n) != AST_Literal || !ast_ident_cval(a, c1n, &ict, &c1v) || c1v != c2v)
		return 0;
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_intt(nt) || !(nt & VT_UNSIGNED))
		return 0; /* unsigned only */
	width = (nt & VT_BTYPE) == VT_LLONG ? 64 : 32;
	if (c1v == 0 || c1v >= width)
		return 0;
	if (!ast_ident_etype(a, x, &xt, &xref) || !ast_ident_pure(a, x))
		return 0;
	mask = width == 64 ? (~0ULL >> c1v) : (uint64_t)(0xFFFFFFFFu >> c1v);
	MCC_TRACE("reassoc shlshr c=%llu -> & 0x%llx\n", (unsigned long long)c1v,
						(unsigned long long)mask);
	ast_set_op(a, n, '&');
	ast_set_type(a, n, nt, nref);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, ast_dup_sub(a, x));
	ast_add_child(a, n, ast_bf_lit(a, nt, mask));
	return 1;
}

/* `(x >> c) << c` clears the LOW c bits -> `x & ~((1<<c)-1)` (align-down). Works for BOTH
 * signed and unsigned: clearing low bits is sign-independent (arith `>>` then `<<` still just
 * zeros the low c bits). Same shift count, c in (0,width). x duplicated so must be pure. */
static int ast_reassoc_shrshl(AstArena *a, AstLocal n) {
	int nt, ct, ict, xt;
	AstLocal inner, c2n, x, c1n;
	uint64_t c1v, c2v, nref, xref, width, mask;
	if (ast_op(a, n) != TOK_SHL || ast_nchild(a, n) != 2) /* outer `<<` */
		return 0;
	inner = ast_child(a, n, 0);
	c2n = ast_child(a, n, 1);
	if (ast_kind(a, c2n) != AST_Literal || !ast_ident_cval(a, c2n, &ct, &c2v))
		return 0;
	if (ast_kind(a, inner) != AST_Binary || ast_op(a, inner) != TOK_SAR || /* inner `>>` */
			ast_nchild(a, inner) != 2)
		return 0;
	x = ast_child(a, inner, 0);
	c1n = ast_child(a, inner, 1);
	if (ast_kind(a, c1n) != AST_Literal || !ast_ident_cval(a, c1n, &ict, &c1v) || c1v != c2v)
		return 0;
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_intt(nt))
		return 0; /* signed OR unsigned — clearing low bits is sign-independent */
	width = (nt & VT_BTYPE) == VT_LLONG ? 64 : 32;
	if (c1v == 0 || c1v >= width)
		return 0;
	if (!ast_ident_etype(a, x, &xt, &xref) || !ast_ident_pure(a, x))
		return 0;
	mask = width == 64 ? (~0ULL << c1v) : (uint64_t)(0xFFFFFFFFu << c1v);
	MCC_TRACE("reassoc shrshl c=%llu -> & 0x%llx\n", (unsigned long long)c1v,
						(unsigned long long)mask);
	ast_set_op(a, n, '&');
	ast_set_type(a, n, nt, nref);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, ast_dup_sub(a, x));
	ast_add_child(a, n, ast_bf_lit(a, nt, mask));
	return 1;
}

/* Extract (x, const) from a commutative `x * const` node. */
static int ast_reassoc_mulconst(AstArena *a, AstLocal n, AstLocal *x, uint64_t *cv) {
	AstLocal l, r;
	int ct;
	if (ast_kind(a, n) != AST_Binary || ast_op(a, n) != '*' || ast_nchild(a, n) != 2)
		return 0;
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	if (ast_kind(a, r) == AST_Literal && ast_ident_cval(a, r, &ct, cv)) {
		*x = l;
		return 1;
	}
	if (ast_kind(a, l) == AST_Literal && ast_ident_cval(a, l, &ct, cv)) {
		*x = r;
		return 1;
	}
	return 0;
}

/* Multiply distribution: `(x*C1) + (x*C2)` -> `x*(C1+C2)`, `(x*C1) - (x*C2)` -> `x*(C1-C2)`
 * (same x, both constants). mcc otherwise emits two multiplies; the combined constant multiply
 * is one imul (or a shl when the product is a power of two). Distributive, exact modulo 2^w.
 * x is duplicated so must be pure. combined 0/1 collapse to 0 / x. */
static int ast_reassoc_muldist(AstArena *a, AstLocal n) {
	int op = ast_op(a, n), nt, xt;
	AstLocal l, r, x1, x2;
	uint64_t c1, c2, combined, nref, xref;
	if ((op != '+' && op != '-') || ast_nchild(a, n) != 2)
		return 0;
	l = ast_child(a, n, 0);
	r = ast_child(a, n, 1);
	{
		/* Each operand is `x*const`, or a bare `x` (treated as x*1). At least one must be a
		 * multiply — else it's `x+x`/`x-x`, better left to the backend. */
		int got_l = ast_reassoc_mulconst(a, l, &x1, &c1);
		int got_r = ast_reassoc_mulconst(a, r, &x2, &c2);
		if (!got_l && !got_r)
			return 0;
		if (!got_l) {
			x1 = l;
			c1 = 1;
		}
		if (!got_r) {
			x2 = r;
			c2 = 1;
		}
	}
	if (!ast_ident_same(a, x1, x2))
		return 0;
	if (!ast_ident_etype(a, n, &nt, &nref) || !ast_ident_intt(nt))
		return 0;
	if (!ast_ident_etype(a, x1, &xt, &xref) || !ast_ident_pure(a, x1))
		return 0;
	combined = op == '+' ? c1 + c2 : c1 - c2;
	if (combined == 0 || combined == 1) /* x*0=0, x*1=x — rare degenerate; leave to keep it simple */
		return 0;
	MCC_TRACE("reassoc muldist op=%d c1=%llu c2=%llu -> x*%llu\n", op, (unsigned long long)c1,
						(unsigned long long)c2, (unsigned long long)combined);
	ast_set_op(a, n, '*');
	ast_set_type(a, n, nt, nref);
	ast_set_ival(a, n, 0);
	ast_set_fbits(a, n, 0);
	ast_set_sym(a, n, 0);
	ast_set_cst(a, n, 0);
	ast_clear_children(a, n);
	ast_add_child(a, n, ast_dup_sub(a, x1));
	ast_add_child(a, n, ast_bf_lit(a, nt, combined));
	return 1;
}

static int ast_reassoc_run(AstArena *a) {
	AstLocal nn = ast_count(a);
	ast_reassoc_folds = 0;
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_Binary && ast_nchild(a, n) == 2) {
			int f = ast_reassoc_try(a, n);
			if (!f)
				f = ast_reassoc_shlshr(a, n);
			if (!f)
				f = ast_reassoc_shrshl(a, n);
			if (!f)
				f = ast_reassoc_muldist(a, n);
			ast_reassoc_folds += f;
		}
	return ast_reassoc_folds;
}

static int ast_sethi_folds;

static int ast_sethi_commutative(int op) {
	return op == '+' || op == '*' || op == '&' || op == '|' || op == '^';
}

static int ast_sethi_cmp_root(AstArena *a, AstLocal n) {
	if (n == AST_NONE || ast_kind(a, n) != AST_Binary)
		return 0;
	switch (ast_op(a, n)) {
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

static int ast_sethi_num(AstArena *a, AstLocal n) {
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2) {
		/* V-sethi(a): a literal leaf is an immediate operand (0 registers); other
		 * leaves (a variable ref / load) need 1. Off by default -> every leaf counts
		 * 1 (byte-identical); the AST_SG_SETHILEAF search knob turns on the refinement
		 * so the higher-register-need subtree, not just the deeper one, sorts first. */
		if (ast_sethi_leaf_env && ast_kind(a, n) == AST_Literal)
			return 0;
		return 1;
	}
	int l = ast_sethi_num(a, ast_child(a, n, 0));
	int r = ast_sethi_num(a, ast_child(a, n, 1));
	return l == r ? l + 1 : (l > r ? l : r);
}

static int ast_sethi_run(AstArena *a) {
	ast_sethi_folds = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Binary || !ast_sethi_commutative(ast_op(a, n)))
			continue;
		if (ast_nchild(a, n) != 2)
			continue;
		AstLocal c0 = ast_child(a, n, 0), c1 = ast_child(a, n, 1);
		if (!ast_cse_regpure(a, c0) || !ast_cse_regpure(a, c1))
			continue;
		if (ast_sethi_cmp_root(a, c0) || ast_sethi_cmp_root(a, c1))
			continue;
		if (ast_sethi_num(a, c1) <= ast_sethi_num(a, c0))
			continue;
		ast_clear_children(a, n);
		ast_add_child(a, n, c1);
		ast_add_child(a, n, c0);
		ast_sethi_folds++;
	}
	return ast_sethi_folds;
}

static void ast_licm_at_loop(AstArena *a, AstLocal s) {
	if (ast_sccp_has_label(a, s))
		return;
	for (int i = 0; i < ast_cse_n; i++) {
		AstLocal e = ast_cse_expr[i], ref = ast_cse_ref[i];
		int foff = ast_cse_off[i];
		if (ast_cprop_escapes(a, foff) || ast_licm_written(a, s, foff))
			continue;
		if (!ast_licm_operands_ok(a, s, e))
			continue;
		ast_licm_subst(a, s, e, ref, 0);
	}
}

static int ast_ltemp_binop_ok(int op) {
	switch (op) {
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
																int *cnt) {
	if (n == AST_NONE)
		return;
	uint16_t k = ast_kind(a, n);
	if (!lval && n != e && ast_ident_same(a, e, n)) {
		(*cnt)++;
		return;
	}
	if (k == AST_Store) {
		ast_ltemp_count_occ(a, ast_child(a, n, 0), e, 1, cnt);
		ast_ltemp_count_occ(a, ast_child(a, n, 1), e, 0, cnt);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_ltemp_count_occ(a, c, e, clval, cnt);
}

static AstLocal ast_ltemp_cand;

static void ast_ltemp_scan(AstArena *a, AstLocal loop, AstLocal n, int lval) {
	if (n == AST_NONE || ast_ltemp_cand != AST_NONE)
		return;
	uint16_t k = ast_kind(a, n);
	if (!lval && k == AST_Binary && ast_ltemp_binop_ok(ast_op(a, n)) &&
			ast_cse_regpure(a, n) && ast_licm_operands_ok(a, loop, n)) {
		int et;
		uint64_t er;
		if (ast_ident_etype(a, n, &et, &er) && ast_ident_intt(et)) {
			int cnt = 0;
			ast_ltemp_count_occ(a, loop, n, 0, &cnt);
			if (cnt >= 1) {
				ast_ltemp_cand = n;
				return;
			}
		}
	}
	if (k == AST_Store) {
		ast_ltemp_scan(a, loop, ast_child(a, n, 0), 1);
		ast_ltemp_scan(a, loop, ast_child(a, n, 1), 0);
		return;
	}
	int clval = k == AST_Unary && ast_cprop_lval_op(ast_op(a, n));
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_ltemp_scan(a, loop, c, clval);
}

static int ast_ltemp_insert_before(AstArena *a, AstLocal parent, AstLocal pivot,
																	 AstLocal node) {
	a->epoch++;
	if (a->first_child[parent] == pivot) {
		a->parent[node] = parent;
		a->next_sib[node] = pivot;
		a->first_child[parent] = node;
		a->nchild[parent]++;
		return 1;
	}
	for (AstLocal c = a->first_child[parent]; c != AST_NONE; c = a->next_sib[c])
		if (a->next_sib[c] == pivot) {
			a->parent[node] = parent;
			a->next_sib[node] = pivot;
			a->next_sib[c] = node;
			a->nchild[parent]++;
			return 1;
		}
	return 0;
}

static int ast_ltemp_materialize(AstArena *a, AstLocal loop, AstLocal e) {
	int et;
	uint64_t er;
	if (ast_ltemp_n >= AST_LTEMP_MAX)
		return 0;
	if (!ast_ident_etype(a, e, &et, &er) || !ast_ident_intt(et))
		return 0;
	AstLocal parent = ast_parent(a, loop);
	if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
		return 0;
	int off = (ast_ltemp_cur - 8) & -8;
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
		return 0;
	AstLocal tref = ast_node(a, AST_Ref);
	ast_set_op(a, tref, VT_LOCAL | VT_LVAL);
	ast_set_ival(a, tref, (uint64_t)off);
	ast_set_type(a, tref, et, er);
	ast_licm_subst(a, loop, e, tref, 0);
	ast_cse_setref(a, e, tref);
	ast_licm_folds++;
	ast_ltemp_cur = off;
	ast_ltemp_off[ast_ltemp_n++] = off;
	return 1;
}

static int ast_ltemp_run(AstArena *a) {
	int did = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_If)
			continue;
		int op = ast_op(a, n);
		if (op < 2 || op > 4)
			continue;
		if (ast_sccp_has_label(a, n))
			continue;
		AstLocal parent = ast_parent(a, n);
		if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
			continue;
		for (int per = 0; per < AST_LTEMP_PER_LOOP && ast_ltemp_n < AST_LTEMP_MAX; per++) {
			ast_ltemp_cand = AST_NONE;
			ast_ltemp_scan(a, n, n, 0);
			if (ast_ltemp_cand == AST_NONE)
				break;
			if (!ast_ltemp_materialize(a, n, ast_ltemp_cand))
				break;
			did++;
		}
	}
	return did;
}

static int ast_ivsr_width(int tt) {
	switch (tt & VT_BTYPE) {
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
														int64_t *stride) {
	uint16_t k = ast_kind(a, s);
	if (k == AST_Unary && (ast_op(a, s) == TOK_INC || ast_op(a, s) == TOK_DEC)) {
		AstLocal r = ast_first_child(a, s);
		int off, tt;
		if (r != AST_NONE && ast_cprop_is_local(a, r, &off, &tt) &&
				ast_ident_intt(tt)) {
			*ivoff = off;
			*ivtt = tt;
			*stride = ast_op(a, s) == TOK_INC ? 1 : -1;
			return 1;
		}
		return 0;
	}
	if (k == AST_Store) {
		AstLocal lval = ast_child(a, s, 0), rhs = ast_child(a, s, 1);
		int off, tt;
		if (!ast_cprop_is_local(a, lval, &off, &tt) || !ast_ident_intt(tt))
			return 0;
		if (rhs == AST_NONE || ast_kind(a, rhs) != AST_Binary ||
				ast_nchild(a, rhs) != 2)
			return 0;
		int bop = ast_op(a, rhs);
		if (bop != '+' && bop != '-')
			return 0;
		AstLocal x = ast_child(a, rhs, 0), y = ast_child(a, rhs, 1);
		if (ast_ref_is_local_off(a, x, off) && ast_kind(a, y) == AST_Literal) {
			int64_t kk = (int64_t)ast_ival(a, y);
			*ivoff = off;
			*ivtt = tt;
			*stride = bop == '+' ? kk : -kk;
			return 1;
		}
		if (bop == '+' && ast_ref_is_local_off(a, y, off) &&
				ast_kind(a, x) == AST_Literal) {
			*ivoff = off;
			*ivtt = tt;
			*stride = (int64_t)ast_ival(a, x);
			return 1;
		}
	}
	return 0;
}

static int ast_ivsr_count_writes(AstArena *a, AstLocal n, int off) {
	if (n == AST_NONE)
		return 0;
	uint16_t k = ast_kind(a, n);
	int cnt = 0;
	if (k == AST_Store && ast_ref_is_local_off(a, ast_child(a, n, 0), off))
		cnt++;
	if (k == AST_Unary && ast_ref_is_local_off(a, ast_first_child(a, n), off))
		cnt++;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		cnt += ast_ivsr_count_writes(a, c, off);
	return cnt;
}

static AstLocal ast_ivsr_target;

static AstLocal ast_ivsr_cofactor(AstArena *a, AstLocal loop, AstLocal mul,
																	int ivoff, int ivtt) {
	int et;
	uint64_t er;
	if (ast_kind(a, mul) != AST_Binary || ast_op(a, mul) != '*' ||
			ast_nchild(a, mul) != 2 || !ast_cse_regpure(a, mul))
		return AST_NONE;
	if (!ast_ident_etype(a, mul, &et, &er) || !ast_ident_intt(et))
		return AST_NONE;
	if (!ast_ivsr_width(et) || ast_ivsr_width(et) != ast_ivsr_width(ivtt))
		return AST_NONE;
	AstLocal x = ast_child(a, mul, 0), y = ast_child(a, mul, 1), c;
	if (ast_ref_is_local_off(a, x, ivoff))
		c = y;
	else if (ast_ref_is_local_off(a, y, ivoff))
		c = x;
	else
		return AST_NONE;
	if (!ast_cse_regpure(a, c) || !ast_licm_operands_ok(a, loop, c))
		return AST_NONE;
	return c;
}

static void ast_ivsr_scan(AstArena *a, AstLocal loop, AstLocal n, int ivoff,
													int ivtt) {
	if (n == AST_NONE || ast_ivsr_target != AST_NONE)
		return;
	if (ast_ivsr_cofactor(a, loop, n, ivoff, ivtt) != AST_NONE) {
		ast_ivsr_target = n;
		return;
	}
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		ast_ivsr_scan(a, loop, c, ivoff, ivtt);
}

static int ast_ivsr_run(AstArena *a) {
	int did = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_If)
			continue;
		int op = ast_op(a, n);
		if (op != 3 && op != 5)
			continue;
		if (ast_sccp_has_label(a, n))
			continue;
		AstLocal parent = ast_parent(a, n);
		if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
			continue;
		AstLocal incrbb = ast_child(a, n, op == 3 ? 1 : 0);
		AstLocal body = ast_child(a, n, op == 3 ? 2 : 1);
		if (incrbb == AST_NONE || body == AST_NONE)
			continue;
		if (ast_kind(a, incrbb) != AST_BasicBlock ||
				ast_kind(a, body) != AST_BasicBlock)
			continue;
		int ivoff = 0, ivtt = 0, found = 0;
		int64_t stride = 0;
		for (AstLocal s = ast_first_child(a, incrbb); s != AST_NONE;
				 s = ast_next_sib(a, s))
			if (ast_ivsr_incr_of(a, s, &ivoff, &ivtt, &stride)) {
				found = 1;
				break;
			}
		if (!found)
			continue;
		if (ast_cprop_escapes(a, ivoff))
			continue;
		if (ast_ivsr_count_writes(a, n, ivoff) != 1)
			continue;
		ast_ivsr_target = AST_NONE;
		ast_ivsr_scan(a, n, body, ivoff, ivtt);
		if (ast_ivsr_target == AST_NONE)
			continue;
		if (ast_ltemp_n >= AST_LTEMP_MAX)
			break;
		AstLocal mul = ast_ivsr_target;
		AstLocal c = ast_ivsr_cofactor(a, n, mul, ivoff, ivtt);
		if (c == AST_NONE)
			continue;
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
			continue;
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
		ast_ltemp_off[ast_ltemp_n++] = off;
		did++;
	}
	return did;
}

static int ast_pre_arm_store(AstArena *a, AstLocal bb, AstLocal *store) {
	if (ast_kind(a, bb) != AST_BasicBlock)
		return 0;
	AstLocal last = ast_last_child(a, bb);
	if (last == AST_NONE || ast_kind(a, last) != AST_Store)
		return 0;
	*store = last;
	return 1;
}

static AstLocal ast_pre_binary_of(AstArena *a, AstLocal store) {
	AstLocal e = ast_child(a, store, 1);
	while (e != AST_NONE && ast_kind(a, e) == AST_Convert && ast_nchild(a, e) == 1)
		e = ast_first_child(a, e);
	if (e == AST_NONE || ast_kind(a, e) != AST_Binary)
		return AST_NONE;
	return e;
}

static int ast_pre_occurs(AstArena *a, AstLocal n, AstLocal e) {
	if (n == AST_NONE)
		return 0;
	if (n != e && ast_ident_same(a, e, n))
		return 1;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (ast_pre_occurs(a, c, e))
			return 1;
	return 0;
}

static int ast_pre_run(AstArena *a) {
	int did = 0;
	AstLocal nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++) {
		if (ast_ltemp_n >= AST_LTEMP_MAX)
			break;
		if (ast_kind(a, n) != AST_If || ast_op(a, n) != 0)
			continue;
		if (ast_nchild(a, n) < 3)
			continue;
		if (ast_sccp_has_label(a, n))
			continue;
		AstLocal parent = ast_parent(a, n);
		if (parent == AST_NONE || ast_kind(a, parent) != AST_BasicBlock)
			continue;
		AstLocal thenbb = ast_child(a, n, 1), elsebb = ast_child(a, n, 2);
		if (ast_kind(a, thenbb) != AST_BasicBlock ||
				ast_kind(a, elsebb) != AST_BasicBlock)
			continue;
		AstLocal ts, es, e = AST_NONE;
		if (ast_pre_arm_store(a, thenbb, &ts))
			e = ast_pre_binary_of(a, ts);
		if (e == AST_NONE && ast_pre_arm_store(a, elsebb, &es))
			e = ast_pre_binary_of(a, es);
		if (e == AST_NONE)
			continue;
		if (!ast_cse_regpure(a, e))
			continue;
		int et;
		uint64_t er;
		if (!ast_ident_etype(a, e, &et, &er) || !ast_ident_intt(et))
			continue;
		if (!ast_licm_operands_ok(a, n, e))
			continue;
		AstLocal post = AST_NONE, prhs = AST_NONE;
		for (AstLocal s = ast_next_sib(a, n); s != AST_NONE; s = ast_next_sib(a, s)) {
			if (ast_sccp_has_label(a, s))
				break;
			if (ast_kind(a, s) == AST_Store) {
				AstLocal rhs = ast_child(a, s, 1);
				if (ast_pre_occurs(a, rhs, e) && ast_licm_operands_ok(a, s, e)) {
					post = s;
					prhs = rhs;
					break;
				}
			}
			if (!ast_licm_operands_ok(a, s, e))
				break;
		}
		if (post == AST_NONE)
			continue;
		int off = (ast_ltemp_cur - 8) & -8;
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
			continue;
		AstLocal tref = ast_node(a, AST_Ref);
		ast_set_op(a, tref, VT_LOCAL | VT_LVAL);
		ast_set_ival(a, tref, (uint64_t)off);
		ast_set_type(a, tref, et, er);
		ast_licm_subst(a, thenbb, e, tref, 0);
		ast_licm_subst(a, elsebb, e, tref, 0);
		ast_licm_subst(a, prhs, e, tref, 0);
		ast_cse_setref(a, e, tref);
		ast_licm_folds++;
		ast_ltemp_cur = off;
		ast_ltemp_off[ast_ltemp_n++] = off;
		did++;
	}
	return did;
}

static void ast_cse_block(AstArena *a, AstLocal bb) {
	ast_cse_n = 0;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) {
		uint16_t k = ast_kind(a, s);
		if (k == AST_Store) {
			AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
			ast_cse_subst(a, val, 0);
			ast_cse_subst(a, lval, 1);
			if (!ast_cprop_safe(a, lval) || !ast_cprop_safe(a, val)) {
				ast_cse_n = 0;
				continue;
			}
			int off, tt;
			if (ast_cprop_is_local(a, lval, &off, &tt)) {
				ast_cse_kill(a, off);
				int et;
				uint64_t er;
				if (!ast_cprop_escapes(a, off) && ast_cse_n < ast_cse_window &&
						ast_cse_regpure(a, val) && !ast_ident_leaf(a, val) &&
						!ast_tco_reads_off(a, val, off) &&
						ast_ident_etype(a, val, &et, &er) && ast_ident_intt(et) &&
						(et & (VT_BTYPE | VT_UNSIGNED)) == (tt & (VT_BTYPE | VT_UNSIGNED)) &&
						er == ast_type_ref(a, lval)) {
					ast_cse_expr[ast_cse_n] = val;
					ast_cse_ref[ast_cse_n] = lval;
					ast_cse_off[ast_cse_n] = off;
					ast_cse_n++;
				}
			} else {
				ast_cse_n = 0;
			}
		} else if (k == AST_Return) {
			ast_cse_subst(a, ast_first_child(a, s), 0);
			ast_cse_n = 0;
		} else if (k == AST_If && ast_op(a, s) == 0) {
			ast_cse_subst(a, ast_child(a, s, 0), 0);
			ast_cse_n = 0;
		} else if (k == AST_Invoke) {
			for (AstLocal c = ast_first_child(a, s); c != AST_NONE; c = ast_next_sib(a, c))
				ast_cse_subst(a, c, 0);
			ast_cse_n = 0;
		} else if (ast_licm_is_loop(a, s)) {
			ast_licm_at_loop(a, s);
			ast_cse_n = 0;
		} else {
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

static void ast_cse_state_save(AstCseState *st) {
	st->n = ast_cse_n;
	memcpy(st->expr, ast_cse_expr, (size_t)ast_cse_n * sizeof(AstLocal));
	memcpy(st->ref, ast_cse_ref, (size_t)ast_cse_n * sizeof(AstLocal));
	memcpy(st->off, ast_cse_off, (size_t)ast_cse_n * sizeof(int));
}

static void ast_cse_state_load(const AstCseState *st) {
	ast_cse_n = st->n;
	memcpy(ast_cse_expr, st->expr, (size_t)st->n * sizeof(AstLocal));
	memcpy(ast_cse_ref, st->ref, (size_t)st->n * sizeof(AstLocal));
	memcpy(ast_cse_off, st->off, (size_t)st->n * sizeof(int));
}

static void ast_cse_state_meet(const AstCseState *st) {
	int i = 0;
	while (i < ast_cse_n) {
		int j, keep = 0;
		for (j = 0; j < st->n; j++)
			if (st->expr[j] == ast_cse_expr[i] && st->ref[j] == ast_cse_ref[i] &&
					st->off[j] == ast_cse_off[i]) {
				keep = 1;
				break;
			}
		if (keep) {
			i++;
		} else {
			ast_cse_n--;
			ast_cse_expr[i] = ast_cse_expr[ast_cse_n];
			ast_cse_ref[i] = ast_cse_ref[ast_cse_n];
			ast_cse_off[i] = ast_cse_off[ast_cse_n];
		}
	}
}

static unsigned char *ast_cse_vis;

static void ast_cse_stmts(AstArena *a, AstLocal bb) {
	if (bb == AST_NONE || ast_kind(a, bb) != AST_BasicBlock)
		return;
	ast_cse_vis[bb] = 1;
	for (AstLocal s = ast_first_child(a, bb); s != AST_NONE; s = ast_next_sib(a, s)) {
		uint16_t k = ast_kind(a, s);
		if (k == AST_Store) {
			AstLocal lval = ast_child(a, s, 0), val = ast_child(a, s, 1);
			ast_cse_subst(a, val, 0);
			ast_cse_subst(a, lval, 1);
			if (!ast_cprop_safe(a, lval) || !ast_cprop_safe(a, val)) {
				ast_cse_n = 0;
				continue;
			}
			int off, tt;
			if (ast_cprop_is_local(a, lval, &off, &tt)) {
				ast_cse_kill(a, off);
				int et;
				uint64_t er;
				if (!ast_cprop_escapes(a, off) && ast_cse_n < ast_cse_window &&
						ast_cse_regpure(a, val) && !ast_ident_leaf(a, val) &&
						!ast_tco_reads_off(a, val, off) &&
						ast_ident_etype(a, val, &et, &er) && ast_ident_intt(et) &&
						(et & (VT_BTYPE | VT_UNSIGNED)) == (tt & (VT_BTYPE | VT_UNSIGNED)) &&
						er == ast_type_ref(a, lval)) {
					ast_cse_expr[ast_cse_n] = val;
					ast_cse_ref[ast_cse_n] = lval;
					ast_cse_off[ast_cse_n] = off;
					ast_cse_n++;
				}
			} else {
				ast_cse_n = 0;
			}
		} else if (k == AST_Return) {
			ast_cse_subst(a, ast_first_child(a, s), 0);
			ast_cse_n = 0;
		} else if (k == AST_BasicBlock) {
			ast_cse_stmts(a, s);
		} else if (k == AST_If && ast_op(a, s) == 0) {
			ast_cse_subst(a, ast_child(a, s, 0), 0);
			AstLocal tb = ast_child(a, s, 1), eb = ast_child(a, s, 2);
			if (ast_cprop_opaque(a, tb) || ast_cprop_opaque(a, eb)) {
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
		} else if (k == AST_Invoke) {
			for (AstLocal c = ast_first_child(a, s); c != AST_NONE; c = ast_next_sib(a, c))
				ast_cse_subst(a, c, 0);
			if (ast_call_window_env) {
				for (int i = 0; i < ast_cse_n;)
					if (ast_cprop_escapes(a, ast_cse_off[i]) ||
							ast_licm_written(a, s, ast_cse_off[i]) ||
							!ast_licm_operands_ok(a, s, ast_cse_expr[i]))
						ast_cse_kill(a, ast_cse_off[i]);
					else
						i++;
			} else {
				ast_cse_n = 0;
			}
		} else if (k == AST_If && ast_op(a, s) >= 2 && ast_op(a, s) <= 4) {
			ast_licm_at_loop(a, s);
			for (int i = 0; i < ast_cse_n;)
				if (ast_licm_written(a, s, ast_cse_off[i]) ||
						!ast_licm_operands_ok(a, s, ast_cse_expr[i]))
					ast_cse_kill(a, ast_cse_off[i]);
				else
					i++;
			if (ast_sccp_has_label(a, s)) {
				ast_cse_n = 0;
				continue;
			}
			AstCseState in;
			ast_cse_state_save(&in);
			for (AstLocal c = ast_first_child(a, s); c != AST_NONE;
					 c = ast_next_sib(a, c)) {
				if (ast_kind(a, c) == AST_BasicBlock) {
					ast_cse_stmts(a, c);
					ast_cse_state_load(&in);
				} else if (ast_cprop_safe(a, c)) {
					ast_cse_subst(a, c, 0);
				}
			}
		} else if (ast_licm_is_loop(a, s)) {
			ast_licm_at_loop(a, s);
			ast_cse_n = 0;
		} else {
			ast_cse_n = 0;
		}
	}
}

static int ast_cse_run(AstArena *a) {
	ast_cse_folds = 0;
	ast_licm_folds = 0;
	AstLocal nn = ast_count(a);
	if (ast_cse_join_env && nn) {
		ast_cse_vis = mcc_mallocz(nn);
		ast_cse_n = 0;
		ast_cse_stmts(a, ast_root(a));
		for (AstLocal n = 0; n < nn; n++)
			if (ast_kind(a, n) == AST_BasicBlock && !ast_cse_vis[n])
				ast_cse_block(a, n);
		mcc_free(ast_cse_vis);
		ast_cse_vis = NULL;
		if (ast_licm_temp_env)
			ast_ltemp_run(a);
		if (ast_ivsr_env)
			ast_ivsr_run(a);
		if (ast_pre_env)
			ast_pre_run(a);
		return ast_cse_folds;
	}
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_BasicBlock)
			ast_cse_block(a, n);
	if (ast_licm_temp_env)
		ast_ltemp_run(a);
	if (ast_ivsr_env)
		ast_ivsr_run(a);
	if (ast_pre_env)
		ast_pre_run(a);
	return ast_cse_folds;
}

static int ast_replay_ok(AstArena *a) {
	if (ast_bail || ast_desync || ast_vn != 0 || ast_cf_top != 0)
		return 0;
	return ast_first_child(a, ast_root(a)) != AST_NONE;
}

static int ast_try_active;
static int ast_body_ind_sv;
static addr_t ast_reloc0_sv;

void ast_func_begin(Sym *sym) {
	MCC_TRACE("%s\n", funcname);
	ast_inline_capture(sym);
	int ast_ret_bad = ast_bad_type(func_vt.t) &&
										(func_vt.t & VT_BTYPE) != VT_STRUCT;
	ast_try_active = ast_replay_env && !debug_modes && !cur_func_inline_extern &&
								!ast_ret_bad;
	ast_body_ind_sv = ind;
	ast_reloc0_sv =
			cur_text_section->reloc ? cur_text_section->reloc->data_offset : 0;
	if (ast_try_active) {
		ast_cur = ast_arena_new();
		ast_cur_bb = ast_node(ast_cur, AST_BasicBlock);
		ast_cf_top = 0;
		ast_tern_top = 0;
		ast_lor_top = 0;
		ast_bail = 0;
		ast_desync = 0;
		ast_reemit_poison = 0;
		ast_in_op = 0;
		ast_in_call = 0;
		ast_call_pending = AST_NONE;
		ast_inc_pending = AST_NONE;
		ast_vn = 0;
		ast_ret_val = AST_NONE;
		ast_last_return = AST_NONE;
		ast_base_depth = (int)(vtop - vstack + 1);
		ast_fconst_n = 0;
		ast_locrec_n = 0;
		ast_replaying = 0;
		ast_switch_node = AST_NONE;
		ast_func_has_asm = 0;
		ast_active = 1;
		ast_capture = 1;
		ast_sym_deferred = NULL;
		ast_sym_defer_on = 1;
	}
}

/*
 * Strategy engine (rollout step 4). Each fixed-pipeline pass is wrapped as an
 * AstStrategy {name, gate, apply} and the pipeline order is a frozen data table
 * (ast_strategies) rather than a hand-ordered call sequence. ast_func_end always
 * consumes this table — it is the sole pipeline (the earlier legacy inline block
 * and its MCC_AST_ENGINE toggle have been removed). The table order, gates, and
 * arguments reproduced the legacy block byte-for-byte, so making it the only
 * engine changed no emitted bytes. `match` is the gate; `est_cost_delta` (the
 * step-5 search's ranking key) is deferred.
 */
typedef struct AstStrategy {
	const char *name;
	const int *gate;
	int (*apply)(AstArena *a, Sym *sym);
} AstStrategy;

static int ast_strat_bfold(AstArena *a, Sym *s) { (void)s; return ast_bfold_run(a); }
static int ast_strat_ident(AstArena *a, Sym *s) { (void)s; return ast_ident_run(a); }
static int ast_strat_narrow(AstArena *a, Sym *s) { (void)s; return ast_narrow_run(a); }
static int ast_strat_cprop(AstArena *a, Sym *s) { (void)s; return ast_cprop_run(a); }
static int ast_strat_cse(AstArena *a, Sym *s) { (void)s; return ast_cse_run(a); }
static int ast_strat_licm(AstArena *a, Sym *s) { (void)a; (void)s; return ast_licm_folds; }
static int ast_strat_dse(AstArena *a, Sym *s) { (void)s; return ast_dse_run(a); }
static int ast_strat_sccp(AstArena *a, Sym *s) { (void)s; return ast_sccp_run(a); }
static int ast_strat_jt(AstArena *a, Sym *s) { (void)s; return ast_jt_run(a); }
static int ast_strat_bf(AstArena *a, Sym *s) { (void)s; return ast_bf_run(a); }
static int ast_strat_range(AstArena *a, Sym *s) { (void)s; return ast_range_run(a); }
static int ast_strat_divmagic(AstArena *a, Sym *s) { (void)s; return ast_divmagic_run(a); }
static int ast_strat_abs(AstArena *a, Sym *s) { (void)s; return ast_abs_run(a); }
static int ast_strat_reassoc(AstArena *a, Sym *s) { (void)s; return ast_reassoc_run(a); }
static int ast_strat_sethi(AstArena *a, Sym *s) { (void)s; return ast_sethi_run(a); }
static int ast_strat_tco(AstArena *a, Sym *s) { return ast_tco_run(a, s); }

enum {
	AST_STRAT_BFOLD,
	AST_STRAT_IDENT,
	AST_STRAT_NARROW,
	AST_STRAT_CPROP,
	AST_STRAT_CSE,
	AST_STRAT_LICM,
	AST_STRAT_DSE,
	AST_STRAT_SCCP,
	AST_STRAT_JT,
	AST_STRAT_BF,
	AST_STRAT_RANGE,
	AST_STRAT_DIVMAGIC,
	AST_STRAT_ABS,
	AST_STRAT_REASSOC,
	AST_STRAT_SETHI,
	AST_STRAT_TCO,
	AST_STRAT_COUNT
};

static const AstStrategy ast_strategies[AST_STRAT_COUNT] = {
	{"bfold", &ast_templates_env, ast_strat_bfold},
	{"ident", &ast_templates_env, ast_strat_ident},
	{"narrow", &ast_narrow_env, ast_strat_narrow},
	{"cprop", &ast_templates_env, ast_strat_cprop},
	{"cse", &ast_templates_env, ast_strat_cse},
	{"licm", &ast_templates_env, ast_strat_licm},
	{"dse", &ast_templates_env, ast_strat_dse},
	{"sccp", &ast_templates_env, ast_strat_sccp},
	{"jt", &ast_templates_env, ast_strat_jt},
	{"bf", &ast_bitflag_env, ast_strat_bf},
	{"range", &ast_range_env, ast_strat_range},
	{"divmagic", &ast_divmagic_env, ast_strat_divmagic},
	{"abs", &ast_abs_env, ast_strat_abs},
	{"reassoc", &ast_reassoc_env, ast_strat_reassoc},
	{"sethi", &ast_sethi_env, ast_strat_sethi},
	{"tco", &ast_templates_env, ast_strat_tco},
};

/*
 * Live -O4+ search (rollout step 5). Opt-in via MCC_AST_SEARCH at -O4+
 * (optimize_search_seconds>0, so the driver has budget for a search): ast_func_end
 * runs a best-first per-function search over the four toggleable fold gates
 * (templates, narrow, bitflag, sethi) instead of applying the single frozen order.
 * It is a separate opt-in (not the default -O4+ path) so it does not perturb the
 * out-of-process superopt's per-worker gate measurements.
 *
 * Execution model (shared shape with the future runtime JIT). Each candidate gate
 * config is a stackless coroutine whose one "tick" applies exactly one strategy
 * pass to its own isolated clone. The scheduler does not round-robin: it always
 * advances the live candidate that has consumed the least total time so far (the
 * running sum of its tick durations; ties break to the lowest index, so the
 * baseline config finishes first as the safe fallback). Equalizing accumulated
 * time keeps the schedule fair — no candidate starves or monopolizes the budget.
 * A rolling window of
 * the last 10 tick durations predicts the next tick cost; the search stops once
 * that predicted cost would exceed the budget remaining. The budget is the -ON
 * seconds, absolute from the first search tick; when it is spent the search stops
 * at the next tick boundary and any unfinished candidate is abandoned.
 * ast_search_abort is a forced-abort hook (for the pool / JIT / a deadline signal):
 * it is checked at every tick boundary and, because each candidate mutates only a
 * throwaway clone, an abort discards in-progress work safely — the pool's "kill and
 * restart the worker" reduces to this discard.
 *
 * Correctness. The search only SELECTS a gate configuration — the winning config is
 * produced by the normal, unmodified pipeline+emit path on the untouched captured
 * tree, so a search bug (or a time-truncated / aborted search) can only pick a
 * larger-but-correct config, never a miscompile. Measurement scores by the static
 * cost model with no emit, so the emit-cursor / promo-plan / *_total-counter hazards
 * do not arise; the module counters are saved/restored regardless. Winners are
 * memoized by intention hash so a recurring function skips the search. -O1..-O3
 * (no budget) never search and stay byte-reproducible.
 *
 * Single-threaded and statically scored here. The NCores-1 coroutine thread pool
 * (the round-robin generalized across workers), emitted-size / JIT-runtime scoring
 * (needs scratch-Section emit isolation), the disk-backed cross-build memo, and
 * runtime deopt are the documented step-5+ continuations in docs/TODO.md.
 */
/* The strategy/knob bitset (AstGateMask), the AST_SG_* gates, and the M3 superopt
 * vocabulary bridge live in a shared, dependency-free header so the unit harness
 * (tools/asttool.c) can selftest the bridge without the MCC_INTERNAL search body. */
#include "mccgate.h"

typedef struct AstSearchMemo {
	uint64_t hash;
	AstGateMask gates;
	unsigned refcount;
	/* M3 blocker A: fields the out-of-process drivers keep in SoPfCkpt so one record
	 * can serve both searches. `score` = the winning config's search score (the packed
	 * cost/emit-size ranking key, -1 if unknown); `tried` = a bitmask of which candidate
	 * configs have been measured, for a resumable per-function search (0 = not tracked by
	 * the in-process search yet). Both persisted so a future unified driver can resume /
	 * compare against the superopt's best_size + tried without a second substrate. */
	int64_t score;
	uint64_t tried;
} AstSearchMemo;

#define AST_SEARCH_MEMO_CAP 4096
/* Max candidates the per-function search enumerates (budget cap on the subset lattice
 * of `searchable`; the per-tick time budget is the primary bound). Also sizes the fork
 * pool's gatelist. 128 comfortably covers 4 fold gates + the opt-in knobs (<=2^9). */
#define AST_SEARCH_MAX_CAND 128
static AstSearchMemo ast_search_memo[AST_SEARCH_MEMO_CAP];
static int ast_search_memo_n;

/*
 * Cross-build persistence of the per-function search winner, as REFCOUNTED
 * permutations. Each record is {uint64 intention-hash, uint64 (gates | MAGIC<<8),
 * uint64 refcount} appended to a file in the per-user cache dir; the MAGIC in every
 * record lets a torn/stale record be skipped without a global header. A hit bumps
 * the winning permutation's refcount and re-appends it (last-wins on load).
 *
 * Eviction: every cache accessor (load / store / hit — all route through
 * ast_search_disk_store) checks the *shared* disk usage of the whole cache dir; once
 * it reaches AST_SEARCH_DISK_MAX (10 GiB, shared across concurrent builds) it drops
 * the lowest-refcount quarter of the working set and rewrites the file (temp +
 * rename), keeping the most-reused permutations. Cleared by `mcc --clear-cache`;
 * opt-in with MCC_AST_SEARCH; a missing/unwritable cache dir degrades to the
 * in-memory memo. The in-memory working set is capped (AST_SEARCH_MEMO_CAP), so an
 * eviction rewrite also compacts the file down to that hot set. */
#define AST_SEARCH_MEMO_MAGIC 0x4643u /* 'FC' */
/* On-disk record word layout: the low AST_GATE_BITS hold the AstGateMask (up to 48
 * strategy knobs — past any host's native width), MAGIC occupies the high 16 bits so
 * a torn/stale record is still skippable without a global header. Widened from the
 * original 8-bit gate field so added knobs are never truncated on persist/reload. */
#define AST_GATE_BITS 48
#define AST_GATE_DISK_MASK ((uint64_t)(((uint64_t)1 << AST_GATE_BITS) - 1))
#define AST_SEARCH_DISK_MAX (10ull << 30) /* 10 GiB shared cache-dir cap */

/* Salt the search-memo key with the build version + target triplet (mirrors
 * so_pf_key in mcc.c, roadmap M2 blocker B): ast_intention_hash folds only AST
 * structure, so without this a winner cached by an incompatible mcc build or a
 * different target would be silently reused across the shared cache dir. The
 * version string is only visible when mccast.c is compiled inside the mcc TU
 * (mcc.h present); the asttool unit harness includes it standalone, so guard on
 * the macro and fall back to triplet-only (which is a compile define everywhere). */
static uint64_t ast_search_key_salt(uint64_t h) {
	const char *s;
	(void)s;
#ifdef MCC_VERSION_STR
	for (s = MCC_VERSION_STR; *s; s++)
		h = (h ^ (unsigned char)*s) * 0x100000001b3ull;
#endif
#ifdef MCC_CONFIG_TRIPLET
	for (s = MCC_CONFIG_TRIPLET; *s; s++)
		h = (h ^ (unsigned char)*s) * 0x100000001b3ull;
#endif
	return h;
}

static int ast_search_cache_dir(char *buf, int cap) {
	return host_cache_dir(buf, cap);
}

static int ast_search_disk_path(char *buf, int cap) {
	char dir[1024];
	if (ast_search_cache_dir(dir, sizeof dir) != 0)
		return -1;
	if (snprintf(buf, cap, "%s/mcc-search.memo", dir) >= cap)
		return -1;
	return 0;
}

/* Total bytes of the shared cache dir (all users' files). POSIX: sum the regular
 * files; else fall back to just the memo file's size. */
#if MCC_HOST_POSIX
#include <dirent.h>
#include <sys/stat.h>
static unsigned long long ast_search_disk_usage(void) {
	char dir[1024], path[1152];
	DIR *d;
	struct dirent *e;
	struct stat st;
	unsigned long long total = 0;
	if (ast_search_cache_dir(dir, sizeof dir) != 0)
		return 0;
	d = opendir(dir);
	if (!d)
		return 0;
	while ((e = readdir(d)) != NULL) {
		if (snprintf(path, sizeof path, "%s/%s", dir, e->d_name) >= (int)sizeof path)
			continue;
		if (stat(path, &st) == 0 && S_ISREG(st.st_mode))
			total += (unsigned long long)st.st_size;
	}
	closedir(d);
	return total;
}
#else
static unsigned long long ast_search_disk_usage(void) {
	char path[1152];
	FILE *f;
	long sz;
	if (ast_search_disk_path(path, sizeof path) != 0)
		return 0;
	f = fopen(path, "rb");
	if (!f)
		return 0;
	fseek(f, 0, SEEK_END);
	sz = ftell(f);
	fclose(f);
	return sz > 0 ? (unsigned long long)sz : 0;
}
#endif

/* Returns 1 if the working set changed (new record, or a bumped gate/refcount), 0
 * if the call was a no-op — the disk store uses this to skip a pointless rewrite. */
static int ast_search_memo_add(uint64_t h, AstGateMask gates, unsigned refcount,
															 int64_t score, uint64_t tried) {
	int i, changed = 0;
	for (i = 0; i < ast_search_memo_n; i++)
		if (ast_search_memo[i].hash == h) {
			if (ast_search_memo[i].gates != gates) {
				ast_search_memo[i].gates = gates;
				changed = 1;
			}
			if (refcount > ast_search_memo[i].refcount) {
				ast_search_memo[i].refcount = refcount;
				changed = 1;
			}
			if (score >= 0 && score != ast_search_memo[i].score) {
				ast_search_memo[i].score = score;
				changed = 1;
			}
			if ((tried | ast_search_memo[i].tried) != ast_search_memo[i].tried) {
				ast_search_memo[i].tried |= tried; /* accumulate measured-config progress */
				changed = 1;
			}
			return changed;
		}
	if (ast_search_memo_n < AST_SEARCH_MEMO_CAP) {
		ast_search_memo[ast_search_memo_n].hash = h;
		ast_search_memo[ast_search_memo_n].gates = gates;
		ast_search_memo[ast_search_memo_n].refcount = refcount;
		ast_search_memo[ast_search_memo_n].score = score;
		ast_search_memo[ast_search_memo_n].tried = tried;
		ast_search_memo_n++;
		changed = 1;
	}
	return changed;
}

/*
 * On-disk container. The working set is serialized to a raw record image, then
 * compressed with the best of the three src/algorithms codecs (rle/lzss/lzw) and
 * written whole (temp + atomic rename): {u32 magic, u32 codec, u32 raw_len,
 * u32 comp_len} followed by the payload. Compressing each build's memo file keeps
 * every user's cache small so far more permutations fit under the shared 10 GiB cap
 * before eviction. The container magic differs from the old raw append-log format,
 * so a pre-compression file simply reads as empty (a harmless cache rebuild). The
 * memo image is highly compressible (a repeated per-record MAGIC, small gate masks,
 * structured 64-bit fields), so a codec almost always beats codec 0 (stored). */
#define AST_MEMO_CONT_MAGIC 0x315a534dUL /* "MSZ1" */
#define AST_MEMO_RECWORDS 5 /* {hash, gates|magic, refcount, score, tried} */
#define AST_MEMO_RECBYTES (AST_MEMO_RECWORDS * 8)
#define AST_MEMO_RAWMAX (AST_SEARCH_MEMO_CAP * AST_MEMO_RECBYTES)
static unsigned char ast_memo_raw[AST_MEMO_RAWMAX];
static unsigned char ast_memo_pk[AST_MEMO_RAWMAX * 2 + 64];
static unsigned char ast_memo_try[AST_MEMO_RAWMAX * 2 + 64];

/* Compress ast_memo_raw[0..rn) into ast_memo_pk with the smallest of the three
 * codecs; returns the codec id (0 = stored, when nothing beats raw) and sets *plen. */
static int ast_memo_pack(long rn, long *plen) {
	long best = rn, l;
	int codec = 0;
	memcpy(ast_memo_pk, ast_memo_raw, (size_t)rn);
	l = rle_compress(ast_memo_raw, rn, ast_memo_try, (long)sizeof ast_memo_try);
	if (l >= 0 && l < best) {
		best = l, codec = 1;
		memcpy(ast_memo_pk, ast_memo_try, (size_t)l);
	}
	l = lzss_compress(ast_memo_raw, rn, ast_memo_try, (long)sizeof ast_memo_try);
	if (l >= 0 && l < best) {
		best = l, codec = 2;
		memcpy(ast_memo_pk, ast_memo_try, (size_t)l);
	}
	l = lzw_compress(ast_memo_raw, rn, ast_memo_try, (long)sizeof ast_memo_try);
	if (l >= 0 && l < best) {
		best = l, codec = 3;
		memcpy(ast_memo_pk, ast_memo_try, (size_t)l);
	}
	*plen = best;
	return codec;
}

/* Decompress a container payload in ast_memo_pk[0..clen) into ast_memo_raw; returns
 * the raw length, or -1 on a corrupt/oversized payload. */
static long ast_memo_unpack(int codec, long clen) {
	switch (codec) {
	case 0:
		if (clen > AST_MEMO_RAWMAX)
			return -1;
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

/* Rewrite the memo file as a compressed container from the in-memory working set. */
static void ast_search_disk_rewrite(void) {
	char path[1152], tmp[1200];
	FILE *f;
	int i;
	long rn = 0, plen = 0;
	unsigned hdr[4];
	int codec;
	if (ast_search_disk_path(path, sizeof path) != 0)
		return;
	if (snprintf(tmp, sizeof tmp, "%s.tmp", path) >= (int)sizeof tmp)
		return;
	for (i = 0; i < ast_search_memo_n; i++) {
		uint64_t rec[AST_MEMO_RECWORDS];
		rec[0] = ast_search_memo[i].hash;
		rec[1] = (ast_search_memo[i].gates & AST_GATE_DISK_MASK) |
						 ((uint64_t)AST_SEARCH_MEMO_MAGIC << AST_GATE_BITS);
		rec[2] = ast_search_memo[i].refcount;
		rec[3] = (uint64_t)ast_search_memo[i].score;
		rec[4] = ast_search_memo[i].tried;
		memcpy(ast_memo_raw + rn, rec, sizeof rec);
		rn += (long)sizeof rec;
	}
	codec = ast_memo_pack(rn, &plen);
	f = fopen(tmp, "wb");
	if (!f)
		return;
	hdr[0] = (unsigned)AST_MEMO_CONT_MAGIC;
	hdr[1] = (unsigned)codec;
	hdr[2] = (unsigned)rn;
	hdr[3] = (unsigned)plen;
	if (fwrite(hdr, sizeof hdr, 1, f) == 1 && plen > 0)
		fwrite(ast_memo_pk, 1, (size_t)plen, f);
	fclose(f);
	if (rename(tmp, path) != 0)
		remove(tmp);
}

static int ast_search_memo_cmp(const void *a, const void *b) {
	const AstSearchMemo *x = a, *y = b;
	if (x->refcount < y->refcount)
		return 1; /* descending: high refcount first */
	if (x->refcount > y->refcount)
		return -1;
	return 0;
}

/* Triggered by each accessor: if the shared cache dir has reached the cap, evict the
 * lowest-refcount quarter of the working set and rewrite the file. */
static void ast_search_disk_evict(void) {
	if (ast_search_memo_n < 4)
		return;
	if (ast_search_disk_usage() < AST_SEARCH_DISK_MAX)
		return;
	qsort(ast_search_memo, (size_t)ast_search_memo_n, sizeof ast_search_memo[0],
				ast_search_memo_cmp);
	MCC_TRACE("disk evict: usage=%lluMiB >= cap, dropping %d/%d lowest-refcount entries\n",
						ast_search_disk_usage() >> 20, ast_search_memo_n / 4, ast_search_memo_n);
	ast_search_memo_n -= ast_search_memo_n / 4;
	ast_search_disk_rewrite();
}

static void ast_search_disk_load(void) {
	char path[1152];
	FILE *f;
	unsigned hdr[4];
	long clen, rl, i;
	if (ast_search_disk_path(path, sizeof path) != 0)
		return;
	f = fopen(path, "rb");
	if (!f)
		return;
	if (fread(hdr, sizeof hdr, 1, f) != 1 || hdr[0] != (unsigned)AST_MEMO_CONT_MAGIC ||
			hdr[2] > (unsigned)AST_MEMO_RAWMAX || hdr[3] > (unsigned)sizeof ast_memo_pk) {
		fclose(f); /* absent / old raw-log / corrupt: start empty (cache rebuilds) */
		return;
	}
	clen = (long)hdr[3];
	if (clen > 0 && fread(ast_memo_pk, 1, (size_t)clen, f) != (size_t)clen) {
		fclose(f);
		return;
	}
	fclose(f);
	rl = ast_memo_unpack((int)hdr[1], clen);
	if (rl != (long)hdr[2])
		return; /* codec/length mismatch: corrupt container */
	for (i = 0; i + AST_MEMO_RECBYTES <= rl && ast_search_memo_n < AST_SEARCH_MEMO_CAP;
			 i += AST_MEMO_RECBYTES) {
		uint64_t rec[AST_MEMO_RECWORDS];
		memcpy(rec, ast_memo_raw + i, sizeof rec);
		if ((rec[1] >> AST_GATE_BITS) == AST_SEARCH_MEMO_MAGIC)
			ast_search_memo_add(rec[0], rec[1] & AST_GATE_DISK_MASK, (unsigned)rec[2],
													(int64_t)rec[3], rec[4]);
	}
	MCC_TRACE("disk load: %s codec=%u raw=%ldB -> %d memo entries\n", path, hdr[1], rl,
						ast_search_memo_n);
	ast_search_disk_evict();
}

/* Persist a permutation: fold it into the working set and, if that changed the set,
 * rewrite the compressed container. Every accessor also triggers the shared-disk
 * eviction check. */
static void ast_search_disk_store(uint64_t h, AstGateMask gates, unsigned refcount,
																	int64_t score, uint64_t tried) {
	if (ast_search_memo_add(h, gates, refcount, score, tried))
		ast_search_disk_rewrite();
	ast_search_disk_evict();
}

/* Round-robin time accounting (see the block comment above). Durations are in
 * milliseconds of CPU time (clock()), an accurate wall-clock proxy for the
 * single-threaded CPU-bound search and portable to every host — including the
 * asttool unit harness, which includes mccast.c without the mcchost timer. */
#define AST_SEARCH_WIN 10
static int ast_search_started;
static unsigned ast_search_start_ms;
static unsigned ast_search_budget_ms;
static unsigned ast_search_durwin[AST_SEARCH_WIN];
static int ast_search_durwin_n;
static int ast_search_durwin_head;
static volatile int ast_search_abort;

static unsigned ast_now_ms(void) {
	return (unsigned)((unsigned long long)clock() * 1000ull / CLOCKS_PER_SEC);
}

static void ast_search_durwin_push(unsigned dt) {
	ast_search_durwin[ast_search_durwin_head] = dt;
	ast_search_durwin_head = (ast_search_durwin_head + 1) % AST_SEARCH_WIN;
	if (ast_search_durwin_n < AST_SEARCH_WIN)
		ast_search_durwin_n++;
}

/* Predict the next tick's duration from the rolling window via the forecasting
 * ensemble (mccforecast.h): the samples are extracted oldest->newest and handed to
 * ast_fc_forecast, which picks the least-outlier of its three most accurate
 * one-step predictors. */
static unsigned ast_search_expect_ms(void) {
	double y[AST_SEARCH_WIN], pred;
	int n = ast_search_durwin_n, start, i;
	if (n <= 0)
		return 0;
	start = (ast_search_durwin_head - n + AST_SEARCH_WIN * 2) % AST_SEARCH_WIN;
	for (i = 0; i < n; i++)
		y[i] = (double)ast_search_durwin[(start + i) % AST_SEARCH_WIN];
	pred = ast_fc_forecast(y, n);
	if (pred < 0)
		pred = 0;
	return (unsigned)(pred + 0.5);
}

static unsigned ast_search_remaining_ms(void) {
	unsigned el = ast_now_ms() - ast_search_start_ms;
	return el >= ast_search_budget_ms ? 0 : ast_search_budget_ms - el;
}

/* stop before the next tick: forced abort, no budget left, or the predicted next
 * tick would overrun the remaining budget. */
static int ast_search_should_stop(void) {
	unsigned rem;
	if (ast_search_abort)
		return 1;
	rem = ast_search_remaining_ms();
	if (rem == 0)
		return 1;
	return ast_search_expect_ms() > rem;
}

static AstGateMask ast_search_gates_now(void) {
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
				 (ast_sccp_fix_env ? AST_SG_SCCPFIX : 0);
}

static void ast_search_gates_set(AstGateMask g) {
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
}

/*
 * Emitted-byte-size scoring (MCC_AST_SEARCH_EMITSIZE). Replay the fully-folded
 * candidate into the live text section — the same in-place emit-and-rewind the
 * inline on/off trial (AST_PF_EMIT) already does — with inline and promotion off
 * (so the promotion save/restore desync never arises), read the byte length, then
 * rewind every emit cursor. Correctness is unaffected: this only produces a score;
 * the winning gate config is emitted by the normal pipeline on the untouched
 * captured tree, and a mis-emit here reverts through ast_func_end's faithful
 * revert. Used only in the run-to-completion path below (the fair-interleave tick
 * scheduler thrashes the shared ltemp/fconst emit state across candidates, so a
 * candidate is only emit-measurable right after it folds to completion). */
static int ast_search_emit_size(AstArena *a, int saved_loc, int saved_anon) {
	Section *rsec = cur_text_section->reloc;
	AstArena *save_cur = ast_cur;
	int save_ind = ind, save_rsym = rsym, save_loc = loc, save_anon = anon_sym;
	addr_t save_reloc = rsec ? rsec->data_offset : 0;
	addr_t save_doff = data_section ? data_section->data_offset : 0;
	addr_t save_roff = rodata_section ? rodata_section->data_offset : 0;
	addr_t ddelta, rodelta;
	int size;
	ind = ast_body_ind_sv;
	rsym = 0;
	if (rsec)
		rsec->data_offset = ast_reloc0_sv;
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
	ast_inline_active = 0;
	ast_graft_budget = ast_graft_budget_max;
	ast_loc_low = loc;
	ast_cur = a;
	ast_replay_body(a);
	ast_replaying = 0;
	ddelta = (data_section ? data_section->data_offset : 0) - save_doff;
	rodelta = (rodata_section ? rodata_section->data_offset : 0) - save_roff;
	size = ind - ast_body_ind_sv;
	if (ddelta || rodelta)
		MCC_TRACE("emit-size data delta text=%d data=%lld rodata=%lld\n", size,
							(long long)ddelta, (long long)rodelta);
	ast_cur = save_cur;
	ind = save_ind;
	rsym = save_rsym;
	if (rsec)
		rsec->data_offset = save_reloc;
	loc = save_loc;
	anon_sym = save_anon;
	return size;
}

/*
 * Pack the primary metric (static cost or emitted size, lower better) with the
 * candidate's total hit count (the sum of fold-counts every applied strategy
 * reported on this AST window) so a single `long` ranks by cost first and, WITHIN
 * an equal cost, favors the config whose algorithms actually fired more on this
 * slice — a config that enables a gate which lands many folds beats one where the
 * gate does nothing. The primary occupies the high bits; the low AST_SCORE_HITBITS
 * hold (max_hits - clamp(hits)) so more hits lowers the packed score. Ties in cost
 * therefore break to more hits, and the primary ordering is otherwise unchanged. */
#define AST_SCORE_HITBITS 12
#define AST_SCORE_HITMAX ((1L << AST_SCORE_HITBITS) - 1)
static long ast_search_pack_score(long primary, long hits) {
	long h;
	if (primary < 0)
		return -1; /* propagate clone failure / reject */
	h = hits < 0 ? 0 : (hits > AST_SCORE_HITMAX ? AST_SCORE_HITMAX : hits);
	return (primary << AST_SCORE_HITBITS) + (AST_SCORE_HITMAX - h);
}

/* Fold a candidate to completion and score it by emitted size (run-to-completion,
 * so its ltemp/fconst emit state is current). */
static long ast_search_score_emitsize(AstArena *pristine, Sym *sym, int faithful,
																			AstGateMask gates, int saved_loc,
																			int saved_anon) {
	AstArena *saved_cur = ast_cur, *trial = ast_arena_clone(pristine);
	int si;
	long size, hits = 0;
	if (!trial)
		return -1;
	ast_search_gates_set(gates);
	ast_cur = trial;
	for (si = 0; si < AST_STRAT_COUNT; si++)
		if (faithful && *ast_strategies[si].gate)
			hits += ast_strategies[si].apply(trial, sym);
	size = ast_search_emit_size(trial, saved_loc, saved_anon);
	ast_cur = saved_cur;
	ast_arena_free(trial);
	return ast_search_pack_score(size, hits);
}

/* Score one candidate (static cost or emit-size) — used by both the serial paths
 * and the fork-pool workers. */
static long ast_search_score_one(AstArena *pristine, Sym *sym, int faithful,
																 AstGateMask gates, int saved_loc, int saved_anon) {
	AstArena *saved_cur, *trial;
	int si;
	long sc, hits = 0;
	if (ast_search_emitsize_env)
		return ast_search_score_emitsize(pristine, sym, faithful, gates, saved_loc,
																		 saved_anon);
	saved_cur = ast_cur;
	trial = ast_arena_clone(pristine);
	if (!trial)
		return -1;
	ast_search_gates_set(gates);
	ast_cur = trial;
	for (si = 0; si < AST_STRAT_COUNT; si++)
		if (faithful && *ast_strategies[si].gate)
			hits += ast_strategies[si].apply(trial, sym);
	sc = ast_cost_score(trial);
	ast_cur = saved_cur;
	ast_arena_free(trial);
	return ast_search_pack_score(sc, hits);
}

/*
 * combo substrate wiring (roadmap M1). The -O4 gate search runs on combo_run
 * (src/mcccombo.h): each combo item is one baseline-enabled fold gate, sel[] is a
 * subset (subset mode) or ordering (MCC_AST_SEARCH_ORDERED) of those items, and the
 * score fn maps sel[] back to an AST_SG_* mask and scores it with the existing
 * ast_search_score_one (static cost or emit-size, whichever the env selects). The
 * fn owns the process-global save/restore inside ast_search_score_one, so combo_run
 * stays a pure enumerator. Budget/abort is enforced here: once the time budget is
 * spent every remaining candidate is rejected (combo_run keeps iterating the <=16
 * space but each check is O(1)). Lower score wins; ties resolve to the base config
 * in ast_search_select (scored first, strict-less keep-rule). */
typedef struct AstComboCtx {
	AstArena *pristine;
	Sym *sym;
	int faithful;
	int saved_loc;
	int saved_anon;
	const AstGateMask *items;
	uint64_t tried; /* bit per candidate actually measured (M3 blocker A progress) */
	int ord;        /* running candidate ordinal, capped at 63 */
} AstComboCtx;

static long ast_search_combo_score(const int *sel, int k, void *user) {
	AstComboCtx *cx = (AstComboCtx *)user;
	AstGateMask gates = 0;
	unsigned t0;
	long sc;
	int i;
	if (ast_search_should_stop())
		return COMBO_REJECT; /* budget spent: this candidate is NOT measured/tried */
	if (cx->ord < 64)
		cx->tried |= (uint64_t)1 << cx->ord; /* record that this candidate was measured */
	cx->ord++;
	for (i = 0; i < k; i++)
		gates |= cx->items[sel[i]];
	t0 = ast_now_ms();
	sc = ast_search_score_one(cx->pristine, cx->sym, cx->faithful, gates,
														cx->saved_loc, cx->saved_anon);
	ast_search_durwin_push(ast_now_ms() - t0);
	MCC_TRACE("combo cand gates=%llx k=%d score=%ld\n", (unsigned long long)gates, k, sc);
	if (sc < 0)
		return COMBO_REJECT;
	return sc;
}

/*
 * NCores-1 process pool. Fork up to nproc-1 score-only workers over the candidate
 * set. A fork gives each worker its own copy of every optimizer global (COW), so a
 * worker folds+scores its candidates with zero shared-state contention and no
 * _Thread_local marking — the fork isolation replaces the whole per-context state
 * refactor for the scoring step. Workers only read the shared pristine tree (COW)
 * and write {index, score} records to a pipe, then _exit without flushing; the
 * parent (which never mutates shared state during the fork window) collects the
 * records and applies the winner single-threaded. POSIX only; elsewhere the caller
 * falls back to the serial loop. Returns 1 if it produced a result. */
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
													 long *best_score_out) {
	int nw = host_nproc() - 1, pipefd[2], w, i, done = 0;
	pid_t pids[64];
	AstGateMask best = gatelist[0];
	long best_score = -1;
	AstScoreRec rec;
	if (nw < 2)
		return 0; /* not worth forking */
	if (nw > nc)
		nw = nc;
	if (nw > 64)
		nw = 64;
	if (pipe(pipefd) != 0)
		return 0;
	for (w = 0; w < nw; w++) {
		pid_t pid = fork();
		if (pid == 0) {
			close(pipefd[0]);
			for (i = w; i < nc; i += nw) {
				AstScoreRec r;
				r.idx = i;
				r.score = ast_search_score_one(pristine, sym, faithful, gatelist[i],
																			 saved_loc, saved_anon);
				if (write(pipefd[1], &r, sizeof r) != (ssize_t)sizeof r)
					break;
			}
			close(pipefd[1]);
			_exit(0);
		}
		pids[w] = pid; /* pid<0 (fork failed): recorded, waitpid skips it */
	}
	close(pipefd[1]);
	while (read(pipefd[0], &rec, sizeof rec) == (ssize_t)sizeof rec) {
		done++;
		if (rec.score >= 0 && (best_score < 0 || rec.score < best_score)) {
			best_score = rec.score;
			best = gatelist[rec.idx];
		}
	}
	close(pipefd[0]);
	for (w = 0; w < nw; w++)
		if (pids[w] > 0) {
			int st;
			waitpid(pids[w], &st, 0);
		}
	if (!done)
		return 0; /* every fork failed: fall back to serial */
	*best_out = best;
	*best_score_out = best_score;
	return 1;
}
#endif

static void ast_search_select(Sym *sym, int faithful, int saved_loc,
															int saved_anon) {
	AstArena *pristine;
	uint64_t h;
	AstGateMask base, best, searchable;
	long best_score = -1;
	uint64_t tried_mask = 0; /* which candidates were measured (M3 blocker A progress) */
	int g0, p0, o0, nc = 0;
	/* Budget-scaling the candidate count: the subset lattice of `searchable` can be as
	 * large as 2^(fold gates + opt-in knobs) (up to 2^9 once ltemp/ivsr/pre are offered),
	 * so both the combo enumeration (spec.budget) and the fork pool's gatelist are capped
	 * at AST_SEARCH_MAX_CAND. The per-tick time budget (ast_search_should_stop) remains the
	 * primary bound; this cap prevents pathological enumeration and gatelist overflow. */
	AstGateMask gatelist[AST_SEARCH_MAX_CAND];
	if (!ast_search_started) {
		ast_search_started = 1;
		ast_search_start_ms = ast_now_ms();
		ast_search_budget_ms = ast_search_seconds * 1000u;
		ast_search_disk_load();
	}
	base = ast_search_gates_now();
	/* searchable = base plus the opt-in enablement knobs the search may ADD this
	 * build. These are off in every -O baseline, so the subset lattice can never
	 * reach them by dropping bits — the search enables them. narrow-fixpoint only
	 * bites when narrow itself is enabled, so gate it on AST_SG_NARROW. */
	searchable = base | AST_SG_RANGE | AST_SG_DIVMAGIC | AST_SG_ABS | AST_SG_REASSOC | /* standalone */
							 ((base & AST_SG_NARROW) ? AST_SG_NARROWFIX : 0) |
							 ((base & AST_SG_SETHI) ? AST_SG_SETHILEAF : 0) |
							 /* ltemp/ivsr/pre run inside cse (templates-gated), so offer them only
								* when templates is in base. Safe to add now that the candidate count is
								* budget-capped (AST_SEARCH_MAX_CAND) — otherwise 4 gates + 5 knobs = 2^9. */
							 ((base & AST_SG_TEMPLATES) ? (AST_SG_LTEMP | AST_SG_IVSR | AST_SG_PRE | AST_SG_DSECALL | AST_SG_TCOPTR | AST_SG_CSECOMM | AST_SG_SCCPFIX)
																				 : 0);
	if (ast_search_should_stop())
		return; /* budget spent / aborted: keep the frozen order */
	pristine = ast_arena_clone(ast_cur);
	if (!pristine)
		return;
	h = ast_intention_hash(pristine, AST_NONE);
	if (h)
		h = ast_search_key_salt(h); /* partition the cache by version + triplet */
	if (h) {
		for (int i = 0; i < ast_search_memo_n; i++)
			if (ast_search_memo[i].hash == h) {
				/* hit: bump this permutation's refcount and persist it (also triggers
				 * the shared-disk eviction check). intersect gates with `searchable`
				 * (base + this build's opt-in knobs): a winner cached under a different -O
				 * base must not enable a fold gate this build disabled, but MUST be allowed
				 * to re-enable an opt-in knob (narrow-fixpoint) the search legitimately
				 * reaches here — `& base` alone would wrongly strip it. The refcount bump is
				 * applied by ast_search_disk_store -> ast_search_memo_add (refcount+1 >
				 * current), so the store sees a real change and rewrites the container. */
				MCC_TRACE("memo hit %s hash=%016llx gates=%llx&%llx->%llx refcount=%u->%u\n",
									funcname, (unsigned long long)h,
									(unsigned long long)ast_search_memo[i].gates,
									(unsigned long long)searchable,
									(unsigned long long)(ast_search_memo[i].gates & searchable),
									ast_search_memo[i].refcount, ast_search_memo[i].refcount + 1);
				ast_search_gates_set(ast_search_memo[i].gates & searchable);
				ast_search_disk_store(ast_search_memo[i].hash, ast_search_memo[i].gates,
															ast_search_memo[i].refcount + 1,
															ast_search_memo[i].score, ast_search_memo[i].tried);
				ast_arena_free(pristine);
				return;
			}
	}
	g0 = ast_graft_total;
	p0 = ast_promo_total;
	o0 = ast_opt_total;
	best = base;
	/* Candidate space = the subset lattice of `searchable` (the baseline-enabled
	 * fold gates plus this build's opt-in enablement knobs), driven by the combo
	 * substrate (roadmap M1 + "widen the search space"). items[i] is one AST_SG_*
	 * bit; combo_run enumerates every non-empty subset (subset mode) or every
	 * ordering (MCC_AST_SEARCH_ORDERED) of them. base (opt-in knobs off) is still
	 * scored first as the safe fallback. The fork pool consumes an explicit
	 * base-first submask gatelist over the same `searchable` set. */
	{
		AstGateMask items[64];
		int nitems = 0, b;
		for (b = 0; b < 64; b++)
			if (searchable & ((AstGateMask)1 << b))
				items[nitems++] = (AstGateMask)1 << b;
#if MCC_HOST_POSIX
		if (ast_search_threads_env) {
			AstGateMask sub = searchable;
			for (;;) {
				if (nc < AST_SEARCH_MAX_CAND)
					gatelist[nc++] = sub;
				else {
					MCC_TRACE("fork pool: submask space > %d, capped (budget)\n",
										AST_SEARCH_MAX_CAND);
					break; /* budget cap: don't silently overrun gatelist */
				}
				if (sub == 0)
					break;
				sub = (sub - 1) & searchable;
			}
			if (ast_search_pool(pristine, sym, faithful, gatelist, nc, saved_loc,
													saved_anon, &best, &best_score))
				goto search_done;
			best_score = -1; /* pool declined (too few cores / all forks failed) */
		}
#endif
		if (best_score < 0) {
			ComboSpec spec;
			ComboBest cbest;
			AstComboCtx cx;
			cx.pristine = pristine;
			cx.sym = sym;
			cx.faithful = faithful;
			cx.saved_loc = saved_loc;
			cx.saved_anon = saved_anon;
			cx.items = items;
			cx.tried = 0;
			cx.ord = 0;
			spec.nitems = nitems;
			spec.min_k = 1;
			spec.max_k = nitems;
			spec.ordered = ast_search_ordered_env ? 1 : 0;
			spec.budget = AST_SEARCH_MAX_CAND; /* budget-cap the enumerated candidates */
			spec.score = ast_search_combo_score;
			spec.user = &cx;
			/* base (the full enabled set) is the safe fallback and wins ties: score it
			 * first, then let combo_run search every non-empty subset/ordering and
			 * finally the empty (all-off) config; the strict-less keep-rule below only
			 * displaces base on a real improvement. */
			best = base;
			best_score = ast_search_score_one(pristine, sym, faithful, base, saved_loc,
																				saved_anon);
			/* Best-first frontier + forecast-driven ordering (est_cost_delta): when the
			 * vocabulary is large enough that the AST_SEARCH_MAX_CAND budget truncates the
			 * enumeration, combo_run's ascending-mask order can miss base's single-toggle
			 * neighbours (drop one enabled gate / add one opt-in knob) — the highest-value
			 * nearby configs. Score each explicitly first (so they are never crowded out by
			 * the cap), record its marginal delta, then REORDER items[] by that measured
			 * delta so the capped combo enumeration spends its candidates on the most-
			 * promising gate combinations first. Scheduling only (any order → correct
			 * winner); skipped when the whole space fits under the cap, so small-vocabulary
			 * searches are byte-identical. */
			if (((AstGateMask)1 << nitems) > AST_SEARCH_MAX_CAND) {
				long idelta[64];
				int i, j;
				for (i = 0; i < nitems; i++) {
					AstGateMask cand = base ^ items[i];
					long sc;
					if (ast_search_should_stop()) {
						idelta[i] = LONG_MAX; /* untried: sort last */
						continue;
					}
					sc = ast_search_score_one(pristine, sym, faithful, cand, saved_loc,
																		saved_anon);
					idelta[i] = (sc < 0) ? LONG_MAX : sc;
					if (sc >= 0 && (best_score < 0 || sc < best_score)) {
						best = cand;
						best_score = sc;
					}
				}
				/* insertion sort items[] by measured delta, most-improving (lowest) first */
				for (i = 1; i < nitems; i++) {
					AstGateMask ki = items[i];
					long kd = idelta[i];
					for (j = i - 1; j >= 0 && idelta[j] > kd; j--) {
						items[j + 1] = items[j];
						idelta[j + 1] = idelta[j];
					}
					items[j + 1] = ki;
					idelta[j + 1] = kd;
				}
			}
			if (nitems > COMBO_MAX)
				MCC_TRACE("combo enum clamped nitems=%d -> COMBO_MAX=%d (frontier scored "
									"every single-toggle; only combinations of the %d least-improving "
									"knobs are dropped)\n",
									nitems, COMBO_MAX, nitems - COMBO_MAX);
			if (combo_run(&spec, &cbest)) {
				AstGateMask g = 0;
				int i;
				for (i = 0; i < cbest.k; i++)
					g |= items[cbest.sel[i]];
				if (cbest.score >= 0 && (best_score < 0 || cbest.score < best_score)) {
					best = g;
					best_score = cbest.score;
				}
			}
			{
				long z = ast_search_score_one(pristine, sym, faithful, 0, saved_loc,
																			saved_anon);
				if (z >= 0 && best_score >= 0 && z < best_score) {
					best = 0;
					best_score = z;
				}
			}
			tried_mask = cx.tried;
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
	if (h) /* store folds the winner into the memo (memo_add) and rewrites the file.
					* score = the winning config's search score; tried = the bitmask of candidates
					* actually measured before the budget ran out (M3 blocker A progress fields).
					* A future resumable / unified search reads both to skip re-measuring. */
		ast_search_disk_store(h, best, 1, best_score, tried_mask);
	ast_arena_free(pristine);
}

#if MCC_HOST_WIN32 && defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O0")))
#endif
void ast_func_end(Sym *sym) {
	MCC_TRACE("%s\n", funcname);
	if (ast_try_active) {
		Section *ast_rsec = cur_text_section->reloc;
		addr_t ast_reloc1 = ast_rsec ? ast_rsec->data_offset : 0;
		ast_active = 0;
		ast_capture = 0;
		ast_fn_faithful = 0;
		ast_fn_tco = 0;
		uint64_t ast_fnh = ast_intention_hash(ast_cur, AST_NONE);
		ast_intention_acc = ast_intention_acc * 0x100000001b3u ^ ast_fnh;
		if (ast_hash_out && ast_hash_out[0]) {
			FILE *ast_hf = fopen(ast_hash_out, "a");
			if (ast_hf) {
				fprintf(ast_hf, "%s %016llx\n", funcname,
								(unsigned long long)ast_fnh);
				fclose(ast_hf);
			}
		}
		if (ast_cost_env)
			ast_fn_cost(ast_cur, funcname);
		if (ast_bitflag_report_env && !ast_search_worker)
			ast_bf_report(ast_cur, funcname);
		int ast_sv_tmpl = ast_templates_env, ast_sv_promo = ast_promote_env,
				ast_sv_inl = ast_inline_env;
		if (ast_fncfg_n) {
			int fi = ast_fncfg_find(funcname);
			if (fi >= 0) {
				ast_templates_env = ast_fncfg[fi].tmpl;
				ast_promote_env = ast_fncfg[fi].promo;
				ast_inline_env = ast_fncfg[fi].inl;
			}
		}
		if (ast_replay_ok(ast_cur)) {
			int orig_ind = ind, orig_rsym = rsym;
			int body_len = orig_ind - ast_body_ind_sv;
			unsigned char *orig = mcc_malloc(body_len > 0 ? body_len : 1);
			memcpy(orig, cur_text_section->data + ast_body_ind_sv, body_len);

			addr_t rel_len = ast_reloc1 - ast_reloc0_sv;
			unsigned char *orig_rel = mcc_malloc(rel_len > 0 ? rel_len : 1);
			if (rel_len)
				memcpy(orig_rel, ast_rsec->data + ast_reloc0_sv, rel_len);

			ind = ast_body_ind_sv;
			rsym = 0;
			if (ast_rsec)
				ast_rsec->data_offset = ast_reloc0_sv;
			nocode_wanted = 0;
			unsigned char ast_sv_warn = mcc_state->warn_none;
			mcc_state->warn_none = 1;
			ast_rp_bsym = ast_rp_csym = NULL;
			ast_tmpl_folds = 0;
			if (ast_templates_env)
				ast_run_templates(ast_cur);
			ast_fconst_i = 0;
			ast_locrec_i = 0;
			ast_replaying = 1;
			ast_rp_switch = NULL;
			ast_rp_nlabel = 0;
			Sym *ast_saved_free = sym_free_first;
			sym_free_first = NULL;
			int saved_loc = loc, saved_anon = anon_sym;
			Section *rsec2 = cur_text_section->reloc;
			volatile int faithful = 0;
			int promoted = 0;
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
			jmp_buf ast_outer_jmp;
			int ast_outer_en = mcc_state->error_set_jmp_enabled;
			int ast_saved_nberr = mcc_state->nb_errors;
			void (*ast_sv_efunc)(void *, const char *) = mcc_state->error_func;
			void *ast_sv_eopaque = mcc_state->error_opaque;
			int ast_saved_floor = stk_data_floor;
			memcpy(ast_outer_jmp, mcc_state->error_jmp_buf, sizeof(jmp_buf));
			mcc_state->error_func = ast_error_sink;
			stk_data_floor = nb_stk_data;
			if (setjmp(mcc_state->error_jmp_buf) == 0) {
				mcc_state->error_set_jmp_enabled = 1;
				ast_promo_n = 0;
				ast_pinned_regs = 0;
				ast_replay_body(ast_cur);
				ast_replaying = 0;

				addr_t new_rel = rsec2 ? rsec2->data_offset : 0;
				int new_len = ind - ast_body_ind_sv;
				faithful = new_len == body_len &&
									 memcmp(cur_text_section->data + ast_body_ind_sv, orig, body_len) == 0 &&
									 new_rel - ast_reloc0_sv == rel_len &&
									 (rel_len == 0 ||
										memcmp(rsec2->data + ast_reloc0_sv, orig_rel, rel_len) == 0);
				ast_fn_faithful = faithful;

				ast_ltemp_cur = saved_loc;
				ast_ltemp_n = 0;
				AstGateMask ast_search_sv_gates = ast_search_gates_now();
				if (faithful && ast_search_env && ast_search_seconds > 0)
					ast_search_select(sym, faithful, saved_loc, saved_anon);
				{
					int sf[AST_STRAT_COUNT];
					for (int si = 0; si < AST_STRAT_COUNT; si++)
						sf[si] = faithful && *ast_strategies[si].gate
												 ? ast_strategies[si].apply(ast_cur, sym)
												 : 0;
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
				}
				ast_search_gates_set(ast_search_sv_gates);
				int do_bfold = bfolds > 0;
				int do_ident = idents > 0;
				int do_narrow = narrows > 0;
				int do_cprop = cprops > 0;
				int do_cse = cses > 0;
				int do_licm = licms > 0;
				int do_dse = dses > 0;
				int do_sccp = sccps > 0;
				int do_jt = jts > 0;
				int do_bf = bfs > 0;
				int do_sethi = sethis > 0;
				int do_tco = tcos > 0;
				int do_inline = faithful && !do_tco && ast_has_graftable_call(ast_cur);
				ast_no_callful_promo = do_inline;
				int do_promote = faithful && !do_tco && ast_promote_env && ast_plan_promotion(ast_cur) > 0;
				ast_no_callful_promo = 0;
				MCC_TRACE("branch %s faithful=%d inline=%d promote=%d tco=%d\n",
									funcname, faithful, do_inline, do_promote, do_tco);
				if (do_promote && ast_promo_limit >= 0 && ast_promo_total >= ast_promo_limit) {
					do_promote = 0;
					ast_promo_n = 0;
				}
				if (do_promote)
					ast_promo_total++;
				if (ast_opt_limit >= 0 && ast_opt_total >= ast_opt_limit) {
					do_inline = do_promote = do_bfold = do_ident = do_cprop = 0;
					do_cse = do_licm = do_dse = do_sccp = do_jt = do_bf = do_sethi = do_tco = 0;
					do_narrow = 0;
					ast_promo_n = 0;
				}
				if (do_inline || do_promote || do_bfold || do_ident || do_cprop ||
						do_cse || do_licm || do_dse || do_sccp || do_jt || do_bf || do_sethi ||
						do_tco || do_narrow)
					ast_opt_total++;
				if (faithful && !do_inline && !do_promote && !do_bfold && !do_ident &&
						!do_cprop && !do_cse && !do_licm && !do_dse && !do_sccp && !do_jt &&
						!do_bf && !do_sethi && !do_tco && !do_narrow)
					loc = saved_loc;
				if (do_inline || do_promote || do_bfold || do_ident || do_cprop ||
						do_cse || do_licm || do_dse || do_sccp || do_jt || do_bf || do_sethi ||
						do_tco || do_narrow) {
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
										do_tco || do_narrow || (ui))                                 \
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
		for (int pi = 0; pi < ast_promo_n; pi++)                                      \
			ast_pinned_regs |= (1u << ast_promo_regpool_at(pi));                       \
		if (do_promote)                                                              \
			ast_promo_entry_init();                                                    \
		ast_loc_low = loc;                                                            \
		ast_replay_body(ast_cur);                                                     \
		if (ast_loc_low < loc)                                                        \
			loc = ast_loc_low;                                                         \
		ast_replaying = 0;                                                            \
		ast_inline_active = 0;                                                        \
		ast_pinned_regs = 0;                                                          \
	} while (0)
					int pf_best = do_inline;
					if (ast_perfn_inproc_env && do_inline) {
						Sym *pf_symmark = local_stack;
						int pf_bestlen = -1;
						for (int ui = 0; ui <= 1; ui++) {
							AST_PF_EMIT(ui);
							int len = ind - ast_body_ind_sv;
							if (pf_bestlen < 0 || len < pf_bestlen) {
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
			} else {
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
			if (!faithful) {
				memcpy(cur_text_section->data + ast_body_ind_sv, orig, body_len);
				if (rel_len)
					memcpy(ast_rsec->data + ast_reloc0_sv, orig_rel, rel_len);
				if (rsec2)
					rsec2->data_offset = ast_reloc1;
				ind = orig_ind;
				rsym = orig_rsym;
				loc = saved_loc;
			} else if (ast_replay_dump) {
				char buf[512];
				ast_dump(ast_cur, ast_root(ast_cur), buf, sizeof buf);
				fprintf(stderr, "[ast-replay] %s\n%s", funcname, buf);
				if (ast_templates_env)
					fprintf(stderr, "[ast-template] const-fold %d %s\n",
									ast_tmpl_folds, funcname);
				if (bfolds)
					fprintf(stderr, "[ast-bfold] %d %s\n", bfolds, funcname);
				if (idents)
					fprintf(stderr, "[ast-ident] %d %s\n", idents, funcname);
				if (narrows)
					fprintf(stderr, "[ast-narrow] %d %s\n", narrows, funcname);
				if (cprops)
					fprintf(stderr, "[ast-cprop] %d %s\n", cprops, funcname);
				if (cses)
					fprintf(stderr, "[ast-cse] %d %s\n", cses, funcname);
				if (licms)
					fprintf(stderr, "[ast-licm] %d %s\n", licms, funcname);
				if (dses)
					fprintf(stderr, "[ast-dse] %d %s\n", dses, funcname);
				if (sccps)
					fprintf(stderr, "[ast-sccp] %d %s\n", sccps, funcname);
				if (jts)
					fprintf(stderr, "[ast-jt] %d %s\n", jts, funcname);
				if (bfs)
					fprintf(stderr, "[ast-bitflag] %d %s\n", bfs, funcname);
				if (sethis)
					fprintf(stderr, "[ast-sethi] %d %s\n", sethis, funcname);
				if (tcos)
					fprintf(stderr, "[ast-tco] %d %s\n", tcos, funcname);
				if (promoted)
					fprintf(stderr, "[ast-promote] %d %s\n", promoted, funcname);
			}
			mcc_free(orig);
			mcc_free(orig_rel);
		}
		int keep_inline = ast_fn_faithful && ast_inline_retain(ast_cur, sym);
		int keep_reemit = ast_fn_faithful && ast_reemit_retain(ast_cur, sym);
		if (!keep_inline && !keep_reemit)
			ast_arena_free(ast_cur);
		ast_cur = NULL;
		ast_sym_defer_on = 0;
		if (keep_inline || keep_reemit) {
			ast_sym_deferred = NULL;
		} else {
			while (ast_sym_deferred) {
				Sym *nx = ast_sym_deferred->next;
#ifndef MCC_SYM_DEBUG
				ast_sym_deferred->next = sym_free_first;
				sym_free_first = ast_sym_deferred;
#else
				mcc_free(ast_sym_deferred);
#endif
				ast_sym_deferred = nx;
			}
		}
		ast_templates_env = ast_sv_tmpl;
		ast_promote_env = ast_sv_promo;
		ast_inline_env = ast_sv_inl;
	}
}

void ast_func_epilog(void) {
	ast_promo_exit_restore();
	ast_promo_n = 0;
	ast_promo_callful = 0;
}

static void ast_reemit(Sym *sym, AstArena *ast) {
	struct scope f = {0};
	cur_scope = root_scope = &f;
	nocode_wanted = 0;
	cur_text_section = text_section;
	ind = cur_text_section->data_offset;
	if (sym->a.aligned) {
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
		for (AstLocal n = 0; n < nn; n++) {
			uint16_t k = ast_kind(ast, n);
			if (k != AST_Ref && k != AST_Literal)
				continue;
			int r = ast_op(ast, n);
			if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
				int off = (int)ast_ival(ast, n);
				if (off < loc)
					loc = off;
			}
		}
	}

	Sym *saved_free = sym_free_first;
	sym_free_first = NULL;
	ast_cur = ast;
	ast_replaying = 1;
	ast_rp_switch = NULL;
	ast_rp_nlabel = 0;
	ast_rp_label_floor = 0;
	ast_rp_bsym = ast_rp_csym = NULL;
	ast_fconst_i = 0;
	ast_locrec_i = 0;
	ast_promo_n = 0;
	ast_pinned_regs = 0;
	ast_inline_active = 1;
	ast_graft_budget = ast_graft_budget_max;
	ast_replay_body(ast);
	ast_inline_active = 0;
	ast_replaying = 0;
	sym_free_first = saved_free;
	ast_cur = NULL;

	gsym(rsym);
	ast_promo_n = 0;
	nocode_wanted = 0;
	gfunc_epilog();
	put_extern_sym(sym, cur_text_section, new_ind, ind - new_ind);
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
		fprintf(stderr, "[ast-inline] re-emitted %s (forward inline)\n",
						get_tok_str(sym->v, NULL));
}

void ast_reemit_forward_inlines(void) {
	for (int i = 0; i < ast_reemit_n; i++)
		if (ast_reemit_has_forward(&ast_reemit_pool[i]))
			ast_reemit(ast_reemit_pool[i].sym, ast_reemit_pool[i].ast);
}

#undef gjmp_addr
#undef gjmp

#endif

#endif
