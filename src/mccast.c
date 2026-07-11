#if MCC_CONFIG_OPTIMIZER && (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))

#include "mccast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

AstArena *ast_arena_new(void) {
	AstArena *a = calloc(1, sizeof *a);
	return a;
}

void ast_arena_reset(AstArena *a) {
	a->count = 0;
}

void ast_arena_free(AstArena *a) {
	if (!a)
		return;
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
	return n;
}

void ast_add_child(AstArena *a, AstLocal parent, AstLocal child) {
	AST_ASSERT(parent < a->count && child < a->count);
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
	a->kind[n] = kind;
}
void ast_clear_children(AstArena *a, AstLocal n) {
	a->first_child[n] = AST_NONE;
	a->last_child[n] = AST_NONE;
	a->nchild[n] = 0;
}

void ast_set_op(AstArena *a, AstLocal n, int op) {
	a->op[n] = op;
}
void ast_set_type(AstArena *a, AstLocal n, int type_t, uint64_t type_ref) {
	a->type_t[n] = type_t;
	a->type_ref[n] = type_ref;
}
void ast_set_ival(AstArena *a, AstLocal n, uint64_t v) {
	a->ival[n] = v;
}
void ast_set_fbits(AstArena *a, AstLocal n, uint64_t bits) {
	a->fbits[n] = bits;
}
void ast_set_sym(AstArena *a, AstLocal n, uint64_t sym) {
	a->sym[n] = sym;
}
void ast_set_cst(AstArena *a, AstLocal n, uint64_t cst_id) {
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

#ifdef MCC_INTERNAL

/* Like the parser in mccgen.c, the capture/replay engine must emit its
   jumps through the _acs wrappers so dead code after an unconditional
   jump stays suppressed (CODE_OFF). */
#define gjmp_addr gjmp_addr_acs
#define gjmp gjmp_acs

/* ==================================================================== */
/* Compiler integration: the capture hooks mccgen.c drives while it     */
/* parses a function body, and the replay/promote/inline drivers that   */
/* re-emit the body from the recorded intention tree.                   */
/* ==================================================================== */

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
static int ast_bitflag_env;
static int ast_bitflag_min;
static int ast_cprop_join_env;
static int ast_cse_join_env;
static int ast_call_window_env;
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

unsigned ast_pinned_regs;
int ast_func_has_asm;

int ast_alloc_loc(int size, int align) {
	if (ast_replaying && ast_locrec_i < ast_locrec_n) {
		loc = ast_locrec[ast_locrec_i++];
		return loc;
	}
	loc = (loc - size) & -align;
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
	ast_sethi_env = ast_env_gate("MCC_AST_SETHI", 0);
	ast_bitflag_env = ast_env_gate("MCC_AST_BITFLAG", 0);
	ast_bitflag_min = ast_env_int("MCC_AST_BITFLAG", 5);
	if (ast_bitflag_min < 3)
		ast_bitflag_min = 5;
	ast_cprop_join_env = ast_env_gate("MCC_AST_CPROP_JOIN", 0);
	ast_cse_join_env = ast_env_gate("MCC_AST_CSE_JOIN", 0);
	ast_call_window_env = ast_env_gate("MCC_AST_CALL_WINDOW", 0);
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
static int ast_promo_off[AST_PROMO_MAX];
static int ast_promo_typ[AST_PROMO_MAX];
static int ast_promo_reg[AST_PROMO_MAX];
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
		/* func_vla_arg evaluates VLA-parameter size expressions (and their
		   side effects) in the prologue, which is outside the captured body,
		   so a grafted call would silently skip them. */
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
#define AST_INLINE_MAX_DEPTH 8
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
	if (ast_inline_depth >= AST_INLINE_MAX_DEPTH)
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

static int ast_local_is_readonly(AstArena *a, int off) {
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

static int ast_plan_promotion(AstArena *a) {
	ast_promo_n = 0;
	ast_promo_callful = 0;
	if (!ast_promote_env || ast_func_has_asm)
		return 0;
	AstLocal nn = ast_count(a);
	int has_call = 0, has_vla = 0;
	for (AstLocal n = 0; n < nn; n++) {
		uint16_t k = ast_kind(a, n);
		if (k == AST_Invoke)
			has_call = 1;
		else if (k == AST_Unary && ast_op(a, n) == AST_OP_VLA)
			has_vla = 1;
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
		cweight[j] = 0;
	ast_promo_weigh(a, ast_root(a), 0, coff, nc, cweight);
	ast_promo_callful = has_call;
	const int *gp_pool = has_call ? ast_promo_callee : ast_promo_caller;
	int gp_max = has_call ? (int)(sizeof ast_promo_callee / sizeof *ast_promo_callee)
												: (int)(sizeof ast_promo_caller / sizeof *ast_promo_caller);
	int xmm_max = has_call ? 0 : (int)(sizeof ast_promo_xmm / sizeof *ast_promo_xmm);
	int gp_n = 0, xmm_n = 0;
	for (;;) {
		int best = -1;
		for (int j = 0; j < nc; j++) {
			if (cpoison[j] || coff[j] >= 0)
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

/* The saved pinned registers live in rbp-relative frame slots, not in
   push slots below the frame: on PE the outgoing-call area (win64 home
   space + stack args) is rsp-relative at the frame bottom, so anything
   pushed below the frame sits inside it and gets scribbled by callees. */
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

static int ast_ident_pure(AstArena *a, AstLocal n) {
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

static int ast_ident_same(const AstArena *a, AstLocal x, AstLocal y) {
	if (a->kind[x] != a->kind[y] || a->op[x] != a->op[y] ||
			a->type_t[x] != a->type_t[y] || a->type_ref[x] != a->type_ref[y] ||
			a->ival[x] != a->ival[y] || a->fbits[x] != a->fbits[y] ||
			a->sym[x] != a->sym[y] || a->nchild[x] != a->nchild[y])
		return 0;
	AstLocal cx = a->first_child[x], cy = a->first_child[y];
	for (; cx != AST_NONE; cx = a->next_sib[cx], cy = a->next_sib[cy])
		if (!ast_ident_same(a, cx, cy))
			return 0;
	return 1;
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

#define AST_CPROP_MAX 128
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

static int ast_cprop_escapes(AstArena *a, int off) {
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

static int ast_cprop_safe(AstArena *a, AstLocal n) {
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
		if (ast_cprop_kn >= AST_CPROP_MAX)
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

static void ast_cprop_stmts(AstArena *a, AstLocal bb) {
	if (bb == AST_NONE || ast_kind(a, bb) != AST_BasicBlock)
		return;
	ast_cprop_vis[bb] = 1;
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
				continue;
			}
			AstCpropState in, tout;
			ast_cprop_state_save(&in);
			ast_cprop_stmts(a, tb);
			ast_cprop_state_save(&tout);
			ast_cprop_state_load(&in);
			ast_cprop_stmts(a, eb);
			ast_cprop_state_meet(&tout);
		} else if (k == AST_If && ast_op(a, s) >= 2 && ast_op(a, s) <= 6) {
			for (int i = 0; i < ast_cprop_kn;)
				if (ast_licm_written(a, s, ast_cprop_koff[i]))
					ast_cprop_kill(ast_cprop_koff[i]);
				else
					i++;
			if (ast_sccp_has_label(a, s)) {
				ast_cprop_kn = 0;
				continue;
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

static int ast_sccp_has_label(AstArena *a, AstLocal n) {
	if (n == AST_NONE)
		return 0;
	if (ast_kind(a, n) == AST_Jump && ast_op(a, n) == 4)
		return 1;
	for (AstLocal c = ast_first_child(a, n); c != AST_NONE; c = ast_next_sib(a, c))
		if (ast_sccp_has_label(a, c))
			return 1;
	return 0;
}

static int ast_sccp_run(AstArena *a) {
	ast_sccp_folds = 0;
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
		ast_sccp_folds++;
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

#define AST_TCO_MAXP 16
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
		if (v < TOK_IDENT || np >= AST_TCO_MAXP)
			return 0;
		Sym *ls = sym_find(v);
		if (!ls)
			return 0;
		if ((ls->r & VT_VALMASK) != VT_LOCAL || !(ls->r & VT_LVAL) || (ls->r & VT_SYM))
			return 0;
		int t = ls->type.t;
		if (!ast_ident_intt(t) || (t & VT_VOLATILE) || (t & (VT_ARRAY | VT_VLA)))
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

#define AST_CSE_MAX 64
static AstLocal ast_cse_expr[AST_CSE_MAX];
static AstLocal ast_cse_ref[AST_CSE_MAX];
static int ast_cse_off[AST_CSE_MAX];
static int ast_cse_n;
static int ast_cse_folds;

static int ast_cse_regpure(AstArena *a, AstLocal n) {
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

static int ast_cse_try_match(AstArena *a, AstLocal n) {
	for (int i = 0; i < ast_cse_n; i++) {
		if (ast_cse_expr[i] == n)
			continue;
		if (ast_ident_same(a, ast_cse_expr[i], n)) {
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
					nodes, maxdepth, calls,
					(long)nodes * (maxdepth + 1) * (calls + 1));
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
		if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LOR)
			ast_bf_folds += ast_bf_try_lor(a, n);
	nn = ast_count(a);
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_Binary && ast_op(a, n) == TOK_LAND)
			ast_bf_folds += ast_bf_try_land(a, n);
	return ast_bf_folds;
}

/* Sethi-Ullman operand ordering (§35, first increment). For a commutative
   binary node whose two operands are both side-effect-free, evaluate the
   operand that needs more registers first — emitted child-order is evaluation
   order in the replay, so put the higher Sethi-Ullman-numbered operand first.
   Commuting a side-effect-free pair is always value-preserving: +,*,&,|,^ are
   commutative for every operand type (IEEE + and * commute bit-exactly — this
   is commutativity, not associativity; &,|,^ are integer-only), and
   ast_cse_regpure guarantees neither operand has an observable evaluation
   order. So this needs no dataflow proof and no result-type restriction. */
static int ast_sethi_folds;

static int ast_sethi_commutative(int op) {
	return op == '+' || op == '*' || op == '&' || op == '|' || op == '^';
}

/* An operand whose root is a comparison/logical op leaves a VT_CMP on the
   vstack for the parent to consume. The replay emitter is order-sensitive
   there (the FIX.md/§30 lesson: a leaf emitted after the compare clobbers the
   flags before the parent op consumes them — exactly why ast_bf_build fixes
   its `bit & guard` order). So SETHI must not reorder an operand that produces
   a VT_CMP; skip the swap when either side is comparison/logical-rooted. */
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
	if (ast_kind(a, n) != AST_Binary || ast_nchild(a, n) != 2)
		return 1;
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
				if (!ast_cprop_escapes(a, off) && ast_cse_n < AST_CSE_MAX &&
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
				if (!ast_cprop_escapes(a, off) && ast_cse_n < AST_CSE_MAX &&
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
		return ast_cse_folds;
	}
	for (AstLocal n = 0; n < nn; n++)
		if (ast_kind(a, n) == AST_BasicBlock)
			ast_cse_block(a, n);
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

/* The replay error trap below is the setjmp that miscompiles under
   mingw-gcc SEH at -O2 (see EXCESS.md); keep this frame unoptimized
   on that host. */
#if MCC_HOST_WIN32 && defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O0")))
#endif
void ast_func_end(Sym *sym) {
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
		if (ast_bitflag_env && !ast_search_worker)
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

				bfolds = faithful && ast_templates_env ? ast_bfold_run(ast_cur) : 0;
				idents = faithful && ast_templates_env ? ast_ident_run(ast_cur) : 0;
				cprops = faithful && ast_templates_env ? ast_cprop_run(ast_cur) : 0;
				cses = faithful && ast_templates_env ? ast_cse_run(ast_cur) : 0;
				licms = faithful && ast_templates_env ? ast_licm_folds : 0;
				dses = faithful && ast_templates_env ? ast_dse_run(ast_cur) : 0;
				sccps = faithful && ast_templates_env ? ast_sccp_run(ast_cur) : 0;
				jts = faithful && ast_templates_env ? ast_jt_run(ast_cur) : 0;
				bfs = faithful && ast_bitflag_env ? ast_bf_run(ast_cur) : 0;
				sethis = faithful && ast_sethi_env ? ast_sethi_run(ast_cur) : 0;
				tcos = faithful && ast_templates_env ? ast_tco_run(ast_cur, sym) : 0;
				int do_bfold = bfolds > 0;
				int do_ident = idents > 0;
				int do_cprop = cprops > 0;
				int do_cse = cses > 0;
				int do_licm = licms > 0;
				int do_dse = dses > 0;
				int do_sccp = sccps > 0;
				int do_jt = jts > 0;
				int do_bf = bfs > 0;
				int do_sethi = sethis > 0;
				int do_tco = tcos > 0;
				/* A tail-self-call rewritten into a back-edge loop keeps all
				   params/locals in their stack slots across the iteration; pinning
				   a param to a register (promotion) or splicing this body into a
				   caller (inline) would break the back-edge, so disable both here. */
				int do_inline = faithful && !do_tco && ast_has_graftable_call(ast_cur);
				/* Tier-4 inline and call-ful Tier-3 promotion both claim the
				   callee-saved bank (RBX/R12-R15); combining them in one function
				   corrupts a pin across the graft. When a function will graft,
				   restrict promotion to the call-free (caller-saved) pool. */
				ast_no_callful_promo = do_inline;
				int do_promote = faithful && !do_tco && ast_promote_env && ast_plan_promotion(ast_cur) > 0;
				ast_no_callful_promo = 0;
				if (do_promote && ast_promo_limit >= 0 && ast_promo_total >= ast_promo_limit) {
					do_promote = 0;
					ast_promo_n = 0;
				}
				if (do_promote)
					ast_promo_total++;
				if (ast_opt_limit >= 0 && ast_opt_total >= ast_opt_limit) {
					do_inline = do_promote = do_bfold = do_ident = do_cprop = 0;
					do_cse = do_licm = do_dse = do_sccp = do_jt = do_bf = do_sethi = do_tco = 0;
					ast_promo_n = 0;
				}
				if (do_inline || do_promote || do_bfold || do_ident || do_cprop ||
						do_cse || do_licm || do_dse || do_sccp || do_jt || do_bf || do_sethi ||
						do_tco)
					ast_opt_total++;
				if (faithful && !do_inline && !do_promote && !do_bfold && !do_ident &&
						!do_cprop && !do_cse && !do_licm && !do_dse && !do_sccp && !do_jt &&
						!do_bf && !do_sethi && !do_tco)
					loc = saved_loc;
				if (do_inline || do_promote || do_bfold || do_ident || do_cprop ||
						do_cse || do_licm || do_dse || do_sccp || do_jt || do_bf || do_sethi ||
						do_tco) {
					ind = ast_body_ind_sv;
					rsym = 0;
					if (ast_rsec)
						ast_rsec->data_offset = ast_reloc0_sv;
					nocode_wanted = 0;
					loc = saved_loc;
					anon_sym = saved_anon;
					ast_fconst_i = (do_bfold || do_ident || do_cprop || do_cse || do_licm || do_dse || do_sccp || do_jt || do_bf || do_sethi || do_tco || do_inline) ? ast_fconst_n : 0;
					ast_locrec_i = 0;
					ast_replaying = 1;
					ast_rp_switch = NULL;
					ast_rp_nlabel = 0;
					ast_rp_bsym = ast_rp_csym = NULL;
					ast_pinned_regs = 0;
					ast_inline_active = do_inline;
					ast_graft_budget = ast_graft_budget_max;
					for (int pi = 0; pi < ast_promo_n; pi++)
						ast_pinned_regs |= (1u << ast_promo_regpool_at(pi));
					if (do_promote)
						ast_promo_entry_init();
					ast_replay_body(ast_cur);
					ast_replaying = 0;
					ast_inline_active = 0;
					ast_pinned_regs = 0;
					promoted = ast_promo_n;
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
				ast_sym_deferred->next = sym_free_first;
				sym_free_first = ast_sym_deferred;
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

#endif /* MCC_INTERNAL */

#endif
