#include "mccast.c"

#include <stdio.h>
#include <string.h>

static int g_failures;
static int g_checks;

#define CHECK(cond, msg)                                            \
	do {                                                              \
		g_checks++;                                                     \
		if (!(cond)) {                                                  \
			fprintf(stderr, "FAIL %s:%d: %s\n", __func__, __LINE__, msg); \
			g_failures++;                                                 \
		}                                                               \
	} while (0)

/* Build `2 + 3 * 4` as an intention tree and check geometry. */
static AstLocal build_expr(AstArena *a) {
	AstLocal add = ast_node(a, AST_Binary);
	ast_set_op(a, add, '+');
	AstLocal two = ast_node(a, AST_Literal);
	ast_set_ival(a, two, 2);
	AstLocal mul = ast_node(a, AST_Binary);
	ast_set_op(a, mul, '*');
	AstLocal three = ast_node(a, AST_Literal);
	ast_set_ival(a, three, 3);
	AstLocal four = ast_node(a, AST_Literal);
	ast_set_ival(a, four, 4);
	ast_add_child(a, mul, three);
	ast_add_child(a, mul, four);
	ast_add_child(a, add, two);
	ast_add_child(a, add, mul);
	return add;
}

static void suite_arena(void) {
	AstArena *a = ast_arena_new();
	AstLocal add = build_expr(a);

	CHECK(ast_root(a) == add, "root is the first node created");
	CHECK(ast_count(a) == 5, "five nodes");
	CHECK(ast_kind(a, add) == AST_Binary, "add is Binary");
	CHECK(ast_op(a, add) == '+', "add op is +");
	CHECK(ast_nchild(a, add) == 2, "add has two children");

	AstLocal lhs = ast_child(a, add, 0);
	AstLocal rhs = ast_child(a, add, 1);
	CHECK(ast_kind(a, lhs) == AST_Literal, "lhs is Literal");
	CHECK(ast_ival(a, lhs) == 2, "lhs value is 2");
	CHECK(ast_kind(a, rhs) == AST_Binary, "rhs is Binary");
	CHECK(ast_op(a, rhs) == '*', "rhs op is *");
	CHECK(ast_parent(a, rhs) == add, "rhs parent is add");
	CHECK(ast_parent(a, lhs) == add, "lhs parent is add");

	CHECK(ast_first_child(a, rhs) != AST_NONE, "mul has a first child");
	CHECK(ast_ival(a, ast_child(a, rhs, 0)) == 3, "mul[0] == 3");
	CHECK(ast_ival(a, ast_child(a, rhs, 1)) == 4, "mul[1] == 4");
	CHECK(ast_next_sib(a, ast_last_child(a, rhs)) == AST_NONE, "last child ends chain");

	ast_arena_free(a);
}

static void suite_validate(void) {
	AstArena *a = ast_arena_new();
	build_expr(a);
	char msg[64];
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "well-formed tree validates");

	ast_arena_reset(a);
	CHECK(ast_count(a) == 0, "reset empties the arena");
	CHECK(ast_root(a) == AST_NONE, "reset arena has no root");

	/* Reuse after reset must produce a fresh, valid tree. */
	build_expr(a);
	CHECK(ast_count(a) == 5, "rebuild after reset");
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "rebuilt tree validates");
	ast_arena_free(a);
}

static void suite_dump(void) {
	AstArena *a = ast_arena_new();
	build_expr(a);
	char buf[256];
	size_t need = ast_dump(a, ast_root(a), NULL, 0);
	CHECK(need > 0, "dump reports a size");
	size_t got = ast_dump(a, ast_root(a), buf, sizeof buf);
	CHECK(got == need, "sized and buffered dump agree");

	const char *want =
		"Binary +\n"
		"  Literal 2\n"
		"  Binary *\n"
		"    Literal 3\n"
		"    Literal 4\n";
	CHECK(strcmp(buf, want) == 0, "dump matches the expected intention tree");

	ast_arena_free(a);
}

/* A minimal function shell: an entry BasicBlock terminated by Return(value). */
static void suite_cfg(void) {
	AstArena *a = ast_arena_new();
	AstLocal bb = ast_node(a, AST_BasicBlock);
	AstLocal ret = ast_node(a, AST_Return);
	AstLocal lit = ast_node(a, AST_Literal);
	ast_set_ival(a, lit, 42);
	ast_add_child(a, ret, lit);
	ast_add_child(a, bb, ret);

	CHECK(ast_root(a) == bb, "entry block is the root");
	CHECK(ast_kind(a, ast_last_child(a, bb)) == AST_Return, "block ends in Return");
	CHECK(ast_kind(a, ast_first_child(a, ret)) == AST_Literal, "Return carries a value");
	CHECK(ast_ival(a, ast_first_child(a, ret)) == 42, "returns 42");

	char msg[64];
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "function shell validates");
	ast_arena_free(a);
}

/* Provenance: every AST node can carry its origin CST node id (§14). */
static void suite_provenance(void) {
	AstArena *a = ast_arena_new();
	AstLocal n = ast_node(a, AST_Binary);
	ast_set_op(a, n, '+');
	ast_set_cst(a, n, 0x0000000700000012ull);
	ast_set_type(a, n, 3 /*VT_INT-ish*/, 0);
	CHECK(ast_cst(a, n) == 0x0000000700000012ull, "cst provenance round-trips");
	CHECK(ast_type_t(a, n) == 3, "type_t round-trips");
	ast_arena_free(a);
}

/* Template rewrite API (§12): ast_set_kind + ast_clear_children collapse a
 * Binary(Literal, Literal) subtree into a Literal in place, as the const-fold
 * template does. Fold `2 + 3 * 4` bottom-up to a single Literal 14. */
static void suite_template(void) {
	AstArena *a = ast_arena_new();
	AstLocal add = build_expr(a);
	AstLocal mul = ast_child(a, add, 1);

	/* fold the inner 3 * 4 -> 12 */
	ast_set_kind(a, mul, AST_Literal);
	ast_clear_children(a, mul);
	ast_set_ival(a, mul, 12);
	CHECK(ast_kind(a, mul) == AST_Literal, "mul retagged to Literal");
	CHECK(ast_nchild(a, mul) == 0, "folded node has no children");
	CHECK(ast_first_child(a, mul) == AST_NONE, "folded first_child cleared");
	CHECK(ast_last_child(a, mul) == AST_NONE, "folded last_child cleared");

	char msg[64];
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "tree valid after inner fold");

	/* now both children of add are Literals (2 and 12) -> fold to 14 */
	CHECK(ast_kind(a, ast_child(a, add, 0)) == AST_Literal, "add[0] Literal");
	CHECK(ast_kind(a, ast_child(a, add, 1)) == AST_Literal, "add[1] Literal");
	ast_set_kind(a, add, AST_Literal);
	ast_clear_children(a, add);
	ast_set_ival(a, add, 14);
	CHECK(ast_kind(a, add) == AST_Literal, "add folded to Literal");
	CHECK(ast_ival(a, add) == 14, "folded value is 14");
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "tree valid after outer fold");

	char buf[64];
	ast_dump(a, ast_root(a), buf, sizeof buf);
	CHECK(strcmp(buf, "Literal 14\n") == 0, "folded tree dumps as a single Literal");
	ast_arena_free(a);
}

int main(int argc, char **argv) {
	const char *only = argc > 1 ? argv[1] : NULL;
	if (!only || !strcmp(only, "arena"))
		suite_arena();
	if (!only || !strcmp(only, "validate"))
		suite_validate();
	if (!only || !strcmp(only, "dump"))
		suite_dump();
	if (!only || !strcmp(only, "cfg"))
		suite_cfg();
	if (!only || !strcmp(only, "provenance"))
		suite_provenance();
	if (!only || !strcmp(only, "template"))
		suite_template();

	fprintf(stderr, "asttool: %d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
