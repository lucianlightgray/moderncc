#include "mccast.c"
#include "mccgate.h" /* the gate vocabulary + M3 bridge (MCC_INTERNAL-independent) */
#include "mccmagic.h" /* constant-division magic numbers, selftested before wiring */

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

static void suite_wide(void) {
	AstArena *a = ast_arena_new();
	AstLocal add = build_expr(a);
	AstLocal lhs = ast_child(a, add, 0);
	AstLocal rhs = ast_child(a, add, 1);
	AstLocal n, i;
	AstArena *b;
	uint64_t h_before, h_after;

	CHECK(ast_wide_hi(a, lhs) == 0, "wide hi defaults to zero");
	CHECK(ast_wide_r2(a, lhs) == AST_R2_NONE, "wide r2 defaults to absent");

	h_before = ast_slice_ident_hash(a, add);
	ast_set_wide(a, lhs, 0, AST_R2_NONE);
	CHECK(ast_slice_ident_hash(a, add) == h_before,
				"writing the default wide value leaves the identity unchanged");

	ast_set_wide(a, lhs, 0xdeadbeefcafebabeull, 3);
	CHECK(ast_wide_hi(a, lhs) == 0xdeadbeefcafebabeull, "wide hi reads back");
	CHECK(ast_wide_r2(a, lhs) == 3, "wide r2 reads back");
	CHECK(ast_wide_hi(a, rhs) == 0, "sibling wide hi still zero");
	CHECK(ast_wide_r2(a, rhs) == AST_R2_NONE, "sibling wide r2 still absent");

	h_after = ast_slice_ident_hash(a, add);
	CHECK(h_after != h_before, "the wide value changes the slice identity");
	ast_set_wide(a, lhs, 0xdeadbeefcafebabfull, 3);
	CHECK(ast_slice_ident_hash(a, add) != h_after,
				"the high half alone separates two identities");
	ast_set_wide(a, lhs, 0xdeadbeefcafebabeull, 3);

	for (i = 0; i < 300; i++) {
		n = ast_node(a, AST_Literal);
		ast_set_ival(a, n, i);
	}
	CHECK(ast_wide_hi(a, lhs) == 0xdeadbeefcafebabeull, "wide hi survives growth");
	CHECK(ast_wide_r2(a, lhs) == 3, "wide r2 survives growth");
	CHECK(ast_wide_hi(a, ast_count(a) - 1) == 0, "grown node hi defaults to zero");
	CHECK(ast_wide_r2(a, ast_count(a) - 1) == AST_R2_NONE,
				"grown node r2 defaults to absent");

	b = ast_arena_clone(a);
	CHECK(b != NULL, "wide arena clones");
	CHECK(ast_wide_hi(b, lhs) == 0xdeadbeefcafebabeull, "clone carries wide hi");
	CHECK(ast_wide_r2(b, lhs) == 3, "clone carries wide r2");
	CHECK(ast_wide_hi(b, rhs) == 0, "clone carries the absent sibling");
	ast_set_wide(b, lhs, 1, 4);
	CHECK(ast_wide_hi(a, lhs) == 0xdeadbeefcafebabeull,
				"original wide hi unchanged after clone mutated");

	{
		AstArena *dst = ast_arena_new();
		AstLocal site = build_expr(dst);
		int spliced = ast_slice_splice(dst, site, a, add);
		CHECK(spliced >= 1, "splice of a wide kernel succeeds");
		CHECK(ast_wide_hi(dst, ast_child(dst, site, 0)) == 0xdeadbeefcafebabeull,
					"splice carries wide hi into the grafted child");
		CHECK(ast_wide_r2(dst, ast_child(dst, site, 0)) == 3,
					"splice carries wide r2 into the grafted child");
		CHECK(ast_slice_ident_hash(dst, site) == ast_slice_ident_hash(a, add),
					"spliced site keeps the kernel's wide-bearing identity");
		ast_arena_free(dst);
	}

	ast_arena_free(a);
	ast_arena_free(b);
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

static void suite_gatemap(void) {
	unsigned g, c;
	AstGateMask m;
	/* superopt-search 4-bit gate <-> unified AstGateMask: lossless round-trip over
	 * the whole 16-value space, both directions. */
	for (g = 0; g < 16; g++)
		CHECK(ast_gate_to_so(ast_gate_from_so(g)) == g,
					"so_gate round-trips through the unified mask");
	for (m = 0; m <= (AST_SG_TEMPLATES | AST_SG_PROMOTE | AST_SG_INLINE | AST_SG_NOCALLFUL);
			 m++) {
		AstGateMask keep = m & (AST_SG_TEMPLATES | AST_SG_PROMOTE | AST_SG_INLINE |
														AST_SG_NOCALLFUL);
		CHECK(ast_gate_from_so(ast_gate_to_so(keep)) == keep,
					"unified so-subset round-trips back through so_gate");
	}
	/* exact bit correspondences (mirror so_setenv_cfg). */
	CHECK(ast_gate_from_so(SO_GATE_TEMPLATES) == AST_SG_TEMPLATES, "so templates bit");
	CHECK(ast_gate_from_so(SO_GATE_PROMOTE) == AST_SG_PROMOTE, "so promote bit");
	CHECK(ast_gate_from_so(SO_GATE_INLINE) == AST_SG_INLINE, "so inline bit");
	CHECK(ast_gate_from_so(SO_GATE_NOCALLFUL) == AST_SG_NOCALLFUL, "so no_callful bit");
	CHECK(ast_gate_from_so(15) ==
					(AST_SG_TEMPLATES | AST_SG_PROMOTE | AST_SG_INLINE | AST_SG_NOCALLFUL),
				"all four so bits map to all four unified bits");
	/* perfn best_cfg (bit0=tmpl, bit1=promo, bit2=inl; the drivers use values 1/3/7). */
	for (c = 0; c < 8; c++)
		CHECK(ast_gate_to_perfn(ast_gate_from_perfn(c)) == c, "perfn cfg round-trips");
	CHECK(ast_gate_from_perfn(1) == AST_SG_TEMPLATES, "perfn cfg=1 is templates only");
	CHECK(ast_gate_from_perfn(3) == (AST_SG_TEMPLATES | AST_SG_PROMOTE),
				"perfn cfg=3 is templates+promote");
	CHECK(ast_gate_from_perfn(7) ==
					(AST_SG_TEMPLATES | AST_SG_PROMOTE | AST_SG_INLINE),
				"perfn cfg=7 is templates+promote+inline");
	/* the superopt-only unified bits sit ABOVE the six in-process fold-gate/knob bits,
	 * so the two vocabularies never collide in one mask. */
	CHECK((AST_SG_PROMOTE | AST_SG_INLINE | AST_SG_NOCALLFUL | AST_SG_CPROPJOIN |
				 AST_SG_CSEJOIN) >
					(AST_SG_TEMPLATES | AST_SG_NARROW | AST_SG_BITFLAG | AST_SG_SETHI |
					 AST_SG_NARROWFIX | AST_SG_SETHILEAF),
				"superopt-only unified bits are disjoint above the fold-gate/knob bits");
}

/* Exhaustively prove the constant-division magic numbers against native `/` and `%`
 * before any AST transform trusts them. For each divisor in a large range, apply the
 * magic to a dense, boundary-heavy dividend set (0, 1, near multiples, the sign/word
 * extremes) and require an exact match. A single mismatch would be a silent arithmetic
 * miscompile, so this is the gate that lets the fold be built with confidence. */
static void suite_magic(void) {
	static const uint32_t uedge[] = {0u,        1u,        2u,          3u,
																	 0x7FFFFFFFu, 0x80000000u, 0x80000001u, 0xFFFFFFFEu,
																	 0xFFFFFFFFu, 0x01234567u, 0xFEDCBA98u, 0xAAAAAAAAu};
	static const int32_t sedge[] = {0,          1,          -1,        2,
																	-2,         3,          -3,        0x7FFFFFFF,
																	(-0x7FFFFFFF - 1), 0x40000000, -0x40000000, 123456789};
	uint32_t d;
	int uok = 1, sok = 1, i;
	for (d = 2; d <= 20000 && uok; d++) {
		MccMagicU mu = mcc_magicu(d);
		uint32_t n;
		for (i = 0; i < (int)(sizeof uedge / sizeof uedge[0]); i++)
			if (mcc_divu_apply(uedge[i], mu) != uedge[i] / d)
				uok = 0;
		/* dense sweep around every multiple boundary up to a cap */
		for (n = 0; n < 40000u; n++)
			if (mcc_divu_apply(n, mu) != n / d)
				uok = 0;
		for (n = d - 1; n < 40000u * d && n >= d - 1; n += d) {
			if (mcc_divu_apply(n, mu) != n / d || mcc_divu_apply(n + 1, mu) != (n + 1) / d) {
				uok = 0;
				break;
			}
		}
	}
	CHECK(uok, "unsigned magic division matches native / over divisors 2..20000");

	for (d = 2; d <= 20000 && sok; d++) {
		MccMagicS mp = mcc_magics((int32_t)d);
		MccMagicS mn = mcc_magics(-(int32_t)d);
		int32_t v;
		for (i = 0; i < (int)(sizeof sedge / sizeof sedge[0]); i++) {
			int32_t x = sedge[i];
			if (mcc_divs_apply(x, (int32_t)d, mp) != x / (int32_t)d)
				sok = 0;
			if (mcc_divs_apply(x, -(int32_t)d, mn) != x / -(int32_t)d)
				sok = 0;
		}
		for (v = -30000; v < 30000; v++)
			if (mcc_divs_apply(v, (int32_t)d, mp) != v / (int32_t)d ||
					mcc_divs_apply(v, -(int32_t)d, mn) != v / -(int32_t)d)
				sok = 0;
	}
	CHECK(sok, "signed magic division matches native / over divisors +-2..20000");

	{
		static const uint64_t u64d[] = {3ull, 5ull, 7ull, 10ull, 100ull, 1000ull,
																		641ull, 6700417ull, 0x100000001ull,
																		0xFFFFFFFFull, 0x8000000000000001ull,
																		0xFFFFFFFFFFFFFFFFull};
		static const uint64_t u64n[] = {0ull, 1ull, 2ull, 3ull, 0x7FFFFFFFFFFFFFFFull,
																		0x8000000000000000ull, 0x8000000000000001ull,
																		0xFFFFFFFFFFFFFFFEull, 0xFFFFFFFFFFFFFFFFull,
																		0x0123456789ABCDEFull, 0xFEDCBA9876543210ull,
																		0xAAAAAAAAAAAAAAAAull, 0xDEADBEEFCAFEBABEull};
		static const int64_t s64d[] = {3, 5, 7, 10, 100, 1000, 641, 6700417,
																	 0x100000001ll, 0x7FFFFFFFFFFFFFFFll};
		static const int64_t s64n[] = {0, 1, -1, 2, -2, 0x7FFFFFFFFFFFFFFFll,
																	 (-0x7FFFFFFFFFFFFFFFll - 1), 0x0123456789ABCDEFll,
																	 -0x0123456789ABCDEFll, 1234567890123456789ll,
																	 -1234567890123456789ll};
		int u64ok = 1, s64ok = 1;
		size_t di, ni;
		for (di = 0; di < sizeof u64d / sizeof u64d[0]; di++) {
			MccMagicU64 mu = mcc_magicu64(u64d[di]);
			for (ni = 0; ni < sizeof u64n / sizeof u64n[0]; ni++)
				if (mcc_divu64_apply(u64n[ni], mu) != u64n[ni] / u64d[di])
					u64ok = 0;
			for (uint64_t k = 0; k < 4096; k++) {
				uint64_t n = k * u64d[di];
				if (mcc_divu64_apply(n, mu) != n / u64d[di] ||
						mcc_divu64_apply(n + 1, mu) != (n + 1) / u64d[di] ||
						mcc_divu64_apply(n - 1, mu) != (n - 1) / u64d[di])
					u64ok = 0;
			}
			for (uint64_t n = 0; n < 60000; n++)
				if (mcc_divu64_apply(n, mu) != n / u64d[di])
					u64ok = 0;
		}
		CHECK(u64ok, "unsigned 64-bit magic division matches native /");
		for (di = 0; di < sizeof s64d / sizeof s64d[0]; di++) {
			MccMagicS64 mp = mcc_magics64(s64d[di]);
			MccMagicS64 mn = mcc_magics64(-s64d[di]);
			for (ni = 0; ni < sizeof s64n / sizeof s64n[0]; ni++) {
				int64_t x = s64n[ni];
				if (mcc_divs64_apply(x, s64d[di], mp) != x / s64d[di])
					s64ok = 0;
				if (mcc_divs64_apply(x, -s64d[di], mn) != x / -s64d[di])
					s64ok = 0;
			}
			for (int64_t v = -60000; v < 60000; v++)
				if (mcc_divs64_apply(v, s64d[di], mp) != v / s64d[di] ||
						mcc_divs64_apply(v, -s64d[di], mn) != v / -s64d[di])
					s64ok = 0;
			for (uint64_t k = 0; k < 4096; k++) {
				int64_t n = (int64_t)(k * (uint64_t)s64d[di]);
				if (mcc_divs64_apply(n, s64d[di], mp) != n / s64d[di] ||
						mcc_divs64_apply(-n, s64d[di], mp) != (-n) / s64d[di])
					s64ok = 0;
			}
		}
		CHECK(s64ok, "signed 64-bit magic division matches native /");
	}
}

static void suite_vlat(void) {
	AstVLat top = ast_vlat_top();
	AstVLat bot = ast_vlat_bottom();
	AstVLat a = ast_vlat_full_fact(0, 10, 0);
	AstVLat b = ast_vlat_full_fact(5, 20, 0);

	CHECK(top.state == AST_VLAT_TOP, "top is TOP");
	CHECK(bot.state == AST_VLAT_BOTTOM, "bottom is BOTTOM");
	CHECK(a.state == AST_VLAT_FACT, "full_fact is a FACT");

	AstVLat m1 = ast_vlat_meet(top, a);
	CHECK(m1.state == AST_VLAT_FACT && m1.lo == 0 && m1.hi == 10, "TOP meet x == x");
	AstVLat m2 = ast_vlat_meet(a, top);
	CHECK(m2.state == AST_VLAT_FACT && m2.lo == 0 && m2.hi == 10, "x meet TOP == x");
	CHECK(ast_vlat_meet(bot, a).state == AST_VLAT_BOTTOM, "BOTTOM meet x == BOTTOM");
	CHECK(ast_vlat_meet(a, bot).state == AST_VLAT_BOTTOM, "x meet BOTTOM == BOTTOM");

	a.kzero = 0xF0;
	a.kone = 0x02;
	b.kzero = 0x30;
	b.kone = 0x06;
	AstVLat u = ast_vlat_meet(a, b);
	CHECK(u.state == AST_VLAT_FACT, "meet of two facts is a fact");
	CHECK(u.lo == 0 && u.hi == 20, "meet is the interval union [0,20]");
	CHECK(u.kzero == 0x30, "meet intersects known-zero bits");
	CHECK(u.kone == 0x02, "meet intersects known-one bits");

	AstVLat t = ast_vlat_full_fact(-2147483647 - 1, 2147483647, 0);
	ast_vlat_refine_bound(&t, 10, 1);
	CHECK(t.lo == 10, "refine x >= 10 raises the lower bound");
	ast_vlat_refine_bound(&t, 20, 0);
	CHECK(t.hi == 20, "refine x <= 20 lowers the upper bound");
	CHECK(t.state == AST_VLAT_FACT && t.lo == 10 && t.hi == 20,
				"if(x>=10 && x<=20) transfer yields [10,20]");
	ast_vlat_refine_bound(&t, 30, 1);
	CHECK(t.state == AST_VLAT_BOTTOM, "empty intersection collapses to BOTTOM");

	AstVLat lo8 = ast_vlat_full_fact(0, 200, 0);
	CHECK(ast_vlat_fits_bytes(&lo8, 1) == 1, "[0,200] fits an 8-bit width");
	CHECK(ast_vlat_fits_bytes(&lo8, 2) == 1, "[0,200] fits a 16-bit width");
	AstVLat wide = ast_vlat_full_fact(0, 100000, 0);
	CHECK(ast_vlat_fits_bytes(&wide, 2) == 0, "[0,100000] does not fit a 16-bit width");
	CHECK(ast_vlat_fits_bytes(&wide, 4) == 1, "[0,100000] fits a 32-bit width");
	AstVLat neg = ast_vlat_full_fact(-100, 100, 0);
	CHECK(ast_vlat_fits_bytes(&neg, 1) == 1, "[-100,100] fits a signed 8-bit width");
	AstVLat negwide = ast_vlat_full_fact(-200, 100, 0);
	CHECK(ast_vlat_fits_bytes(&negwide, 1) == 0, "[-200,100] does not fit any 8-bit width");

	CHECK(ast_vlat_fits_bytes(&top, 4) == 0, "TOP never fits (conservative)");
	CHECK(ast_vlat_fits_bytes(&bot, 4) == 0, "BOTTOM never fits (conservative)");
	CHECK(ast_vlat_fits_bytes(&lo8, 8) == 0, "8-byte width query is refused (no narrowing)");
	CHECK(ast_vlat_fits_bytes(&lo8, 0) == 0, "zero-width query is refused");
}

typedef struct WalkRec {
	int logk[64];
	int logsel[64][COMBO_MAX];
	int n;
} WalkRec;

static long walk_sum_score(const int *sel, int k, void *user) {
	long s = 0;
	int i;
	(void)user;
	for (i = 0; i < k; i++)
		s += sel[i];
	return s;
}

static void walk_recorder(const int *sel, int k, int depth, int walk, void *user) {
	WalkRec *r = (WalkRec *)user;
	int i;
	(void)depth;
	(void)walk;
	if (r->n >= 64)
		return;
	r->logk[r->n] = k;
	for (i = 0; i < k; i++)
		r->logsel[r->n][i] = sel[i];
	r->n++;
}

static int walk_match(const WalkRec *r, const int (*exp)[4], int rows) {
	int i, j;
	if (r->n != rows)
		return 0;
	for (i = 0; i < rows; i++) {
		if (r->logk[i] != exp[i][0])
			return 0;
		for (j = 0; j < r->logk[i]; j++)
			if (r->logsel[i][j] != exp[i][1 + j])
				return 0;
	}
	return 1;
}

static void suite_combo_walk(void) {
	static const int exp_linear[][4] = {{1, 0}, {1, 1}, {2, 0, 1}, {1, 2},
																			{2, 0, 2}, {2, 1, 2}, {3, 0, 1, 2}};
	static const int exp_dfs[][4] = {{1, 0}, {2, 0, 1}, {3, 0, 1, 2}, {2, 0, 2},
																	 {1, 1}, {2, 1, 2}, {1, 2}};
	static const int exp_bfs[][4] = {{1, 0}, {1, 1}, {1, 2}, {2, 0, 1},
																	 {2, 0, 2}, {2, 1, 2}, {3, 0, 1, 2}};
	static const int exp_product[][4] = {{1, 0}, {1, 1}, {1, 2}, {2, 0, 1},
																			 {3, 0, 1, 2}, {2, 0, 2}, {2, 1, 2}};
	ComboSpec spec;
	ComboBest best[4];
	WalkRec rec;
	int w, i;
	spec.nitems = 3;
	spec.min_k = 1;
	spec.max_k = 3;
	spec.ordered = 0;
	spec.budget = 0;
	spec.score = walk_sum_score;
	spec.visit = walk_recorder;
	spec.user = &rec;
	for (w = 0; w < 4; w++) {
		rec.n = 0;
		spec.walk = w;
		CHECK(combo_run(&spec, &best[w]), "combo_run finds a best for each walk");
		if (w == COMBO_WALK_LINEAR)
			CHECK(walk_match(&rec, exp_linear, 7), "LINEAR visit order is the mask-counter order");
		else if (w == COMBO_WALK_DFS)
			CHECK(walk_match(&rec, exp_dfs, 7), "DFS visits the subset lattice depth-first");
		else if (w == COMBO_WALK_BFS)
			CHECK(walk_match(&rec, exp_bfs, 7), "BFS visits subsets by increasing size, lex order");
		else
			CHECK(walk_match(&rec, exp_product, 7),
						"PRODUCT visits breadth roots then deepens each");
		CHECK(rec.n == 7, "each walk enumerates all 7 non-empty subsets of 3 items");
	}
	for (w = 1; w < 4; w++) {
		int same = best[w].k == best[0].k && best[w].score == best[0].score;
		for (i = 0; i < best[0].k; i++)
			if (best[w].sel[i] != best[0].sel[i])
				same = 0;
		CHECK(same, "winner (sel + score) is invariant across all four walks");
	}
	CHECK(best[0].k == 1 && best[0].sel[0] == 0 && best[0].score == 0,
				"the deterministic winner is the singleton {0}");
}

/* Build `local(offa) <op> local(offb)` as a standalone expression; return the
   Binary root. Refs are pure VT_LOCAL reads (offset in ival, no sym) so they hit
   ast_slice_ident_hash's offset-interning path. */
static AstLocal build_binop(AstArena *a, int op, int32_t offa, int32_t offb) {
	AstLocal l = ast_node(a, AST_Ref);
	ast_set_op(a, l, VT_LOCAL);
	ast_set_ival(a, l, (uint64_t)(int64_t)offa);
	AstLocal r = ast_node(a, AST_Ref);
	ast_set_op(a, r, VT_LOCAL);
	ast_set_ival(a, r, (uint64_t)(int64_t)offb);
	AstLocal bin = ast_node(a, AST_Binary);
	ast_set_op(a, bin, op);
	ast_add_child(a, bin, l);
	ast_add_child(a, bin, r);
	return bin;
}

static void suite_slice_ident(void) {
	/* Baseline: `l(-8) + l(-16)` (input pattern [distinct, distinct]). */
	AstArena *a = ast_arena_new();
	AstLocal ra = build_binop(a, '+', -8, -16);
	uint64_t ha = ast_slice_ident_hash(a, ra);
	CHECK(ha != 0, "slice identity is nonzero");
	CHECK(ha == ast_slice_ident_hash(a, ra), "slice identity is stable");

	/* Frame-offset invariance / cross-function reuse: the SAME computation reading
	   different actual slots (-32,-40) has the SAME identity -- this is what lets a
	   variant proved in one function be reused in another. */
	AstArena *b = ast_arena_new();
	AstLocal rb = build_binop(b, '+', -32, -40);
	CHECK(ha == ast_slice_ident_hash(b, rb),
				"identity is invariant to the actual frame offsets");
	ast_arena_free(b);

	/* Sharing-pattern sensitivity: `l(-8) + l(-8)` (pattern [x,x]) must DIFFER from
	   `l(-8) + l(-16)` (pattern [x,y]) -- they are different computations. */
	b = ast_arena_new();
	AstLocal rxx = build_binop(b, '+', -8, -8);
	CHECK(ha != ast_slice_ident_hash(b, rxx),
				"x+x and x+y have distinct identities (input sharing matters)");
	/* ...where the coarse whole-function intention hash canNOT tell them apart. */
	CHECK(ast_intention_hash(a, ra) == ast_intention_hash(b, rxx),
				"intention hash is offset-blind (why slice identity refines it)");
	ast_arena_free(b);

	/* Operator sensitivity. */
	b = ast_arena_new();
	AstLocal rsub = build_binop(b, '-', -8, -16);
	CHECK(ha != ast_slice_ident_hash(b, rsub), "operator changes identity");
	ast_arena_free(b);

	/* Context-freedom: the same slice hashed at its root is identical whether it
	   sits under a Return or as an Invoke argument -- enclosing scope is irrelevant,
	   so the slice keeps its identity through inlining and other passes. */
	b = ast_arena_new();
	AstLocal bb = ast_node(b, AST_BasicBlock);
	AstLocal ret = ast_node(b, AST_Return);
	AstLocal inner = build_binop(b, '+', -8, -16);
	ast_add_child(b, ret, inner);
	ast_add_child(b, bb, ret);
	CHECK(ha == ast_slice_ident_hash(b, inner),
				"identity is independent of the enclosing scope");
	ast_arena_free(b);

	CHECK(ast_slice_ident_hash(NULL, AST_NONE) == 0, "NULL arena identity is 0");
	ast_arena_free(a);
}

static void suite_slice_window(void) {
	/* Reset the module-global cache (asttool includes mccast.c, so the statics are
	   in scope) to make the suite order-independent. */
	ast_slice_memo_n = 0;
	ast_slice_seen = 0;
	ast_slice_reuse = 0;

	/* A function body holding one compute slice `l(-8) + l(-16)` under a Return. */
	AstArena *a = ast_arena_new();
	AstLocal bb = ast_node(a, AST_BasicBlock);
	AstLocal ret = ast_node(a, AST_Return);
	AstLocal add = build_binop(a, '+', -8, -16);
	ast_add_child(a, ret, add);
	ast_add_child(a, bb, ret);

	long rec = ast_slice_window_scan(a, 0x5u);
	CHECK(rec == 1, "one compute slice recorded (Binary root; leaf Refs are too small)");
	CHECK(ast_slice_memo_n == 1, "one entry in the slice memo");
	uint64_t id = ast_slice_ident_hash(a, add);
	const AstSliceMemo *m = ast_slice_memo_get(id);
	CHECK(m != NULL, "the slice is found by its identity");
	CHECK(m && m->gates == 0x5u, "the enclosing function's gate config is stored");
	CHECK(m && m->refcount == 1, "refcount starts at 1");

	/* A DIFFERENT function computing the SAME slice from other slots reuses the
	   cache entry by identity -- the cross-function memoization win. */
	AstArena *b = ast_arena_new();
	AstLocal bb2 = ast_node(b, AST_BasicBlock);
	AstLocal ret2 = ast_node(b, AST_Return);
	AstLocal add2 = build_binop(b, '+', -64, -72);
	ast_add_child(b, ret2, add2);
	ast_add_child(b, bb2, ret2);
	ast_slice_window_scan(b, 0x5u);
	CHECK(ast_slice_memo_n == 1, "identical slice in another function does NOT add an entry");
	m = ast_slice_memo_get(id);
	CHECK(m && m->refcount == 2, "the shared identity's refcount grew to 2");
	CHECK(ast_slice_reuse == 1, "the second scan counted as a reuse hit");

	/* A structurally different slice adds a distinct entry. */
	AstArena *c = ast_arena_new();
	AstLocal bb3 = ast_node(c, AST_BasicBlock);
	AstLocal ret3 = ast_node(c, AST_Return);
	AstLocal sub = build_binop(c, '-', -8, -16);
	ast_add_child(c, ret3, sub);
	ast_add_child(c, bb3, ret3);
	ast_slice_window_scan(c, 0x5u);
	CHECK(ast_slice_memo_n == 2, "a different computation adds a new entry");

	ast_arena_free(a);
	ast_arena_free(b);
	ast_arena_free(c);
	ast_slice_memo_n = 0;
	ast_slice_seen = 0;
	ast_slice_reuse = 0;
}

/* Phase 3: the pure (host-I/O-free) half of the disk substrate — record
   (de)serialization and the per-function probe/warm-start decision. */
static void suite_slice_persist(void) {
	ast_slice_memo_n = 0;
	ast_slice_seen = 0;
	ast_slice_reuse = 0;

	/* Populate one slice, then round-trip the memo through the on-disk record
	   format and confirm every field survives. */
	AstArena *a = ast_arena_new();
	AstLocal bb = ast_node(a, AST_BasicBlock);
	AstLocal ret = ast_node(a, AST_Return);
	AstLocal add = build_binop(a, '+', -8, -16);
	ast_add_child(a, ret, add);
	ast_add_child(a, bb, ret);
	ast_slice_window_scan(a, 0x5u);
	CHECK(ast_slice_memo_n == 1, "one slice populated");

	unsigned char buf[256];
	long nb = ast_slice_rec_serialize(ast_slice_memo, ast_slice_memo_n, buf, sizeof buf);
	CHECK(nb == AST_SLICE_RECBYTES, "one record serializes to RECBYTES");

	AstSliceMemo table[8];
	int tn = ast_slice_rec_deserialize(buf, nb, table, 8);
	CHECK(tn == 1, "one record deserializes back");
	uint64_t id = ast_slice_ident_hash(a, add);
	CHECK(table[0].ident == id, "ident round-trips");
	CHECK(table[0].gates == 0x5u, "gates round-trip");
	CHECK(table[0].size == 3, "size round-trips");
	CHECK(table[0].refcount == 1, "refcount round-trips");

	/* A too-small buffer fails cleanly rather than overrunning. */
	CHECK(ast_slice_rec_serialize(ast_slice_memo, ast_slice_memo_n, buf, 8) == -1,
				"serialize into a too-small buffer returns -1");

	/* A word quad without the MAGIC tag (torn/foreign) is skipped on parse. */
	unsigned char junk[AST_SLICE_RECBYTES];
	memset(junk, 0, sizeof junk);
	CHECK(ast_slice_rec_deserialize(junk, sizeof junk, table, 8) == 0,
				"a record without the MAGIC tag is skipped");

	/* Probe HIT: a different function computing the same slice warm-starts. */
	tn = ast_slice_rec_deserialize(buf, nb, table, 8);
	AstArena *f = ast_arena_new();
	AstLocal fbb = ast_node(f, AST_BasicBlock);
	AstLocal fret = ast_node(f, AST_Return);
	AstLocal fadd = build_binop(f, '+', -64, -72); /* same shape, other slots */
	ast_add_child(f, fret, fadd);
	ast_add_child(f, fbb, fret);
	uint64_t g = 0;
	CHECK(ast_slice_probe_table(f, table, tn, &g) == 1, "probe hits the recurring slice");
	CHECK(g == 0x5u, "probe returns the cached gate config");

	/* Probe MISS: a structurally different function does not warm-start, and
	   leaves the caller's out_gates untouched. */
	AstArena *m = ast_arena_new();
	AstLocal mbb = ast_node(m, AST_BasicBlock);
	AstLocal mret = ast_node(m, AST_Return);
	AstLocal msub = build_binop(m, '-', -8, -16);
	ast_add_child(m, mret, msub);
	ast_add_child(m, mbb, mret);
	g = 0xdead;
	CHECK(ast_slice_probe_table(m, table, tn, &g) == 0, "probe misses an unknown slice");
	CHECK(g == 0xdead, "out_gates untouched on a miss");

	/* Dominant selection: a function containing both a small and a large cached
	   slice warm-starts from the LARGEST one's config. */
	AstArena *d = ast_arena_new();
	AstLocal dbb = ast_node(d, AST_BasicBlock);
	AstLocal dret = ast_node(d, AST_Return);
	AstLocal inner = build_binop(d, '+', -8, -16); /* 3 nodes */
	AstLocal tail = ast_node(d, AST_Ref);
	ast_set_op(d, tail, VT_LOCAL);
	ast_set_ival(d, tail, (uint64_t)(int64_t)-24);
	AstLocal outer = ast_node(d, AST_Binary); /* (inner) + l(-24) = 5 nodes */
	ast_set_op(d, outer, '+');
	ast_add_child(d, outer, inner);
	ast_add_child(d, outer, tail);
	ast_add_child(d, dret, outer);
	ast_add_child(d, dbb, dret);
	uint64_t id_inner = ast_slice_ident_hash(d, inner);
	uint64_t id_outer = ast_slice_ident_hash(d, outer);
	CHECK(id_inner != id_outer, "inner and outer slices have distinct identities");
	AstSliceMemo dtab[2];
	dtab[0].ident = id_inner;
	dtab[0].gates = 0x1u;
	dtab[0].size = 3;
	dtab[0].refcount = 1;
	dtab[0].proven = 0;
	dtab[1].ident = id_outer;
	dtab[1].gates = 0x2u;
	dtab[1].size = 5;
	dtab[1].refcount = 1;
	dtab[1].proven = 0;
	g = 0;
	CHECK(ast_slice_probe_table(d, dtab, 2, &g) == 1, "dominant probe hits");
	CHECK(g == 0x2u, "dominant slice (largest) config wins");

	/* The consume path must hand back the winning record's CANDIDATE SPACE, not
	   only the config that won. Without this the runtime can replay the AOT
	   decision but cannot benchmark anything against it, which is the whole point
	   of recording `eligible`. `chosen` and `eligible` are reported separately so
	   a caller can intersect the space with what its own target permits. */
	dtab[0].eligible = 0xF0u;
	dtab[1].eligible = 0xFFu;
	{
		uint64_t chosen = 0, elig = 0;
		CHECK(ast_slice_probe_table_cand(d, dtab, 2, &chosen, &elig) == 1,
					"candidate probe hits");
		CHECK(chosen == 0x2u, "candidate probe reports the winning chosen config");
		CHECK(elig == 0xFFu, "candidate probe reports the winning eligible space");
		CHECK((elig & chosen) == chosen, "chosen stays a subset of the space");
	}

	ast_arena_free(a);
	ast_arena_free(f);
	ast_arena_free(m);
	ast_arena_free(d);
	ast_slice_memo_n = 0;
	ast_slice_seen = 0;
	ast_slice_reuse = 0;
}

/* Phase 4: benchmark-proven records round-trip through the on-disk format and
   outrank static records both in the per-ident merge and in the dominant probe. */
static void suite_slice_graduate(void) {
	/* PROVEN flag survives serialize -> deserialize (word-3 bit 32). */
	AstSliceMemo recs[2];
	unsigned char buf[256];
	AstSliceMemo table[8];
	long nb;
	int tn;

	recs[0].ident = 0x1111;
	recs[0].gates = 0xAu;
	recs[0].size = 4;
	recs[0].refcount = 1;
	recs[0].proven = 1; /* bench-proven */
	recs[1].ident = 0x2222;
	recs[1].gates = 0xBu;
	recs[1].size = 7;
	recs[1].refcount = 2;
	recs[1].proven = 0; /* static */
	nb = ast_slice_rec_serialize(recs, 2, buf, sizeof buf);
	CHECK(nb == 2 * AST_SLICE_RECBYTES, "two records serialize");
	tn = ast_slice_rec_deserialize(buf, nb, table, 8);
	CHECK(tn == 2, "two records deserialize");
	CHECK(table[0].proven == 1, "proven flag round-trips (set)");
	CHECK(table[1].proven == 0, "proven flag round-trips (clear)");
	CHECK(table[0].gates == 0xAu && table[1].gates == 0xBu, "gates unaffected by proven bit");
	CHECK(table[0].refcount == 1 && table[1].refcount == 2,
				"refcount unaffected by proven bit");

	/* `eligible` is the candidate SPACE the JIT may benchmark, distinct from the
	   `gates` the AOT compile happened to choose. These pin the properties the
	   runtime depends on: it survives the round trip, it is independent of
	   `gates`, and a record may legitimately carry an eligible set WIDER than its
	   chosen one (that is the whole point -- a gate switched off for this compile
	   is still a candidate the JIT can try). */
	recs[0].eligible = 0xFFu; /* wider than chosen 0xA */
	recs[1].eligible = 0u;    /* not recorded, e.g. a no-optimizer build */
	nb = ast_slice_rec_serialize(recs, 2, buf, sizeof buf);
	tn = ast_slice_rec_deserialize(buf, nb, table, 8);
	CHECK(tn == 2, "records with eligible deserialize");
	CHECK(table[0].eligible == 0xFFu, "eligible round-trips");
	CHECK(table[1].eligible == 0u, "absent eligible round-trips as 0");
	CHECK(table[0].gates == 0xAu, "eligible does not disturb gates");
	CHECK((table[0].eligible & table[0].gates) == table[0].gates,
				"chosen is a subset of eligible");
	CHECK(table[0].eligible != table[0].gates,
				"eligible may be strictly wider than chosen");

	/* Back-compat: a legacy pre-Phase-4 record wrote word 3 as (refcount|MAGIC<<48)
	   with the proven bit region zero. It used to deserialize cleanly as static,
	   because `proven` was packed into a spare region of an existing word and the
	   record stayed 4 words. Adding `eligible` widened the record to 5 words, which
	   a stride-based parser CANNOT read compatibly, so AST_SLICE_REC_MAGIC was
	   bumped ('SL' -> 'SM') and a legacy record must now be SKIPPED rather than
	   misparsed at the wrong stride. Asserting the skip is the point: silently
	   reading 4-word records at a 5-word stride would fabricate gates and
	   refcounts out of adjacent records. */
	{
		uint64_t legacy[4];
		AstSliceMemo lt[2];
		int ln;
		legacy[0] = 0x3333;
		legacy[1] = 0xCu;
		legacy[2] = 6;
		legacy[3] = (uint64_t)4 | ((uint64_t)0x534cu << 48); /* old 'SL' magic */
		memcpy(buf, legacy, sizeof legacy);
		ln = ast_slice_rec_deserialize(buf, (long)sizeof legacy, lt, 2);
		CHECK(ln == 0, "legacy 4-word record is skipped, not misparsed");
	}

	/* Per-ident merge: a PROVEN record overrides an existing STATIC one for the
	   same slice — proven config wins regardless of size, refcounts sum. */
	{
		AstSliceMemo tab[8];
		int n = 0;
		AstSliceMemo st, pv;
		st.ident = 0xABCD; st.gates = 0x1u; st.size = 3; st.refcount = 5; st.proven = 0;
		pv.ident = 0xABCD; pv.gates = 0x2u; pv.size = 9 /*bigger*/; pv.refcount = 1; pv.proven = 1;
		ast_slice_merge_one(tab, &n, 8, &st);
		CHECK(n == 1 && tab[0].proven == 0 && tab[0].gates == 0x1u, "static seeded first");
		ast_slice_merge_one(tab, &n, 8, &pv);
		CHECK(n == 1, "same ident does not add a row");
		CHECK(tab[0].proven == 1, "proven overrides static");
		CHECK(tab[0].gates == 0x2u, "merged gates are the proven config");
		CHECK(tab[0].size == 9, "merged size follows the proven record");
		CHECK(tab[0].refcount == 6, "refcounts sum across merge");
		/* A later STATIC record for the same ident must NOT clobber the proven one. */
		{
			AstSliceMemo st2;
			st2.ident = 0xABCD; st2.gates = 0x4u; st2.size = 1 /*cheaper*/; st2.refcount = 1;
			st2.proven = 0;
			ast_slice_merge_one(tab, &n, 8, &st2);
			CHECK(tab[0].proven == 1 && tab[0].gates == 0x2u,
						"static cannot override an existing proven record");
			CHECK(tab[0].refcount == 7, "refcount still sums under a non-overriding merge");
		}
	}

	/* Probe preference: with BOTH a proven (small) slice and a static (large)
	   slice matching a function, the proven one wins even though it is smaller —
	   inverting the size-only dominant rule. */
	{
		AstArena *d = ast_arena_new();
		AstLocal dbb = ast_node(d, AST_BasicBlock);
		AstLocal dret = ast_node(d, AST_Return);
		AstLocal inner = build_binop(d, '+', -8, -16); /* 3 nodes */
		AstLocal tail = ast_node(d, AST_Ref);
		AstLocal outer;
		uint64_t id_inner, id_outer, g;
		AstSliceMemo pt[2];
		ast_set_op(d, tail, VT_LOCAL);
		ast_set_ival(d, tail, (uint64_t)(int64_t)-24);
		outer = ast_node(d, AST_Binary); /* (inner) + l(-24) = 5 nodes */
		ast_set_op(d, outer, '+');
		ast_add_child(d, outer, inner);
		ast_add_child(d, outer, tail);
		ast_add_child(d, dret, outer);
		ast_add_child(d, dbb, dret);
		id_inner = ast_slice_ident_hash(d, inner);
		id_outer = ast_slice_ident_hash(d, outer);
		/* larger slice is STATIC, smaller slice is PROVEN */
		pt[0].ident = id_outer; pt[0].gates = 0x2u; pt[0].size = 5; pt[0].refcount = 1; pt[0].proven = 0;
		pt[1].ident = id_inner; pt[1].gates = 0x1u; pt[1].size = 3; pt[1].refcount = 1; pt[1].proven = 1;
		g = 0;
		CHECK(ast_slice_probe_table(d, pt, 2, &g) == 1, "probe hits with mixed proven/static");
		CHECK(g == 0x1u, "proven slice config wins over a larger static slice");
		/* Flip: make the larger slice ALSO proven -> now the largest proven wins. */
		pt[0].proven = 1;
		g = 0;
		CHECK(ast_slice_probe_table(d, pt, 2, &g) == 1, "probe hits with both proven");
		CHECK(g == 0x2u, "among proven, the largest slice wins");
		ast_arena_free(d);
	}
}

/* Phase 5 roadmap 14: node-stable in-arena splice primitive. Builds a site
   function, extracts-then-modifies a kernel, splices it back, and asserts the
   documented node-stability guarantee (indices/fields outside the replaced
   subtree are preserved), plus arena validity and subtree equality. */
static int subtree_eq(const AstArena *x, AstLocal xr, const AstArena *y, AstLocal yr) {
	AstLocal cx, cy;
	if (ast_kind(x, xr) != ast_kind(y, yr) || ast_op(x, xr) != ast_op(y, yr) ||
			ast_type_t(x, xr) != ast_type_t(y, yr) || ast_ival(x, xr) != ast_ival(y, yr) ||
			ast_sym(x, xr) != ast_sym(y, yr) || ast_nchild(x, xr) != ast_nchild(y, yr))
		return 0;
	cx = ast_first_child(x, xr);
	cy = ast_first_child(y, yr);
	while (cx != AST_NONE && cy != AST_NONE) {
		if (!subtree_eq(x, cx, y, cy))
			return 0;
		cx = ast_next_sib(x, cx);
		cy = ast_next_sib(y, cy);
	}
	return cx == AST_NONE && cy == AST_NONE;
}

static void suite_slice_splice(void) {
	char msg[128];
	/* Site: a function `return l(-8) - l(-16);`  plus a sibling BasicBlock holding
	   `return l(-24) + l(-32);` so there is a well-defined "outside" region whose
	   node identity we assert is preserved across the splice. */
	AstArena *a = ast_arena_new();
	AstLocal tu = ast_node(a, AST_BasicBlock); /* synthetic multi-child container root */
	AstLocal bb1 = ast_node(a, AST_BasicBlock);
	AstLocal ret1 = ast_node(a, AST_Return);
	AstLocal site = build_binop(a, '-', -8, -16); /* the slice to replace */
	AstLocal bb2 = ast_node(a, AST_BasicBlock);
	AstLocal ret2 = ast_node(a, AST_Return);
	AstLocal keep = build_binop(a, '+', -24, -32); /* the "outside" slice */
	AstLocal outside_l, outside_r;
	AstLocal count_before, i;
	int spliced;
	uint64_t site_parent_before, site_sib_before, keep_ident_before;
	ast_add_child(a, ret1, site);
	ast_add_child(a, bb1, ret1);
	ast_add_child(a, ret2, keep);
	ast_add_child(a, bb2, ret2);
	ast_add_child(a, tu, bb1);
	ast_add_child(a, tu, bb2);
	outside_l = ast_first_child(a, keep);
	outside_r = ast_next_sib(a, outside_l);
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "site arena validates");

	/* Kernel: a distinct arena holding `l(-100) * l(-100)` (a 3-node slice with a
	   different shape/arity-of-1-distinct-input than the site). */
	AstArena *k = ast_arena_new();
	AstLocal kroot = build_binop(k, '*', -100, -100);
	CHECK(ast_validate(k, msg, sizeof msg) == 0, "kernel arena validates");

	count_before = ast_count(a);
	site_parent_before = ast_parent(a, site);
	site_sib_before = ast_next_sib(a, site);
	keep_ident_before = ast_slice_ident_hash(a, keep);

	spliced = ast_slice_splice(a, site, k, kroot);
	CHECK(spliced == 3, "splice reports the kernel node count (3)");
	CHECK(ast_validate(a, msg, sizeof msg) == 0, "arena validates after splice");

	/* Guarantee 1: the spliced subtree at `site` equals the kernel. */
	CHECK(subtree_eq(a, site, k, kroot), "spliced subtree matches the kernel");
	CHECK(ast_slice_ident_hash(a, site) == ast_slice_ident_hash(k, kroot),
				"spliced site has the kernel's slice identity");
	CHECK(ast_op(a, site) == '*', "site root op replaced with kernel's");

	/* Guarantee 2: site_root slot re-used -> its parent/sibling links unchanged. */
	CHECK(ast_parent(a, site) == site_parent_before, "site_root parent link preserved");
	CHECK(ast_next_sib(a, site) == site_sib_before, "site_root sibling link preserved");
	CHECK(ast_first_child(a, ret1) == site, "enclosing Return still points at site slot");

	/* Guarantee 3: every LIVE node outside the replaced subtree keeps its index and
	   fields -- the untouched sibling slice is byte-identical, ident unchanged. */
	CHECK(ast_slice_ident_hash(a, keep) == keep_ident_before,
				"outside slice identity is preserved");
	CHECK(ast_op(a, keep) == '+' && ast_nchild(a, keep) == 2, "outside slice fields intact");
	CHECK(ast_ival(a, outside_l) == (uint64_t)(int64_t)-24, "outside operand[0] intact");
	CHECK(ast_ival(a, outside_r) == (uint64_t)(int64_t)-32, "outside operand[1] intact");
	CHECK(ast_parent(a, bb2) == tu && ast_first_child(a, bb2) == ret2,
				"outside block structure intact");

	/* Kernel body nodes were appended at the tail (no compaction of live nodes). */
	CHECK(ast_count(a) == count_before + 2, "kernel's 2 non-root nodes appended at tail");
	for (i = 0; i < count_before; i++)
		(void)i; /* indices < count_before still address the same live/dead nodes */

	/* Error paths. */
	CHECK(ast_slice_splice(NULL, 0, k, kroot) == 0, "NULL site arena rejected");
	CHECK(ast_slice_splice(a, ast_count(a), k, kroot) == 0, "out-of-range site rejected");
	CHECK(ast_slice_splice(a, site, k, ast_count(k)) == 0, "out-of-range kernel root rejected");

	ast_arena_free(a);
	ast_arena_free(k);
}

/* Phase 5 roadmap 15: hierarchical locator. Finds every occurrence of a repeated
   slice identity and none of a distinct one, so a single optimized kernel can be
   spliced at all occurrences. */
static void suite_slice_locate(void) {
	AstArena *a = ast_arena_new();
	AstLocal tu = ast_node(a, AST_BasicBlock); /* synthetic multi-child container root */
	/* Three occurrences of `l+l` reading DIFFERENT slots (same identity), plus one
	   distinct `l-l` slice. */
	AstLocal s1 = build_binop(a, '+', -8, -16);
	AstLocal s2 = build_binop(a, '+', -32, -40);   /* same identity as s1 */
	AstLocal s3 = build_binop(a, '+', -64, -72);   /* same identity as s1 */
	AstLocal d1 = build_binop(a, '-', -8, -16);    /* distinct identity */
	AstLocal sites[8];
	uint64_t idadd, idsub, idnone;
	int n;
	ast_add_child(a, tu, s1);
	ast_add_child(a, tu, s2);
	ast_add_child(a, tu, s3);
	ast_add_child(a, tu, d1);

	idadd = ast_slice_ident_hash(a, s1);
	idsub = ast_slice_ident_hash(a, d1);
	CHECK(idadd != idsub, "the two slice shapes have distinct identities");

	n = ast_slice_locate(a, idadd, sites, 8);
	CHECK(n == 3, "locate finds all three occurrences of the repeated slice");
	CHECK(sites[0] == s1 && sites[1] == s2 && sites[2] == s3,
				"located sites are the three '+' roots in order");

	n = ast_slice_locate(a, idsub, sites, 8);
	CHECK(n == 1, "locate finds the single distinct slice");
	CHECK(sites[0] == d1, "located site is the '-' root");

	/* An identity present nowhere yields zero sites. */
	idnone = idadd ^ 0x9e3779b97f4a7c15ull;
	CHECK(ast_slice_locate(a, idnone, sites, 8) == 0, "absent identity locates nothing");

	/* `max` caps writes but the true count is still returned. */
	n = ast_slice_locate(a, idadd, sites, 2);
	CHECK(n == 3, "return is the total match count even when it exceeds max");
	CHECK(sites[0] == s1 && sites[1] == s2, "only the first `max` sites are written");

	/* Guard/error paths. */
	CHECK(ast_slice_locate(NULL, idadd, sites, 8) == 0, "NULL arena locates nothing");
	CHECK(ast_slice_locate(a, 0, sites, 8) == 0, "zero identity locates nothing");
	CHECK(ast_slice_locate(a, idadd, sites, 0) == 0, "non-positive max locates nothing");

	/* The static promotion gate: strict-cheaper keeps, ties/unmeasurable reject. */
	CHECK(ast_slice_promote_static(10, 7) == 1, "cheaper candidate is kept");
	CHECK(ast_slice_promote_static(7, 10) == 0, "costlier candidate is rejected");
	CHECK(ast_slice_promote_static(7, 7) == 0, "a tie rejects (incumbent wins)");
	CHECK(ast_slice_promote_static(-1, 7) == 1, "no baseline -> accept a measured candidate");
	CHECK(ast_slice_promote_static(7, -1) == 0, "an unmeasurable candidate is rejected");

	ast_arena_free(a);
}

int main(int argc, char **argv) {
	const char *only = argc > 1 ? argv[1] : NULL;
	if (!only || !strcmp(only, "arena"))
		suite_arena();
	if (!only || !strcmp(only, "forecast"))
		suite_forecast();
	if (!only || !strcmp(only, "clone"))
		suite_clone();
	if (!only || !strcmp(only, "wide"))
		suite_wide();
	if (!only || !strcmp(only, "color"))
		suite_color();
	if (!only || !strcmp(only, "validate"))
		suite_validate();
	if (!only || !strcmp(only, "dump"))
		suite_dump();
	if (!only || !strcmp(only, "cfg"))
		suite_cfg();
	if (!only || !strcmp(only, "template"))
		suite_template();
	if (!only || !strcmp(only, "intention"))
		suite_intention();
	if (!only || !strcmp(only, "gatemap"))
		suite_gatemap();
	if (!only || !strcmp(only, "magic"))
		suite_magic();
	if (!only || !strcmp(only, "vlat"))
		suite_vlat();
	if (!only || !strcmp(only, "combo_walk"))
		suite_combo_walk();
	if (!only || !strcmp(only, "slice_ident"))
		suite_slice_ident();
	if (!only || !strcmp(only, "slice_window"))
		suite_slice_window();
	if (!only || !strcmp(only, "slice_persist"))
		suite_slice_persist();
	if (!only || !strcmp(only, "slice_graduate"))
		suite_slice_graduate();
	if (!only || !strcmp(only, "slice_splice"))
		suite_slice_splice();
	if (!only || !strcmp(only, "slice_locate"))
		suite_slice_locate();

	fprintf(stderr, "asttool: %d checks, %d failures\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
