#if defined(CONFIG_AST) && CONFIG_AST

#include "mccast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef AST_ASSERT
#include <assert.h>
#define AST_ASSERT(x) assert(x)
#endif

/* The amalgamated build poisons malloc/realloc/free (src/mcc.h) so that the
 * compiler proper routes through its tracked allocators. The AST library is a
 * standalone side-channel with its own lifetime, so it un-poisons them for the
 * span of this file, exactly as mcccst.c does. */
#pragma push_macro("malloc")
#pragma push_macro("realloc")
#pragma push_macro("free")
#undef malloc
#undef realloc
#undef free

/* Structure-of-arrays node arena. One AstArena per function (D-c: minimal, no
 * hash-consing until virtual-inline lands at Mid). Node 0 is the first node
 * created (typically the TranslationUnit or the function's entry BasicBlock)
 * and doubles as the root. */
struct AstArena {
	uint16_t *kind;
	AstLocal *parent;
	AstLocal *first_child;
	AstLocal *last_child;
	AstLocal *next_sib;
	uint32_t *nchild;

	int32_t *op;       /* token value / cast selector / terminator tag */
	int32_t *type_t;   /* CType.t bit-field (opaque to the pure lib)   */
	uint64_t *type_ref; /* opaque Sym* for pointer/aggregate types      */
	uint64_t *ival;    /* literal integer payload / block target       */
	uint64_t *fbits;   /* literal float payload (IEEE bits)            */
	uint64_t *sym;     /* opaque Sym* the node refers to (Ref/Invoke)  */
	uint64_t *cst;     /* CST provenance id (§14)                      */

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

void ast_set_op(AstArena *a, AstLocal n, int op) { a->op[n] = op; }
void ast_set_type(AstArena *a, AstLocal n, int type_t, uint64_t type_ref) {
	a->type_t[n] = type_t;
	a->type_ref[n] = type_ref;
}
void ast_set_ival(AstArena *a, AstLocal n, uint64_t v) { a->ival[n] = v; }
void ast_set_fbits(AstArena *a, AstLocal n, uint64_t bits) { a->fbits[n] = bits; }
void ast_set_sym(AstArena *a, AstLocal n, uint64_t sym) { a->sym[n] = sym; }
void ast_set_cst(AstArena *a, AstLocal n, uint64_t cst_id) { a->cst[n] = cst_id; }

uint16_t ast_kind(const AstArena *a, AstLocal n) { return a->kind[n]; }
int ast_op(const AstArena *a, AstLocal n) { return a->op[n]; }
int ast_type_t(const AstArena *a, AstLocal n) { return a->type_t[n]; }
uint64_t ast_type_ref(const AstArena *a, AstLocal n) { return a->type_ref[n]; }
uint64_t ast_ival(const AstArena *a, AstLocal n) { return a->ival[n]; }
uint64_t ast_fbits(const AstArena *a, AstLocal n) { return a->fbits[n]; }
uint64_t ast_sym(const AstArena *a, AstLocal n) { return a->sym[n]; }
uint64_t ast_cst(const AstArena *a, AstLocal n) { return a->cst[n]; }

AstLocal ast_parent(const AstArena *a, AstLocal n) { return a->parent[n]; }
AstLocal ast_first_child(const AstArena *a, AstLocal n) { return a->first_child[n]; }
AstLocal ast_last_child(const AstArena *a, AstLocal n) { return a->last_child[n]; }
AstLocal ast_next_sib(const AstArena *a, AstLocal n) { return a->next_sib[n]; }
uint32_t ast_nchild(const AstArena *a, AstLocal n) { return a->nchild[n]; }
AstLocal ast_count(const AstArena *a) { return a->count; }
AstLocal ast_root(const AstArena *a) { return a->count ? 0 : AST_NONE; }

AstLocal ast_child(const AstArena *a, AstLocal n, uint32_t i) {
	AstLocal c = a->first_child[n];
	while (c != AST_NONE && i--)
		c = a->next_sib[c];
	return c;
}

static const char *const kind_names[AST_KIND_COUNT] = {
	"TranslationUnit", "BasicBlock", "If", "Jump", "Return",
	"Ref", "Literal", "Load", "Store", "Unary",
	"Binary", "Convert", "Invoke", "InitList", "Poison",
};

const char *ast_kind_name(uint16_t kind) {
	if (kind >= AST_KIND_COUNT)
		return "?";
	return kind_names[kind];
}

/* A single printable token for the node's operator field. Binary/Unary ops
 * carry the parser's token value; for the common ASCII operators that value is
 * the character itself, so we print it directly and fall back to a numeric tag
 * for multi-char tokens (the pure harness pins only the ASCII ones). */
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

/* Structural invariants: parent/child links are mutually consistent, nchild
 * matches the sibling chain, and every reachable-from-root node's kind is in
 * range. Returns 0 on success, -1 on failure (msg filled). */
int ast_validate(const AstArena *a, char *msg, size_t msgcap) {
#define AST_FAIL(m)                          \
	do {                                       \
		if (msg)                                 \
			snprintf(msg, msgcap, "%s", m);        \
		return -1;                               \
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

#pragma pop_macro("malloc")
#pragma pop_macro("realloc")
#pragma pop_macro("free")

#endif /* CONFIG_AST */
