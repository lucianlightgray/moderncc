#ifndef MCC_SLICE_INLINE_H
#define MCC_SLICE_INLINE_H

/* --- D4b step 1: slice-level inlining of a graftable leaf callee ---------- *
 *
 * This is deliberately not OpFunctionCall. SPIR-V compute has no recursion and
 * the corpus has a ~130-function SCC, so a real call boundary cannot be taken
 * in one increment. What can be taken in one increment is the case where the
 * callee has no boundary at all: a body that is exactly
 *
 *     BasicBlock { Return <pure expression over the parameters> }
 *
 * Such a call is a macro with a stack frame. Substituting the argument
 * subtrees for the parameter refs turns the AST_Invoke into an ordinary
 * expression node, and every consumer downstream -- the frame predicate, the
 * CPU reference in ast_eval_slice.h/mccslice.h, and the SPIR-V builder in
 * mccgpu.h -- sees a tree with no AST_Invoke in it. That is the whole point:
 * the board's hard precondition is that the CPU reference has NO AST_Invoke
 * case, so any arm that leaves an Invoke in the tree for one executor and not
 * the other is a vacuous differential. Rewriting the shared tree instead makes
 * both arms the same tree by construction, and neither arm can silently refuse
 * what the other accepted.
 *
 * Every restriction below exists because dropping it would make the graft
 * wrong in a way the differential cannot see (both executors would run the
 * same wrong tree):
 *
 *   direct callee only   an indirect callee has no device answer anywhere in
 *                        the plan, and no body to graft. Refused, not guessed.
 *   allow_load = 0       the callee expression may not touch memory. A leaf
 *                        that loads reads a host address, which a frame-scoped
 *                        space cannot resolve.
 *   one Return, one BB   no control flow, no stores, no second exit.
 *   recorded param slots the arena carries a body, not a signature, so the
 *                        argument-to-parameter mapping has to be recovered
 *                        from frame offsets. The hook supplies the callee's
 *                        own offsets in declaration order, as the arch layer
 *                        recorded them, and the used set must equal that set
 *                        exactly -- an unused parameter, a wide slot or a
 *                        reference to anything that is not a parameter is
 *                        REFUSED. This used to hard-code -8, -16, ..., which is
 *                        x86-64's layout; arm64 puts parameters above the frame
 *                        pointer and ascending, so every leaf was refused there
 *                        and nothing was ever grafted. A wrong mapping would
 *                        swap operands identically in both executors and the
 *                        differential would stay green, so this rule is
 *                        checked rather than assumed.
 *   Convert wrappers     C converts each argument to the parameter type and
 *                        the result to the return type. Both conversions are
 *                        materialised as AST_Convert nodes rather than left
 *                        implicit, so the graft computes what the call
 *                        computed and not merely something of the right shape.
 *
 * The callee body is not in the caller's arena, so the host supplies it
 * through mcc_slice_leaf_hook. Nothing is inlined when the hook is unset. */

#define MCC_SLICE_INL_MAXPARAM 4
#define MCC_SLICE_INL_MAXNODE 48
#define MCC_SLICE_INL_RECNODE 512
#define MCC_SLICE_INL_EXPAND 200000

typedef struct MccSliceLeaf {
	AstArena *a;
	AstLocal expr;
	int nparam;
	int nodes;
	int nodemax;
	int32_t off[MCC_SLICE_INL_MAXPARAM];
	int ptype[MCC_SLICE_INL_MAXPARAM];
} MccSliceLeaf;

/* poff/pnparam return the callee's own incoming-parameter offsets in
 * declaration order, as the arch layer recorded them. The scan below needs them
 * because the frame layout is not portable and this header may not ask which
 * target it is: MCC_TARGET_* in a conditional outside src/arch/ is what
 * target-gate-invariant forbids. */
static AstArena *(*mcc_slice_leaf_hook)(AstArena *a, AstLocal inv,
																				AstLocal *root, int32_t *poff,
																				int *pnparam);
static long mcc_slice_inl_n;
static long mcc_slice_inl_seen;
static long mcc_slice_inl_rec;
static long mcc_slice_inl_bail;
static long mcc_slice_inl_expand;
static long mcc_slice_inl_expand_tot;
static int mcc_slice_inl_depth_max;
static long mcc_slice_inl_expand_max = MCC_SLICE_INL_EXPAND;
/* The argument-to-parameter mapping is recovered from frame offsets, and a
 * wrong recovery would swap operands identically in both executors, so the
 * graft has to be readable by a human before it is trusted. Set
 * MCC_SLICE_INL_DUMP=1 and check an asymmetric callee. */
static int mcc_slice_inl_dump;

static int mcc_slice_has_invoke(AstArena *c, AstLocal n) {
	AstLocal k;
	if (n == AST_NONE)
		return 0;
	if (ast_kind(c, n) == AST_Invoke)
		return 1;
	for (k = ast_first_child(c, n); k != AST_NONE; k = ast_next_sib(c, k))
		if (mcc_slice_has_invoke(c, k))
			return 1;
	return 0;
}

static int mcc_slice_inl_body_ok(AstArena *c, AstLocal n) {
	uint32_t nc, k;
	if (n == AST_NONE)
		return 0;
	switch (ast_kind(c, n)) {
	case AST_Invoke: {
		AstLocal cref;
		int rt = ast_type_t(c, n);
		nc = ast_nchild(c, n);
		if (nc < 1 || nc - 1 > MCC_SLICE_INL_MAXPARAM)
			return 0;
		cref = ast_child(c, n, 0);
		if (cref == AST_NONE || ast_kind(c, cref) != AST_Ref || !ast_sym(c, cref))
			return 0;
		if (ast_bad_type(rt) || is_float(rt) || !ast_eval_slice_intt(rt))
			return 0;
		for (k = 1; k < nc; k++)
			if (!mcc_slice_inl_body_ok(c, ast_child(c, n, k)))
				return 0;
		return 1;
	}
	case AST_If:
		if (ast_nchild(c, n) != 3 || (ast_op(c, n) != 5 && ast_op(c, n) != 7))
			return 0;
		return mcc_slice_inl_body_ok(c, ast_child(c, n, 0)) &&
					 mcc_slice_inl_body_ok(c, ast_child(c, n, 1)) &&
					 mcc_slice_inl_body_ok(c, ast_child(c, n, 2));
	case AST_Binary:
	case AST_Unary:
	case AST_Convert: {
		int uop = ast_op(c, n);
		if (!mcc_slice_has_invoke(c, n))
			return ast_eval_slice_kind_ok(c, n, 0);
		nc = ast_nchild(c, n);
		if (nc < 1 || !ast_eval_slice_wtype(c, n))
			return 0;
		if (ast_kind(c, n) == AST_Unary && uop != '-' && uop != TOK_NEG &&
				uop != '~' && uop != '!')
			return 0;
		if (ast_kind(c, n) == AST_Binary && nc != 2 && uop != TOK_LAND &&
				uop != TOK_LOR)
			return 0;
		if (ast_kind(c, n) == AST_Convert && nc != 1)
			return 0;
		for (k = 0; k < nc; k++) {
			AstLocal ch = ast_child(c, n, k);
			if (ast_eval_slice_ftype(c, ch) || is_float(ast_type_t(c, ch)))
				return 0;
			if (!mcc_slice_inl_body_ok(c, ch))
				return 0;
		}
		return 1;
	}
	default:
		return ast_eval_slice_kind_ok(c, n, 0);
	}
}

static int mcc_slice_leaf_walk(AstArena *c, AstLocal n, MccSliceLeaf *L) {
	AstLocal k;
	if (++L->nodes > L->nodemax)
		return 0;
	if (ast_kind(c, n) == AST_Ref) {
		int r = ast_op(c, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int32_t o = (int32_t)(int64_t)ast_ival(c, n);
			int t = ast_type_t(c, n);
			int i;
			for (i = 0; i < L->nparam; i++)
				if (L->off[i] == o)
					break;
			if (i == L->nparam) {
				if (L->nparam >= MCC_SLICE_INL_MAXPARAM)
					return 0;
				if (ast_bad_type(t) || is_float(t) || !ast_eval_slice_intt(t))
					return 0;
				L->off[L->nparam] = o;
				L->ptype[L->nparam] = t;
				L->nparam++;
			} else if (L->ptype[i] != t) {
				return 0;
			}
		}
	}
	for (k = ast_first_child(c, n); k != AST_NONE; k = ast_next_sib(c, k))
		if (!mcc_slice_leaf_walk(c, k, L))
			return 0;
	return 1;
}

static int mcc_slice_leaf_scan_rec(AstArena *c, AstLocal root, MccSliceLeaf *L,
                                   const int32_t *poff, int pnparam,
                                   int allow_invoke) {
	AstLocal s, e;
	int i, j;
	memset(L, 0, sizeof *L);
	L->nodemax = allow_invoke ? MCC_SLICE_INL_RECNODE : MCC_SLICE_INL_MAXNODE;
	if (!c)
		return 0;
	if (root == AST_NONE || root >= ast_count(c))
		return 0;
	if (ast_kind(c, root) != AST_BasicBlock)
		return 0;
	s = ast_first_child(c, root);
	if (s == AST_NONE || ast_next_sib(c, s) != AST_NONE)
		return 0;
	if (ast_kind(c, s) != AST_Return || ast_nchild(c, s) != 1)
		return 0;
	e = ast_first_child(c, s);
	if (allow_invoke ? !mcc_slice_inl_body_ok(c, e)
									 : !ast_eval_slice_kind_ok(c, e, 0))
		return 0;
	if (!mcc_slice_leaf_walk(c, e, L))
		return 0;
	if (!ast_eval_slice_wtype(c, e))
		return 0;
	/* Put index i on declaration parameter i by matching the offsets the walk
	 * collected against the ones the arch layer recorded for this callee, and
	 * reject the leaf if the two sets are not equal.
	 *
	 * This used to be `L->off[i] != -8 * (i + 1)` after a descending sort, which
	 * is x86-64's layout written as if it were universal: parameters below the
	 * frame pointer at -8, -16, ... arm64 puts them above it and ascending --
	 * `mix3(int, int)` reports 160 and 168 -- so every candidate was refused
	 * there and nothing was ever grafted (`invoke-seen=4 invoke-inlined=0`).
	 * Matching recorded offsets is both portable and stricter than the old
	 * stride test: it pins each slot to a specific declared parameter instead of
	 * inferring the mapping from an assumed order. */
	if (poff) {
		if (pnparam != L->nparam)
			return 0;
		for (i = 0; i < L->nparam; i++) {
			int found = -1;
			for (j = i; j < L->nparam; j++)
				if (L->off[j] == poff[i]) {
					found = j;
					break;
				}
			if (found < 0)
				return 0;
			if (found != i) {
				int32_t to = L->off[i];
				int tt = L->ptype[i];
				L->off[i] = L->off[found];
				L->ptype[i] = L->ptype[found];
				L->off[found] = to;
				L->ptype[found] = tt;
			}
		}
	} else {
		/* No signature available -- slicerun rebuilds callee bodies from arena
		 * dumps, which carry nodes and not parameter lists. Fall back to the one
		 * ordering rule that holds on every layout mcc targets: parameter 0 sits
		 * closest to the frame base and the rest follow at stride 8, whichever
		 * side of it they are on. x86-64 gives -8, -16, ...; arm64 gives 160,
		 * 168, ... and |off| ascending puts both in declaration order. Ordering
		 * by the signed value instead is what pinned this to x86-64 and refused
		 * every arm64 leaf. */
		for (i = 0; i < L->nparam; i++)
			for (j = i + 1; j < L->nparam; j++) {
				int32_t ai = L->off[i] < 0 ? -L->off[i] : L->off[i];
				int32_t aj = L->off[j] < 0 ? -L->off[j] : L->off[j];
				if (aj < ai) {
					int32_t to = L->off[i];
					int tt = L->ptype[i];
					L->off[i] = L->off[j];
					L->ptype[i] = L->ptype[j];
					L->off[j] = to;
					L->ptype[j] = tt;
				}
			}
		for (i = 1; i < L->nparam; i++) {
			int32_t a0 = L->off[0] < 0 ? -L->off[0] : L->off[0];
			int32_t ai = L->off[i] < 0 ? -L->off[i] : L->off[i];
			if (ai != a0 + 8 * i)
				return 0;
		}
	}
	L->a = c;
	L->expr = e;
	return 1;
}

static int mcc_slice_leaf_scan(AstArena *c, AstLocal root, MccSliceLeaf *L,
                               const int32_t *poff, int pnparam) {
	return mcc_slice_leaf_scan_rec(c, root, L, poff, pnparam, 0);
}

static AstLocal mcc_slice_conv(AstArena *a, AstLocal v, int t) {
	AstLocal cv;
	if (v == AST_NONE)
		return AST_NONE;
	if (ast_type_t(a, v) == t)
		return v;
	cv = ast_node(a, AST_Convert);
	ast_set_type(a, cv, t, 0);
	ast_add_child(a, cv, v);
	return cv;
}

static AstLocal mcc_slice_graft(AstArena *a, AstArena *c, AstLocal n,
																const MccSliceLeaf *L, const AstLocal *arg) {
	AstLocal g, k;
	if (L && ast_kind(c, n) == AST_Ref) {
		int r = ast_op(c, n);
		if ((r & VT_VALMASK) == VT_LOCAL && !(r & VT_SYM)) {
			int32_t o = (int32_t)(int64_t)ast_ival(c, n);
			int i;
			for (i = 0; i < L->nparam; i++)
				if (L->off[i] == o)
					return mcc_slice_conv(
							a, mcc_slice_graft(a, a, arg[i], NULL, NULL), L->ptype[i]);
			return AST_NONE;
		}
	}
	g = ast_node(a, ast_kind(c, n));
	ast_set_op(a, g, ast_op(c, n));
	ast_copy_type(a, g, c, n);
	ast_set_ival(a, g, ast_ival(c, n));
	ast_set_fbits(a, g, ast_fbits(c, n));
	ast_set_sym(a, g, ast_sym(c, n));
	if (ast_wide_r2(c, n) != AST_R2_NONE)
		ast_set_wide(a, g, ast_wide_hi(c, n), ast_wide_r2(c, n));
	for (k = ast_first_child(c, n); k != AST_NONE; k = ast_next_sib(c, k)) {
		AstLocal d = mcc_slice_graft(a, c, k, L, arg);
		if (d == AST_NONE)
			return AST_NONE;
		ast_add_child(a, g, d);
	}
	return g;
}

static void mcc_slice_become(AstArena *a, AstLocal dst, AstLocal src) {
	AstLocal c, next;
	ast_set_kind(a, dst, ast_kind(a, src));
	ast_set_op(a, dst, ast_op(a, src));
	ast_copy_type(a, dst, a, src);
	ast_set_ival(a, dst, ast_ival(a, src));
	ast_set_fbits(a, dst, ast_fbits(a, src));
	ast_set_sym(a, dst, ast_sym(a, src));
	if (ast_wide_r2(a, src) != AST_R2_NONE)
		ast_set_wide(a, dst, ast_wide_hi(a, src), ast_wide_r2(a, src));
	c = ast_first_child(a, src);
	ast_clear_children(a, src);
	ast_clear_children(a, dst);
	for (; c != AST_NONE; c = next) {
		next = ast_next_sib(a, c);
		ast_add_child(a, dst, c);
	}
}

static void mcc_slice_plant_bailout(AstArena *a, AstLocal inv) {
	ast_clear_children(a, inv);
	ast_set_kind(a, inv, AST_Bailout);
	ast_set_op(a, inv, 0);
	ast_set_ival(a, inv, 0);
	ast_set_fbits(a, inv, 0);
	ast_set_sym(a, inv, 0);
	mcc_slice_inl_bail++;
}

static int mcc_slice_resolvable(AstArena *a, AstLocal inv) {
	AstLocal croot = AST_NONE;
	int32_t cpoff[MCC_SLICE_INL_MAXPARAM];
	int cpn = 0;
	AstArena *c;
	if (!mcc_slice_leaf_hook)
		return 0;
	c = mcc_slice_leaf_hook(a, inv, &croot, cpoff, &cpn);
	return c && c != a && croot != AST_NONE;
}

/* Rewrite one AST_Invoke in place. Returns 1 if the node is no longer an
 * Invoke. */
static int mcc_slice_inline_depth(AstArena *a, AstLocal inv, int depth) {
	AstLocal arg[MCC_SLICE_INL_MAXPARAM], cref, g, croot = AST_NONE;
	int32_t cpoff[MCC_SLICE_INL_MAXPARAM];
	int cpn = 0;
	MccSliceLeaf L;
	AstArena *c;
	AstLocal n0, nn, j;
	int rec = mcc_slice_inl_depth_max > 0;
	int rt = ast_type_t(a, inv);
	int nargs, i;
	if (ast_kind(a, inv) != AST_Invoke)
		return 0;
	nargs = (int)ast_nchild(a, inv) - 1;
	if (nargs < 0 || nargs > MCC_SLICE_INL_MAXPARAM)
		return 0;
	cref = ast_child(a, inv, 0);
	if (cref == AST_NONE || ast_kind(a, cref) != AST_Ref)
		return 0;
	if (ast_bad_type(rt) || is_float(rt) || !ast_eval_slice_intt(rt))
		return 0;
	if (!mcc_slice_leaf_hook)
		return 0;
	c = mcc_slice_leaf_hook(a, inv, &croot, cpoff, &cpn);
	if (!c || c == a)
		return 0;
	/* cpn == 0 means the provider had no parameter list to give (slicerun,
	 * rebuilding from a dump). Pass NULL so the scan derives the order rather
	 * than comparing against an empty signature and refusing everything. */
	if (!mcc_slice_leaf_scan_rec(c, croot, &L, cpn ? cpoff : NULL, cpn, rec))
		return 0;
	if (L.nparam != nargs)
		return 0;
	for (i = 0; i < nargs; i++) {
		arg[i] = ast_child(a, inv, (uint32_t)i + 1);
		if (arg[i] == AST_NONE || !ast_eval_slice_kind_ok(a, arg[i], 1))
			return 0;
	}
	n0 = ast_count(a);
	ast_clear_children(a, inv);
	g = mcc_slice_graft(a, c, L.expr, &L, arg);
	if (g == AST_NONE) {
		ast_add_child(a, inv, cref);
		for (i = 0; i < nargs; i++)
			ast_add_child(a, inv, arg[i]);
		return 0;
	}
	g = mcc_slice_conv(a, g, rt);
	mcc_slice_become(a, inv, g);
	mcc_slice_inl_n++;
	if (depth > 0)
		mcc_slice_inl_rec++;
	nn = ast_count(a);
	mcc_slice_inl_expand += (long)(nn - n0);
	for (j = n0; j < nn; j++) {
		if (ast_kind(a, j) != AST_Invoke)
			continue;
		if (depth + 1 >= mcc_slice_inl_depth_max ||
				mcc_slice_inl_expand >= mcc_slice_inl_expand_max) {
			if (mcc_slice_resolvable(a, j))
				mcc_slice_plant_bailout(a, j);
			continue;
		}
		mcc_slice_inline_depth(a, j, depth + 1);
	}
	if (mcc_slice_inl_dump) {
		char buf[4096];
		ast_dump(a, inv, buf, sizeof buf);
		fprintf(stderr, "[graft] node=%u nparam=%d depth=%d\n%s\n", (unsigned)inv,
						L.nparam, depth, buf);
	}
	return 1;
}

static int mcc_slice_inline_at(AstArena *a, AstLocal inv) {
	return mcc_slice_inline_depth(a, inv, 0);
}

static void mcc_slice_inline_arena(AstArena *a) {
	AstLocal nn = ast_count(a), n;
	mcc_slice_inl_expand = 0;
	for (n = 0; n < nn; n++) {
		if (ast_kind(a, n) != AST_Invoke)
			continue;
		mcc_slice_inl_seen++;
		mcc_slice_inline_at(a, n);
	}
	mcc_slice_inl_expand_tot += mcc_slice_inl_expand;
}

#endif
