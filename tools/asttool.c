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

static void suite_clone(void) {
	AstArena *a = ast_arena_new();
	AstLocal add = build_expr(a);
	AstLocal mul = ast_child(a, add, 1);

	AstArena *b = ast_arena_clone(a);
	CHECK(b != NULL, "clone allocates");
	CHECK(ast_count(b) == ast_count(a), "clone has same node count");

	for (AstLocal n = 0; n < ast_count(a); n++) {
		CHECK(ast_kind(b, n) == ast_kind(a, n), "clone kind matches");
		CHECK(ast_op(b, n) == ast_op(a, n), "clone op matches");
		CHECK(ast_ival(b, n) == ast_ival(a, n), "clone ival matches");
		CHECK(ast_nchild(b, n) == ast_nchild(a, n), "clone nchild matches");
		CHECK(ast_first_child(b, n) == ast_first_child(a, n), "clone first_child matches");
		CHECK(ast_next_sib(b, n) == ast_next_sib(a, n), "clone next_sib matches");
		CHECK(ast_parent(b, n) == ast_parent(a, n), "clone parent matches");
	}
	char msg[64];
	CHECK(ast_validate(b, msg, sizeof msg) == 0, "clone validates");

	ast_set_op(b, add, '-');
	ast_clear_children(b, mul);
	CHECK(ast_op(a, add) == '+', "original op unchanged after clone mutated");
	CHECK(ast_nchild(a, mul) == 2, "original children unchanged after clone cleared");
	CHECK(ast_op(b, add) == '-', "clone op did change");
	CHECK(ast_nchild(b, mul) == 0, "clone children did clear");

	AstArena *c = ast_arena_clone(a);
	ast_set_ival(a, ast_child(a, add, 0), 99);
	CHECK(ast_ival(c, ast_child(c, add, 0)) == 2, "second clone unchanged after original mutated");

	AstLocal extra = ast_node(b, AST_Literal);
	ast_set_ival(b, extra, 7);
	CHECK(ast_count(b) == ast_count(a) + 1, "clone grew by one node");
	CHECK(ast_count(c) == ast_count(a), "unrelated clone did not grow");

	CHECK(ast_arena_clone(a) != a, "clone is a distinct object");
	AstArena *empty = ast_arena_new();
	AstArena *ec = ast_arena_clone(empty);
	CHECK(ec != NULL && ast_count(ec) == 0, "clone of empty arena is empty");

	ast_arena_free(a);
	ast_arena_free(b);
	ast_arena_free(c);
	ast_arena_free(empty);
	ast_arena_free(ec);
}

static void suite_validate(void) {
	AstArena *a = ast_arena_new();
	build_expr(a);
	char msg[64];
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "well-formed tree validates");

	ast_arena_reset(a);
	CHECK(ast_count(a) == 0, "reset empties the arena");
	CHECK(ast_root(a) == AST_NONE, "reset arena has no root");

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

static void suite_provenance(void) {
	AstArena *a = ast_arena_new();
	AstLocal n = ast_node(a, AST_Binary);
	ast_set_op(a, n, '+');
	ast_set_cst(a, n, 0x0000000700000012ull);
	ast_set_type(a, n, 3, 0);
	CHECK(ast_cst(a, n) == 0x0000000700000012ull, "cst provenance round-trips");
	CHECK(ast_type_t(a, n) == 3, "type_t round-trips");
	ast_arena_free(a);
}

static void suite_template(void) {
	AstArena *a = ast_arena_new();
	AstLocal add = build_expr(a);
	AstLocal mul = ast_child(a, add, 1);

	ast_set_kind(a, mul, AST_Literal);
	ast_clear_children(a, mul);
	ast_set_ival(a, mul, 12);
	CHECK(ast_kind(a, mul) == AST_Literal, "mul retagged to Literal");
	CHECK(ast_nchild(a, mul) == 0, "folded node has no children");
	CHECK(ast_first_child(a, mul) == AST_NONE, "folded first_child cleared");
	CHECK(ast_last_child(a, mul) == AST_NONE, "folded last_child cleared");

	char msg[64];
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "tree valid after inner fold");

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

static AstLocal build_fn(AstArena *a, uint64_t callee, uint64_t local,
												 uint64_t litval, uint64_t refoff) {
	AstLocal bb = ast_node(a, AST_BasicBlock);
	AstLocal call = ast_node(a, AST_Invoke);
	ast_set_sym(a, call, callee);
	AstLocal ref = ast_node(a, AST_Ref);
	ast_set_sym(a, ref, local);
	ast_set_ival(a, ref, refoff);
	AstLocal lit = ast_node(a, AST_Literal);
	ast_set_ival(a, lit, litval);
	AstLocal ret = ast_node(a, AST_Return);
	ast_add_child(a, call, ref);
	ast_add_child(a, call, lit);
	ast_add_child(a, ret, call);
	ast_add_child(a, bb, ret);
	return bb;
}

static void suite_intention(void) {
	AstArena *a = ast_arena_new();
	AstArena *b = ast_arena_new();
	build_fn(a, 0x1111, 0x2222, 42, 8);
	build_fn(b, 0x9999, 0x7777, 42, 8);
	uint64_t ha = ast_intention_hash(a, AST_NONE);
	uint64_t hb = ast_intention_hash(b, AST_NONE);
	CHECK(ha != 0, "hash is nonzero");
	CHECK(ha == ast_intention_hash(a, AST_NONE), "hash is stable");
	CHECK(ha == hb, "alpha-renamed identifiers hash equal");
	ast_arena_free(b);

	b = ast_arena_new();
	build_fn(b, 0x1111, 0x2222, 43, 8);
	CHECK(ha != ast_intention_hash(b, AST_NONE),
				"edited literal changes the hash");
	ast_arena_free(b);

	b = ast_arena_new();
	build_fn(b, 0x1111, 0x2222, 42, 24);
	CHECK(ha == ast_intention_hash(b, AST_NONE),
				"Ref frame offset is excluded");
	ast_arena_free(b);

	b = ast_arena_new();
	build_fn(b, 0x1111, 0x1111, 42, 8);
	CHECK(ha != ast_intention_hash(b, AST_NONE),
				"distinct identifiers collapsing to one changes the hash");
	ast_arena_free(b);

	b = ast_arena_new();
	AstLocal bb = build_fn(b, 0x1111, 0x2222, 42, 8);
	AstLocal extra = ast_node(b, AST_Literal);
	ast_set_ival(b, extra, 1);
	ast_add_child(b, bb, extra);
	CHECK(ha != ast_intention_hash(b, AST_NONE),
				"added node changes the hash");
	ast_arena_free(b);

	CHECK(ast_intention_hash(NULL, AST_NONE) == 0, "NULL arena hashes to 0");
	AstArena *e = ast_arena_new();
	CHECK(ast_intention_hash(e, AST_NONE) == 0, "empty arena hashes to 0");
	ast_arena_free(e);
	ast_arena_free(a);
}

static int color_valid(int n, const uint64_t *adj, const int *color, int k) {
	for (int i = 0; i < n; i++) {
		if (color[i] < 0)
			continue;
		if (color[i] >= k)
			return 0;
		for (int j = 0; j < n; j++)
			if (i != j && (adj[i] & ((uint64_t)1 << j)) && color[j] == color[i])
				return 0;
	}
	return 1;
}

static void suite_color(void) {
	int color[8];
	{
		uint64_t adj[4];
		int cost[4] = {10, 20, 30, 40};
		for (int i = 0; i < 4; i++)
			adj[i] = (0xf & ~(1u << i));
		int nc = ast_color_graph(4, adj, cost, 4, color);
		CHECK(nc == 4, "K4 with k=4 colors all four");
		CHECK(color_valid(4, adj, color, 4), "K4/k=4 coloring is proper");
	}
	{
		uint64_t adj[4];
		int cost[4] = {10, 20, 30, 40};
		for (int i = 0; i < 4; i++)
			adj[i] = (0xf & ~(1u << i));
		int nc = ast_color_graph(4, adj, cost, 3, color);
		CHECK(nc == 3, "K4 with k=3 spills exactly one");
		CHECK(color[0] < 0, "the lowest-cost node is the one spilled");
		CHECK(color_valid(4, adj, color, 3), "K4/k=3 coloring is proper");
	}
	{
		uint64_t adj[4] = {0, 0, 0, 0};
		int cost[4] = {1, 1, 1, 1};
		int nc = ast_color_graph(4, adj, cost, 1, color);
		CHECK(nc == 4, "independent set needs only one color");
		for (int i = 0; i < 4; i++)
			CHECK(color[i] == 0, "all independent nodes share color 0");
	}
	{
		uint64_t adj[4] = {0x2, 0x1 | 0x4, 0x2 | 0x8, 0x4};
		int cost[4] = {1, 1, 1, 1};
		int nc = ast_color_graph(4, adj, cost, 2, color);
		CHECK(nc == 4, "a path is 2-colorable");
		CHECK(color_valid(4, adj, color, 2), "path 2-coloring is proper");
	}
	CHECK(ast_color_graph(0, NULL, NULL, 4, color) == 0, "empty graph colors nothing");
}

static void suite_forecast(void) {
	{
		double y[3] = {10, 20, 30};
		CHECK(ast_fc_rw(y, 3) == 30, "rw predicts the last sample");
	}
	{
		double y[4] = {1, 2, 3, 4};
		double p = ast_fc_lin(y, 4); /* slope 1 at t=4 -> 5 */
		CHECK(p > 4.5 && p < 5.5, "lin extrapolates a ramp");
	}
	{
		double y[5] = {7, 7, 7, 7, 7};
		double p = ast_fc_forecast(y, 5);
		CHECK(p > 6.9 && p < 7.1, "ensemble holds a constant series");
	}
	{
		double y[6] = {2, 4, 6, 8, 10, 12};
		double p = ast_fc_forecast(y, 6);
		CHECK(p > 11.0 && p < 15.0, "ensemble tracks a linear ramp");
		CHECK(ast_fc_finite(p), "ensemble prediction is finite");
	}
	{
		double y[10] = {5, 1, 9, 3, 7, 2, 8, 4, 6, 5};
		int k;
		for (k = 0; k < AST_FC_COUNT; k++)
			CHECK(ast_fc_finite(ast_fc_call(k, y, 10)), ast_fc_models[k].name);
	}
	{
		double e0 = ast_fc_exp(0.0), en = ast_fc_exp(-1.0);
		CHECK(e0 > 0.99 && e0 < 1.01, "exp(0) ~ 1");
		CHECK(en > 0.30 && en < 0.42, "exp(-1) ~ 0.3679");
	}
}

int main(int argc, char **argv) {
	const char *only = argc > 1 ? argv[1] : NULL;
	if (!only || !strcmp(only, "arena"))
		suite_arena();
	if (!only || !strcmp(only, "forecast"))
		suite_forecast();
	if (!only || !strcmp(only, "clone"))
		suite_clone();
	if (!only || !strcmp(only, "color"))
		suite_color();
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
	if (!only || !strcmp(only, "intention"))
		suite_intention();

	fprintf(stderr, "asttool: %d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
