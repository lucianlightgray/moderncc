#if MCC_CONFIG_OPTIMIZER && (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))
#if MCC_REPLAY_IR

enum { RIR_T_OP = 0, RIR_T_RBEGIN, RIR_T_REND, RIR_T_MARK };

#define RIR_PT_NONE (-1)
#define RIR_PT_HERE (-2)
#define RIR_SHIFT 64

/* C2 research probe, compile-time OFF. Driving the tree's emitter speculatively
   from a reconstructed arena is not containable in-process: an emit that errors
   leaves compiler state the surrounding save/restore does not cover, and 17 of
   276 corpus files stop compiling with it on. Build -DMCC_REPLAY_IR_C2=1 to
   measure it; never ship it on. */
#ifndef MCC_REPLAY_IR_C2
#define MCC_REPLAY_IR_C2 0
#endif

typedef struct RirOp {
	int tag;
	int rkind;
	int jidx;
	int lbl, lbl2;
	int pt;
	int rval;
	JrnOp p;
} RirOp;

typedef struct RirChainEnt {
	int addr;
	int label;
} RirChainEnt;

typedef struct RirMark {
	int tag;
	int kind;
	int val;
	int at;
} RirMark;

int rir_env;
int rir_active;
int rir_started;

static const char *rir_out;
static RirOp *rir_ops;
static int rir_n, rir_cap;
static RirMark *rir_marks;
static int rir_markn, rir_markcap;
static int rir_stack[256];
static int rir_stackn;
static int rir_unbal;
static int rir_ovf;
static int rir_fail_op, rir_fail_kind;
static int *rir_jlbl, *rir_jlbl2, *rir_jpt;
static int rir_jcap;
static int rir_nlbl;
static int rir_fallback;
static RirChainEnt *rir_cmap;
static int rir_cmapn, rir_cmapcap;
static int *rir_lblhead;
static int rir_lblcap;
static int *rir_ptaddr;
static int rir_ptcap;
static int *rir_vslbl, *rir_vslbl2;
static int rir_vscap;
static int *rir_jsvlbl;
static int rir_jmpsv_fb;
static long rir_tot_jmpsv, rir_tot_jmpsv_fb;
static long rir_tot_fn, rir_tot_faithful, rir_tot_ops, rir_tot_regions;
static long rir_tot_unbal, rir_tot_ovf;
static long rir_tot_fallback, rir_tot_labels, rir_tot_jumps;
static long rir_tot_fallback_fn;
static long rir_tot_fb_chain, rir_tot_fb_point;
static int rir_delta;
static long rir_tot_shift_ok, rir_tot_shift_bad, rir_tot_shift_skip;
static const char *rir_shift_verdict = "-";
static int rir_open_chains;
static long rir_tot_shift_open;
static int rir_shift_failop, rir_shift_failkind, rir_shift_diff;
static long rir_reghist[RIR_R_COUNT];

static const char *rir_region_name(int k) {
	static const char *const n[RIR_R_COUNT] = {
			"none",	 "if",		"then",		"else", "while", "do",
			"for",	 "switch", "ternary", "landor", "call", "cond",
			"body",	 "synth"};
	return k >= 0 && k < RIR_R_COUNT ? n[k] : "?";
}

static RirOp *rir_new(int tag) {
	RirOp *o;
	if (rir_n >= rir_cap) {
		rir_cap = rir_cap ? rir_cap * 2 : 256;
		rir_ops = mcc_realloc(rir_ops, (size_t)rir_cap * sizeof *rir_ops);
	}
	o = &rir_ops[rir_n++];
	memset(o, 0, sizeof *o);
	o->tag = tag;
	return o;
}

static void rir_mark_v(int tag, int kind, int val) {
	RirMark *m;
	if (rir_markn >= rir_markcap) {
		rir_markcap = rir_markcap ? rir_markcap * 2 : 128;
		rir_marks = mcc_realloc(rir_marks, (size_t)rir_markcap * sizeof *rir_marks);
	}
	m = &rir_marks[rir_markn++];
	m->tag = tag;
	m->kind = kind;
	m->val = val;
	m->at = jrn_n;
	rir_tot_regions++;
	if (tag != RIR_T_MARK && kind >= 0 && kind < RIR_R_COUNT)
		rir_reghist[kind]++;
}

static void rir_mark(int tag, int kind) { rir_mark_v(tag, kind, 0); }

void rir_rbegin(int kind) {
	if (!rir_active)
		return;
	if (rir_stackn >= (int)(sizeof rir_stack / sizeof rir_stack[0])) {
		rir_ovf = 1;
		return;
	}
	rir_stack[rir_stackn++] = kind;
	rir_mark(RIR_T_RBEGIN, kind);
}

void rir_rend_to(int kind) {
	int i, found = 0;
	if (!rir_active)
		return;
	for (i = rir_stackn - 1; i >= 0; i--)
		if (rir_stack[i] == kind) {
			found = 1;
			break;
		}
	if (!found) {
		rir_unbal = 1;
		return;
	}
	while (rir_stackn > 0) {
		int k = rir_stack[--rir_stackn];
		rir_mark(RIR_T_REND, k);
		if (k == kind)
			return;
	}
}

void rir_rcond_done(void) {
	int i, open = 0;
	if (!rir_active)
		return;
	for (i = rir_stackn - 1; i >= 0; i--)
		if (rir_stack[i] == RIR_R_COND) {
			open = 1;
			break;
		}
	if (!open)
		return;
	rir_rend_to(RIR_R_COND);
	rir_rbegin(rir_stackn && rir_stack[rir_stackn - 1] == RIR_R_IF ? RIR_R_THEN
																																 : RIR_R_BODY);
}

void rir_mark_pt(int kind) {
	if (!rir_active)
		return;
	rir_mark(RIR_T_MARK, kind);
}

void rir_mark_val(int kind, int val) {
	if (!rir_active)
		return;
	rir_mark_v(RIR_T_MARK, kind, val);
}

void rir_reset(void) {
	rir_n = 0;
	rir_markn = 0;
	rir_stackn = 0;
	rir_unbal = 0;
	rir_ovf = 0;
	rir_fail_op = -1;
	rir_fail_kind = -1;
	rir_fallback = 0;
	rir_nlbl = 0;
}

static int rir_cmap_find(int addr) {
	int i;
	if (!addr)
		return -1;
	for (i = 0; i < rir_cmapn; i++)
		if (rir_cmap[i].addr == addr)
			return rir_cmap[i].label;
	return -1;
}

static void rir_cmap_drop(int addr) {
	int i;
	if (!addr)
		return;
	for (i = 0; i < rir_cmapn; i++)
		if (rir_cmap[i].addr == addr) {
			rir_cmap[i] = rir_cmap[--rir_cmapn];
			return;
		}
}

static void rir_cmap_bind(int addr, int label) {
	if (!addr || label < 0)
		return;
	rir_cmap_drop(addr);
	if (rir_cmapn >= rir_cmapcap) {
		rir_cmapcap = rir_cmapcap ? rir_cmapcap * 2 : 64;
		rir_cmap = mcc_realloc(rir_cmap, (size_t)rir_cmapcap * sizeof *rir_cmap);
	}
	rir_cmap[rir_cmapn].addr = addr;
	rir_cmap[rir_cmapn].label = label;
	rir_cmapn++;
}

static int rir_chain_label(int addr) {
	if (!addr)
		return -1;
	return rir_cmap_find(addr);
}

static int rir_point_of(int addr) {
	int lo = 0, hi = jrn_n - 1;
	while (lo <= hi) {
		int mid = lo + (hi - lo) / 2;
		if (jrn_ops[mid].ind_pre == addr)
			return mid;
		if (jrn_ops[mid].ind_pre < addr)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return RIR_PT_NONE;
}

static void rir_resolve(void) {
	int i;
	if (jrn_n > rir_jcap) {
		rir_jcap = jrn_n;
		rir_jlbl = mcc_realloc(rir_jlbl, (size_t)rir_jcap * sizeof *rir_jlbl);
		rir_jlbl2 = mcc_realloc(rir_jlbl2, (size_t)rir_jcap * sizeof *rir_jlbl2);
		rir_jpt = mcc_realloc(rir_jpt, (size_t)rir_jcap * sizeof *rir_jpt);
		rir_jsvlbl = mcc_realloc(rir_jsvlbl, (size_t)rir_jcap * sizeof *rir_jsvlbl);
	}
	if (jrn_vsn > rir_vscap) {
		rir_vscap = jrn_vsn;
		rir_vslbl = mcc_realloc(rir_vslbl, (size_t)rir_vscap * sizeof *rir_vslbl);
		rir_vslbl2 = mcc_realloc(rir_vslbl2, (size_t)rir_vscap * sizeof *rir_vslbl2);
	}
	rir_cmapn = 0;
	rir_nlbl = 0;
	rir_fallback = 0;
	rir_jmpsv_fb = 0;
	for (i = 0; i < jrn_vsn; i++)
		rir_vslbl[i] = rir_vslbl2[i] = -1;
	for (i = 0; i < jrn_n; i++) {
		JrnOp *o = &jrn_ops[i];
		int in = 0, out = 0, L = -1, k;
		rir_jlbl[i] = -1;
		rir_jlbl2[i] = -1;
		rir_jpt[i] = RIR_PT_NONE;
		rir_jsvlbl[i] = -1;
		for (k = 0; k < o->vs_n; k++) {
			SValue *v = &jrn_vs[o->vs_off + k];
			int vv = v->r & VT_VALMASK;
			if (vv == VT_JMP || vv == VT_JMPI) {
				rir_tot_jmpsv++;
				rir_vslbl[o->vs_off + k] = rir_chain_label((int)v->c.i);
				if (v->c.i && rir_vslbl[o->vs_off + k] < 0) {
					rir_jmpsv_fb++;
					rir_tot_jmpsv_fb++;
				}
			} else if (v->r == VT_CMP) {
				rir_tot_jmpsv++;
				rir_vslbl[o->vs_off + k] = rir_chain_label(v->jtrue);
				rir_vslbl2[o->vs_off + k] = rir_chain_label(v->jfalse);
				if ((v->jtrue && rir_vslbl[o->vs_off + k] < 0) ||
						(v->jfalse && rir_vslbl2[o->vs_off + k] < 0)) {
					rir_jmpsv_fb++;
					rir_tot_jmpsv_fb++;
				}
			}
		}
		if (o->sv_slot < 0) {
			int vv = o->svarg.r & VT_VALMASK;
			if (((vv == VT_JMP || vv == VT_JMPI) && o->svarg.c.i) ||
					(o->svarg.r == VT_CMP && (o->svarg.jtrue || o->svarg.jfalse)))
				rir_jmpsv_fb++;
		}
		switch (o->kind) {
		case JOP_JMP:
			in = o->a0;
			out = o->ret;
			goto chain;
		case JOP_JMPCOND:
			in = o->a1;
			out = o->ret;
			goto chain;
		chain:
			rir_tot_jumps++;
			L = in ? rir_cmap_find(in) : -1;
			if (in && L < 0) {
				rir_fallback++;
				rir_tot_fb_chain++;
				break;
			}
			if (L < 0)
				L = rir_nlbl++;
			rir_jlbl[i] = L;
			rir_cmap_drop(in);
			rir_cmap_bind(out, L);
			break;
		case JOP_JMPAPPEND: {
			int Ln, Lt;
			rir_tot_jumps++;
			Ln = o->a0 ? rir_cmap_find(o->a0) : -1;
			Lt = o->a1 ? rir_cmap_find(o->a1) : -1;
			if ((o->a0 && Ln < 0) || (o->a1 && Lt < 0)) {
				rir_fallback++;
				rir_tot_fb_chain++;
				break;
			}
			if (o->a0) {
				if (Ln < 0)
					Ln = rir_nlbl++;
				L = Ln;
			} else {
				if (Lt < 0)
					Lt = rir_nlbl++;
				L = Lt;
			}
			rir_jlbl[i] = Ln;
			rir_jlbl2[i] = Lt;
			rir_cmap_drop(o->a0);
			rir_cmap_drop(o->a1);
			rir_cmap_bind(o->ret, L);
			break;
		}
		case JOP_GSYMADDR:
			rir_tot_jumps++;
			L = o->a0 ? rir_cmap_find(o->a0) : -1;
			if (o->a0 && L < 0) {
				rir_fallback++;
				rir_tot_fb_chain++;
				break;
			}
			rir_jlbl[i] = L;
			rir_cmap_drop(o->a0);
			rir_jpt[i] = o->a1 == o->ind_pre ? RIR_PT_HERE : rir_point_of(o->a1);
			if (rir_jpt[i] == RIR_PT_NONE) {
				rir_fallback++;
				rir_tot_fb_point++;
			}
			break;
		case JOP_JMPADDR:
			rir_tot_jumps++;
			rir_jpt[i] = o->a0 == o->ind_pre ? RIR_PT_HERE : rir_point_of(o->a0);
			if (rir_jpt[i] == RIR_PT_NONE) {
				rir_fallback++;
				rir_tot_fb_point++;
			}
			break;
		default:
			break;
		}
	}
	rir_tot_labels += rir_nlbl;
	rir_tot_fallback += rir_fallback;
	if (rir_fallback)
		rir_tot_fallback_fn++;
	if (rir_nlbl > rir_lblcap) {
		rir_lblcap = rir_nlbl < 64 ? 64 : rir_nlbl;
		rir_lblhead = mcc_realloc(rir_lblhead, (size_t)rir_lblcap * sizeof *rir_lblhead);
	}
	if (jrn_n > rir_ptcap) {
		rir_ptcap = jrn_n;
		rir_ptaddr = mcc_realloc(rir_ptaddr, (size_t)rir_ptcap * sizeof *rir_ptaddr);
	}
}

static void rir_build(void) {
	int i, m = 0;
	rir_n = 0;
	rir_resolve();
	for (i = 0; i <= jrn_n; i++) {
		while (m < rir_markn && rir_marks[m].at <= i) {
			RirOp *o = rir_new(rir_marks[m].tag);
			o->rkind = rir_marks[m].kind;
			o->rval = rir_marks[m].val;
			o->jidx = -1;
			o->lbl = -1;
			o->lbl2 = -1;
			o->pt = RIR_PT_NONE;
			m++;
		}
		if (i < jrn_n) {
			RirOp *o = rir_new(RIR_T_OP);
			o->p = jrn_ops[i];
			o->jidx = i;
			o->lbl = rir_jlbl[i];
			o->lbl2 = rir_jlbl2[i];
			o->pt = rir_jpt[i];
		}
	}
}

static AstArena *rir_arena;
static AstLocal rir_sh[VSTACK_SIZE + 1];
static unsigned char rir_shtype[VSTACK_SIZE + 1];
static int rir_shn;
static AstLocal rir_bb[64];
static int rir_bbn;
static AstLocal rir_cf[64];
static int rir_cfn;
static int rir_arena_mismatch;
static int rir_after_ret;
static long rir_tot_arena_fn, rir_tot_arena_nodes, rir_tot_arena_hash_eq;
static long rir_tot_arena_cmp, rir_tot_arena_count_eq;
static long rir_tot_tree_nodes, rir_tot_arena_cmp_nodes;
static long rir_tot_c2_try, rir_tot_c2_ok, rir_tot_c2_bytes, rir_tot_c2_len,
		rir_tot_c2_err;
static char rir_c2_msg[256];
static long rir_tot_c2_invalid;

static void rir_c2_sink(void *opaque, const char *msg) {
	(void)opaque;
	snprintf(rir_c2_msg, sizeof rir_c2_msg, "%s", msg ? msg : "?");
}
static long rir_kindhist[AST_KIND_COUNT], rir_treekindhist[AST_KIND_COUNT];

static AstLocal rir_leaf(const SValue *sv) {
	int is_const = (sv->r & (VT_VALMASK | VT_LVAL)) == VT_CONST;
	AstLocal n = ast_node(rir_arena, is_const ? AST_Literal : AST_Ref);
	ast_set_op(rir_arena, n, sv->r);
	ast_set_type(rir_arena, n, sv->type.t, (uint64_t)(uintptr_t)sv->type.ref);
	ast_set_ival(rir_arena, n, (uint64_t)sv->c.i);
	ast_set_wide(rir_arena, n, ast_sv_hi(sv),
							 sv->r2 >= VT_CONST ? (unsigned)VT_CONST : (unsigned)sv->r2);
	ast_set_sym(rir_arena, n, (uint64_t)(uintptr_t)sv->sym);
	return n;
}

static void rir_stmt(AstLocal n) {
	if (n == AST_NONE || !rir_bbn)
		return;
	ast_add_child(rir_arena, rir_bb[rir_bbn - 1], n);
}

static AstLocal rir_pop(void) {
	if (rir_shn <= 0)
		return AST_NONE;
	return rir_sh[--rir_shn];
}

static void rir_push(AstLocal n) {
	if (rir_shn > VSTACK_SIZE)
		return;
	rir_shtype[rir_shn] = 0;
	rir_sh[rir_shn++] = n;
}

/* A computed node's result type is not in the op that produced it -- it is in
   the NEXT op's snapshot, as the SValue occupying that slot. Push it untyped and
   stamp it at the next boundary. */
static void rir_push_typed(AstLocal n) {
	if (rir_shn > VSTACK_SIZE)
		return;
	rir_shtype[rir_shn] = 1;
	rir_sh[rir_shn++] = n;
}

/* Reconcile the shadow stack against the state the op actually saw. The
   recorded snapshot is the dataflow made explicit: any slot the model does not
   already hold is materialised as a leaf straight from its SValue, and any slot
   the model holds past the recorded depth is dropped. Divergences are counted
   rather than smoothed over -- rir_arena_mismatch is the honest quality signal
   for this reconstruction, the same role fix= plays for replay. */
static void rir_stamp_types(const JrnOp *o) {
	int k, want;
	if (o->vs_n < 0)
		return;
	want = o->vs_n - ast_base_depth;
	for (k = 0; k < rir_shn && k < want; k++) {
		const SValue *v;
		if (!rir_shtype[k])
			continue;
		v = &jrn_vs[o->vs_off + ast_base_depth + k];
		ast_set_type(rir_arena, rir_sh[k], v->type.t,
								 (uint64_t)(uintptr_t)v->type.ref);
		rir_shtype[k] = 0;
	}
}

static void rir_reconcile(const JrnOp *o) {
	int want, k;
	if (o->vs_n < 0)
		return;
	rir_stamp_types(o);
	want = o->vs_n - ast_base_depth;
	if (want < 0)
		want = 0;
	if (want > VSTACK_SIZE)
		want = VSTACK_SIZE;
	while (rir_shn > want) {
		AstLocal d = rir_pop();
		uint16_t dk = d == AST_NONE ? AST_Poison : ast_kind(rir_arena, d);
		if (dk == AST_Store || dk == AST_Invoke)
			rir_stmt(d);
	}
	if (rir_after_ret && rir_shn == 0)
		return;
	for (k = rir_shn; k < want; k++) {
		rir_push(rir_leaf(&jrn_vs[o->vs_off + ast_base_depth + k]));
		if (k < want - 1)
			rir_arena_mismatch++;
	}
}

static void rir_op_effect(const RirOp *ro) {
	const JrnOp *o = &ro->p;
	int k;
	switch (o->kind) {
	case JOP_GENOP:
	case JOP_OPI:
	case JOP_OPL:
	case JOP_OPF: {
		AstLocal b = rir_pop(), a = rir_pop(), n;
		if (a == AST_NONE || b == AST_NONE) {
			rir_arena_mismatch++;
			rir_push(ast_node(rir_arena, AST_Poison));
			break;
		}
		n = ast_node(rir_arena, AST_Binary);
		ast_set_op(rir_arena, n, o->a0);
		ast_add_child(rir_arena, n, a);
		ast_add_child(rir_arena, n, b);
		rir_push(n);
		break;
	}
	case JOP_VSTORE: {
		AstLocal v = rir_pop(), t = rir_pop(), n;
		if (v == AST_NONE || t == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Store);
		ast_add_child(rir_arena, n, t);
		ast_add_child(rir_arena, n, v);
		rir_stmt(n);
		{
			AstLocal mv = ast_node(rir_arena, AST_StoreVal);
			ast_set_type(rir_arena, mv, ast_type_t(rir_arena, v),
									 ast_type_ref(rir_arena, v));
			ast_set_ival(rir_arena, mv, (uint64_t)n);
			rir_push(mv);
		}
		break;
	}
	case JOP_CALL: {
		AstLocal n = ast_node(rir_arena, AST_Invoke);
		int na = o->a0;
		AstLocal args[32];
		if (na < 0 || na > 32) {
			rir_arena_mismatch++;
			na = 0;
		}
		for (k = na - 1; k >= 0; k--) {
			args[k] = rir_pop();
			if (args[k] == AST_NONE)
				rir_arena_mismatch++;
		}
		{
			AstLocal callee = rir_pop();
			if (callee == AST_NONE)
				rir_arena_mismatch++;
			else
				ast_add_child(rir_arena, n, callee);
		}
		for (k = 0; k < na; k++)
			if (args[k] != AST_NONE)
				ast_add_child(rir_arena, n, args[k]);
		rir_push_typed(n);
		break;
	}
	case JOP_VPOP: {
		AstLocal d = rir_pop();
		uint16_t dk = d == AST_NONE ? AST_Poison : ast_kind(rir_arena, d);
		if (dk == AST_Store || dk == AST_Invoke)
			rir_stmt(d);
		break;
	}
	case JOP_VSWAP:
		if (rir_shn >= 2) {
			AstLocal t = rir_sh[rir_shn - 1];
			rir_sh[rir_shn - 1] = rir_sh[rir_shn - 2];
			rir_sh[rir_shn - 2] = t;
		}
		break;
	case JOP_CVT_ITOF:
	case JOP_CVT_FTOF:
	case JOP_CVT_FTOI:
	case JOP_CVT_SXTW:
	case JOP_CVT_TRUNC32:
	case JOP_CVT_CSTI: {
		AstLocal a = rir_pop(), n;
		if (a == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Convert);
		ast_set_op(rir_arena, n, o->a0);
		ast_add_child(rir_arena, n, a);
		rir_push_typed(n);
		break;
	}
	case JOP_ADDROF: {
		AstLocal a = rir_pop(), n;
		if (a == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, n, '&');
		ast_add_child(rir_arena, n, a);
		rir_push_typed(n);
		break;
	}
	default:
		break;
	}
}

static int rir_cond_depth, rir_synth_depth, rir_call_depth;
static AstLocal rir_last_return = AST_NONE;

static void rir_mark_apply(const RirOp *ro) {
	AstLocal a, n;
	switch (ro->rkind) {
	case RIR_M_RETURN:
		n = ast_node(rir_arena, AST_Return);
		a = rir_shn ? rir_pop() : AST_NONE;
		if (a != AST_NONE)
			ast_add_child(rir_arena, n, a);
		rir_stmt(n);
		rir_last_return = n;
		rir_after_ret = 1;
		break;
	case RIR_M_RETJMP:
		if (rir_last_return != AST_NONE)
			ast_set_op(rir_arena, rir_last_return, ro->rval ? 1 : 0);
		break;
	case RIR_M_JUMP:
		rir_stmt(ast_node(rir_arena, AST_Jump));
		break;
	case RIR_M_LABEL:
		break;
	case RIR_M_LOAD:
		a = rir_pop();
		if (a == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Load);
		ast_add_child(rir_arena, n, a);
		rir_push_typed(n);
		break;
	case RIR_M_CONVERT:
		a = rir_pop();
		if (a == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Convert);
		ast_add_child(rir_arena, n, a);
		rir_push_typed(n);
		break;
	default:
		break;
	}
}

static void rir_region(const RirOp *ro) {
	rir_after_ret = 0;
	if (ro->tag == RIR_T_RBEGIN) {
		switch (ro->rkind) {
		case RIR_R_IF:
		case RIR_R_WHILE:
		case RIR_R_DO:
		case RIR_R_FOR:
		case RIR_R_SWITCH: {
			AstLocal n = ast_node(rir_arena, AST_If);
			AstLocal cond = rir_shn ? rir_pop() : AST_NONE;
			ast_set_op(rir_arena, n, ro->rkind);
			if (cond != AST_NONE)
				ast_add_child(rir_arena, n, cond);
			rir_stmt(n);
			if (rir_cfn < 64)
				rir_cf[rir_cfn++] = n;
			break;
		}
		case RIR_R_COND:
			rir_cond_depth++;
			break;
		case RIR_R_SYNTH:
			rir_synth_depth++;
			break;
		case RIR_R_CALL:
			rir_call_depth++;
			break;
		case RIR_R_BODY:
		case RIR_R_THEN:
		case RIR_R_ELSE: {
			AstLocal bb = ast_node(rir_arena, AST_BasicBlock);
			if (rir_cfn)
				ast_add_child(rir_arena, rir_cf[rir_cfn - 1], bb);
			if (rir_bbn < 64)
				rir_bb[rir_bbn++] = bb;
			break;
		}
		default:
			break;
		}
		return;
	}
	switch (ro->rkind) {
	case RIR_R_COND:
		if (rir_cond_depth)
			rir_cond_depth--;
		break;
	case RIR_R_SYNTH:
		if (rir_synth_depth)
			rir_synth_depth--;
		break;
	case RIR_R_CALL:
		if (rir_call_depth)
			rir_call_depth--;
		break;
	case RIR_R_BODY:
	case RIR_R_THEN:
	case RIR_R_ELSE:
		if (rir_bbn > 1)
			rir_bbn--;
		break;
	case RIR_R_IF:
	case RIR_R_WHILE:
	case RIR_R_DO:
	case RIR_R_FOR:
	case RIR_R_SWITCH:
		if (rir_cfn)
			rir_cfn--;
		break;
	default:
		break;
	}
}

#if MCC_REPLAY_IR_C2
/* ast_validate checks link consistency, not arity. ast_replay_bb assumes the
   shapes the hooks always produce -- an If with a condition and at least one
   block, a Store with two children, and so on -- and walks off the end when a
   reconstruction is short. Reject those here so an incomplete arena is an
   honest skip instead of a segfault in the emitter. */
static int rir_unsafe(const char *why, AstLocal n, uint32_t nc) {
	snprintf(rir_c2_msg, sizeof rir_c2_msg, "arity %s n=%u nc=%u op=%d", why,
					 (unsigned)n, (unsigned)nc, ast_op(rir_arena, n));
	return 0;
}

static int rir_emit_safe(void) {
	AstLocal n;
	for (n = 0; n < ast_count(rir_arena); n++) {
		uint32_t nc = ast_nchild(rir_arena, n);
		switch (ast_kind(rir_arena, n)) {
		case AST_If:
			if (nc < 2)
				return rir_unsafe("If", n, nc);
			break;
		case AST_Store:
			if (nc != 2)
				return rir_unsafe("Store", n, nc);
			break;
		case AST_Binary:
			if (nc != 2)
				return rir_unsafe("Binary", n, nc);
			break;
		case AST_Convert:
			if (nc != 1)
				return rir_unsafe("Convert", n, nc);
			break;
		case AST_Load:
			if (nc != 1)
				return rir_unsafe("Load", n, nc);
			break;
		case AST_Unary:
			if (nc != 1)
				return rir_unsafe("Unary", n, nc);
			break;
		case AST_Invoke: {
			AstLocal callee;
			if (nc < 1)
				return rir_unsafe("Invoke-nc", n, nc);
			callee = ast_child(rir_arena, n, 0);
			if (callee == AST_NONE || ast_type_t(rir_arena, callee) == 0)
				return rir_unsafe("Invoke-callee-untyped", n, nc);
			/* gfunc_call walks the callee's signature Sym for the ABI; a callee
			   with no type ref, or one that is not a function, faults there. */
			if (ast_type_ref(rir_arena, callee) == 0)
				return rir_unsafe("Invoke-callee-noref", n, nc);
			if ((ast_type_t(rir_arena, callee) & VT_BTYPE) != VT_FUNC)
				return rir_unsafe("Invoke-callee-notfunc", n, nc);
			break;
		}
		case AST_Return:
			if (nc > 1)
				return rir_unsafe("Return", n, nc);
			break;
		default:
			break;
		}
	}
	return 1;
}
#endif

static void rir_to_arena(void) {
	int i;
	if (!rir_arena)
		rir_arena = ast_arena_new();
	else
		ast_arena_reset(rir_arena);
	rir_shn = 0;
	rir_cfn = 0;
	rir_bbn = 0;
	rir_last_return = AST_NONE;
	rir_after_ret = 0;
	rir_cond_depth = 0;
	rir_synth_depth = 0;
	rir_call_depth = 0;
	rir_arena_mismatch = 0;
	rir_bb[rir_bbn++] = ast_node(rir_arena, AST_BasicBlock);
	for (i = 0; i < rir_n; i++) {
		RirOp *ro = &rir_ops[i];
		if (ro->tag == RIR_T_MARK) {
			if (!rir_cond_depth && !rir_synth_depth && !rir_call_depth) {
				int j;
				for (j = i + 1; j < rir_n; j++)
					if (rir_ops[j].tag == RIR_T_OP) {
						rir_stamp_types(&rir_ops[j].p);
						break;
					}
				rir_mark_apply(ro);
			}
			continue;
		}
		if (ro->tag != RIR_T_OP) {
			rir_region(ro);
			continue;
		}
		if (rir_cond_depth)
			continue;
		rir_reconcile(&ro->p);
		rir_op_effect(ro);
	}
	while (rir_shn > 0) {
		AstLocal d = rir_pop();
		uint16_t dk = d == AST_NONE ? AST_Poison : ast_kind(rir_arena, d);
		if (dk == AST_Store || dk == AST_Invoke)
			rir_stmt(d);
	}
}

static int rir_pt_addr(const RirOp *o, int fallback) {
	if (o->pt == RIR_PT_HERE)
		return ind;
	if (o->pt >= 0 && rir_ptaddr[o->pt] >= 0)
		return rir_ptaddr[o->pt];
	return fallback;
}

static int rir_issue_jump(RirOp *o) {
	int t, nh;
	switch (o->p.kind) {
	case JOP_JMP:
		if (o->lbl < 0)
			return 0;
		nh = (gjmp)(rir_lblhead[o->lbl]);
		rir_lblhead[o->lbl] = nh;
		return 1;
	case JOP_JMPCOND:
		if (o->lbl < 0)
			return 0;
		nh = (gjmp_cond)(o->p.a0, rir_lblhead[o->lbl]);
		rir_lblhead[o->lbl] = nh;
		return 1;
	case JOP_JMPAPPEND: {
		int n = o->p.a0 ? (o->lbl >= 0 ? rir_lblhead[o->lbl] : 0) : 0;
		int tt = o->p.a1 ? (o->lbl2 >= 0 ? rir_lblhead[o->lbl2] : 0) : 0;
		if ((o->p.a0 && o->lbl < 0) || (o->p.a1 && o->lbl2 < 0))
			return 0;
		nh = (gjmp_append)(n, tt);
		if (o->p.a0) {
			if (o->lbl >= 0)
				rir_lblhead[o->lbl] = nh;
			if (o->lbl2 >= 0)
				rir_lblhead[o->lbl2] = 0;
		} else if (o->lbl2 >= 0) {
			rir_lblhead[o->lbl2] = nh;
		}
		return 1;
	}
	case JOP_GSYMADDR:
		if (o->p.a0 && o->lbl < 0)
			return 0;
		if (o->pt == RIR_PT_NONE)
			return 0;
		t = o->lbl >= 0 ? rir_lblhead[o->lbl] : 0;
		(gsym_addr)(t, rir_pt_addr(o, o->p.a1));
		if (o->lbl >= 0)
			rir_lblhead[o->lbl] = 0;
		return 1;
	case JOP_JMPADDR:
		if (o->pt == RIR_PT_NONE)
			return 0;
		(gjmp_addr)(rir_pt_addr(o, o->p.a0));
		return 1;
	default:
		return 0;
	}
}

static void rir_run(void) {
	int i;
	rir_fail_op = -1;
	rir_fail_kind = -1;
	for (i = 0; i < rir_nlbl; i++)
		rir_lblhead[i] = 0;
	for (i = 0; i < jrn_n; i++)
		rir_ptaddr[i] = -1;
	for (i = 0; i < rir_n; i++) {
		RirOp *o = &rir_ops[i];
		if (o->tag != RIR_T_OP)
			continue;
		nocode_wanted = o->p.nocode;
		loc = o->p.loc_pre;
		nb_temp_local_vars = o->p.ntlv_pre;
		if (o->p.vs_n >= 0) {
			if (o->p.vs_n) {
				int k;
				memcpy(vstack, jrn_vs + o->p.vs_off,
							 (size_t)o->p.vs_n * sizeof(SValue));
				if (rir_delta)
					for (k = 0; k < o->p.vs_n; k++) {
						int L = rir_vslbl[o->p.vs_off + k];
						int L2 = rir_vslbl2[o->p.vs_off + k];
						if (vstack[k].r == VT_CMP) {
							if (L >= 0)
								vstack[k].jtrue = rir_lblhead[L];
							if (L2 >= 0)
								vstack[k].jfalse = rir_lblhead[L2];
						} else if (L >= 0) {
							vstack[k].c.i = rir_lblhead[L];
						}
					}
			}
			vtop = vstack + o->p.vs_n - 1;
		}
		if (ind != o->p.ind_pre + rir_delta) {
			rir_fail_op = i;
			rir_fail_kind = o->p.kind;
			return;
		}
		jrn_fc_cur = o->p.fc_off;
		jrn_fc_end = o->p.fc_off + o->p.fc_n;
		jrn_pred_have = o->p.swpred != 0;
		jrn_pred_cur = o->p.swpred - 1;
		if (o->jidx >= 0)
			rir_ptaddr[o->jidx] = ind;
		if (!rir_issue_jump(o)) {
			jrn_issue(&o->p);
		}
		if (rir_env >= 2)
			fprintf(stderr, "[rir-op] %-4d %-10s pre=%d post=%d now=%d vs=%d\n", i,
							jrn_op_name(o->p.kind), o->p.ind_pre, o->p.ind_post, ind,
							o->p.vs_n);
	}
}

static void rir_emit_line(const char *verdict, int ops, int regions) {
	const char *vf = mcc_state && mcc_state->current_filename
											 ? mcc_state->current_filename
											 : "?";
	if (rir_out && rir_out[0]) {
		FILE *f = fopen(rir_out, "a");
		if (f) {
			fprintf(f,
							"%s\t%s\t%s\tops=%d\tregions=%d\tlbl=%d\tfb=%d\tunbal=%d\tovf=%d\n",
							verdict, vf, funcname, ops, regions, rir_nlbl, rir_fallback,
							rir_unbal, rir_ovf);
			fclose(f);
		}
	} else {
		fprintf(stderr,
						"[rir-verify] %s\t%s\t%s\tops=%d\tregions=%d\tlbl=%d\tfb=%d\tunbal=%d\t"
						"ovf=%d\tshift=%s\tsfop=%s@%d\tsdiff=%d\topen=%d\n",
						verdict, vf, funcname, ops, regions, rir_nlbl, rir_fallback,
						rir_unbal, rir_ovf, rir_shift_verdict,
						rir_shift_failkind >= 0 ? jrn_op_name(rir_shift_failkind) : "-",
						rir_shift_failop, rir_shift_diff, rir_open_chains);
	}
}

static int rir_blame(int diff_off) {
	int i;
	int at = ast_body_ind_sv + diff_off;
	for (i = 0; i < rir_n; i++) {
		if (rir_ops[i].tag != RIR_T_OP)
			continue;
		if (at >= rir_ops[i].p.ind_pre && at < rir_ops[i].p.ind_post)
			return i;
	}
	return -1;
}

void rir_verify(void) {
	Section *rsec = cur_text_section->reloc;
	int rel1 = rsec ? (int)rsec->data_offset : 0;
	int orig_ind = ind, orig_rsym = rsym;
	int body_len = orig_ind - ast_body_ind_sv;
	int rel_len = rel1 - (int)ast_reloc0_sv;
	unsigned char *orig, *orig_rel, *repl = NULL;
	int new_len_fin = 0;
	SValue *vsave;
	int saved_loc = loc, saved_anon = anon_sym, saved_nocode = nocode_wanted;
	int saved_func_alloca = mcc_state->cg_func_alloca;
	int saved_vn = (int)(vtop - vstack + 1);
	int faithful = 0, errored = 0;
	int nops = 0, nregions = 0, i;
	char vbuf[48];
	const char *verdict;
	jmp_buf outer;
	int outer_en = mcc_state->error_set_jmp_enabled;
	int saved_nberr = mcc_state->nb_errors;
	void (*sv_efunc)(void *, const char *) = mcc_state->error_func;
	void *sv_eop = mcc_state->error_opaque;
	unsigned char sv_warn = mcc_state->warn_none;
	Sym *saved_free = sym_free_first;
	int saved_floor = stk_data_floor;
	uint64_t saved_pin = ast_pinned_regs;
	int saved_ntlv = nb_temp_local_vars;
	struct temp_local_variable saved_tlv[MAX_TEMP_LOCAL_VARIABLE_NUMBER];
	int sv_ast_active, sv_ast_capture, sv_ast_replaying;

	rir_active = 0;
	rir_tot_fn++;
	rir_shift_verdict = "-";
	rir_shift_failop = -1;
	rir_shift_failkind = -1;
	rir_shift_diff = -1;
	rir_open_chains = 0;
	rir_build();
	if (rir_env >= 4) {
		rir_to_arena();
		rir_tot_arena_fn++;
		rir_tot_arena_nodes += ast_count(rir_arena);
		if (rir_env >= 6) {
			static char db[8192];
			ast_dump(rir_arena, ast_root(rir_arena), db, sizeof db);
			fprintf(stderr, "[rir-dump] %s RIR:\n%s\n", funcname, db);
			if (ast_cur && ast_replay_ok(ast_cur)) {
				AstLocal q;
				ast_dump(ast_cur, ast_root(ast_cur), db, sizeof db);
				fprintf(stderr, "[rir-dump] %s TREE:\n%s\n", funcname, db);
				for (q = 0; q < ast_count(rir_arena) && q < ast_count(ast_cur); q++) {
					if (ast_kind(rir_arena, q) == ast_kind(ast_cur, q) &&
							ast_op(rir_arena, q) == ast_op(ast_cur, q) &&
							ast_type_t(rir_arena, q) == ast_type_t(ast_cur, q) &&
							ast_type_ref(rir_arena, q) == ast_type_ref(ast_cur, q) &&
							ast_ival(rir_arena, q) == ast_ival(ast_cur, q) &&
							ast_sym(rir_arena, q) == ast_sym(ast_cur, q) &&
							ast_nchild(rir_arena, q) == ast_nchild(ast_cur, q))
						continue;
					fprintf(stderr,
									"[rir-diff] n%u rir(%s op=%d t=%d ref=%llx iv=%llu sym=%llx "
									"nc=%u) tree(%s op=%d t=%d ref=%llx iv=%llu sym=%llx nc=%u)\n",
									q, ast_kind_name(ast_kind(rir_arena, q)), ast_op(rir_arena, q),
									ast_type_t(rir_arena, q),
									(unsigned long long)ast_type_ref(rir_arena, q),
									(unsigned long long)ast_ival(rir_arena, q),
									(unsigned long long)ast_sym(rir_arena, q),
									ast_nchild(rir_arena, q), ast_kind_name(ast_kind(ast_cur, q)),
									ast_op(ast_cur, q), ast_type_t(ast_cur, q),
									(unsigned long long)ast_type_ref(ast_cur, q),
									(unsigned long long)ast_ival(ast_cur, q),
									(unsigned long long)ast_sym(ast_cur, q),
									ast_nchild(ast_cur, q));
				}
			}
		}
		if (ast_try_active && ast_cur && ast_replay_ok(ast_cur)) {
			rir_tot_arena_cmp++;
			rir_tot_tree_nodes += ast_count(ast_cur);
			rir_tot_arena_cmp_nodes += ast_count(rir_arena);
			{
				AstLocal q;
				for (q = 0; q < ast_count(rir_arena); q++)
					rir_kindhist[ast_kind(rir_arena, q) % AST_KIND_COUNT]++;
				for (q = 0; q < ast_count(ast_cur); q++)
					rir_treekindhist[ast_kind(ast_cur, q) % AST_KIND_COUNT]++;
			}
			if (ast_count(rir_arena) == ast_count(ast_cur))
				rir_tot_arena_count_eq++;
			if (ast_intention_hash(rir_arena, ast_root(rir_arena)) ==
					ast_intention_hash(ast_cur, ast_root(ast_cur)))
				rir_tot_arena_hash_eq++;
		}
	}
	for (i = 0; i < rir_n; i++) {
		if (rir_ops[i].tag == RIR_T_OP)
			nops++;
		else
			nregions++;
	}
	rir_tot_ops += nops;
	if (rir_unbal)
		rir_tot_unbal++;
	if (rir_ovf)
		rir_tot_ovf++;
	if (nops == 0) {
		rir_emit_line("rempty", 0, nregions);
		return;
	}
	if (jrn_bad) {
		rir_emit_line("rrewind", nops, nregions);
		return;
	}

	orig = mcc_malloc(body_len > 0 ? (size_t)body_len : 1);
	memcpy(orig, cur_text_section->data + ast_body_ind_sv, (size_t)body_len);
	orig_rel = mcc_malloc(rel_len > 0 ? (size_t)rel_len : 1);
	if (rel_len > 0)
		memcpy(orig_rel, rsec->data + ast_reloc0_sv, (size_t)rel_len);
	vsave = mcc_malloc(sizeof(SValue) * (VSTACK_SIZE + 1));
	memcpy(vsave, vstack - 1, sizeof(SValue) * (VSTACK_SIZE + 1));
	memcpy(saved_tlv, arr_temp_local_vars, sizeof saved_tlv);

	ind = ast_body_ind_sv;
	rsym = 0;
	if (rsec)
		rsec->data_offset = ast_reloc0_sv;
	nocode_wanted = 0;
	mcc_state->warn_none = 1;
	sym_free_first = NULL;
	sv_ast_active = ast_active;
	sv_ast_capture = ast_capture;
	sv_ast_replaying = ast_replaying;
	ast_active = 0;
	ast_capture = 0;
	ast_replaying = 1;
	ast_fconst_i = 0;
	ast_locrec_i = 0;
	jrn_replaying = 1;
	mcc_state->cg_func_alloca = 0;
	memcpy(outer, mcc_state->error_jmp_buf, sizeof(jmp_buf));
	mcc_state->error_func = ast_error_sink;
	stk_data_floor = nb_stk_data;
	if (setjmp(mcc_state->error_jmp_buf) == 0) {
		mcc_state->error_set_jmp_enabled = 1;
		rir_run();
		if (rir_fail_op < 0) {
			int new_rel = rsec ? (int)rsec->data_offset : 0;
			int new_len = ind - ast_body_ind_sv;
			faithful = new_len == body_len &&
								 memcmp(cur_text_section->data + ast_body_ind_sv, orig,
												(size_t)body_len) == 0 &&
								 new_rel - (int)ast_reloc0_sv == rel_len &&
								 (rel_len == 0 ||
									ast_reloc_range_equiv(rsec->data + ast_reloc0_sv, orig_rel,
																				rel_len));
		}
	} else {
		errored = 1;
	}
	new_len_fin = ind - ast_body_ind_sv;
	if (new_len_fin > 0 && !faithful && !errored) {
		repl = mcc_malloc((size_t)new_len_fin);
		memcpy(repl, cur_text_section->data + ast_body_ind_sv, (size_t)new_len_fin);
	}

	if (rir_env >= 3 && faithful && body_len > 0) {
		int nraw = 0;
		for (i = 0; i < jrn_n; i++)
			if (jrn_ops[i].kind == JOP_RAW)
				nraw++;
		rir_shift_failop = -1;
		rir_shift_failkind = -1;
		rir_shift_diff = -1;
		if (nraw || rir_fallback || rir_jmpsv_fb) {
			rir_shift_verdict = "skip";
			rir_tot_shift_skip++;
		} else {
			int shift = RIR_SHIFT;
			int base2 = ast_body_ind_sv + shift;
			section_realloc(cur_text_section, base2 + body_len + 64);
			memset(cur_text_section->data + ast_body_ind_sv, 0,
						 (size_t)(shift + body_len));
			ind = base2;
			rsym = 0;
			if (rsec)
				rsec->data_offset = ast_reloc0_sv;
			nocode_wanted = 0;
			mcc_state->cg_func_alloca = 0;
			rir_delta = shift;
			if (setjmp(mcc_state->error_jmp_buf) == 0) {
				rir_run();
				rir_open_chains = 0;
				for (i = 0; i < rir_nlbl; i++)
					if (rir_lblhead[i])
						rir_open_chains++;
				if (rir_fail_op < 0 && ind - base2 == body_len &&
						memcmp(cur_text_section->data + base2, orig, (size_t)body_len) == 0 &&
						(rsec ? (int)rsec->data_offset - (int)ast_reloc0_sv : 0) == rel_len) {
					rir_shift_verdict = "ok";
					rir_tot_shift_ok++;
				} else {
					int k;
					rir_shift_verdict = rir_open_chains ? "open" : "bad";
					if (rir_open_chains) {
						rir_tot_shift_open++;
						rir_tot_shift_bad--;
					}
					rir_shift_failop = rir_fail_op;
					rir_shift_failkind = rir_fail_kind;
					if (rir_fail_op < 0 && ind - base2 == body_len) {
						for (k = 0; k < body_len; k++)
							if (cur_text_section->data[base2 + k] != orig[k]) {
								rir_shift_diff = k;
								break;
							}
						if (rir_shift_diff >= 0) {
							int bi = rir_blame(rir_shift_diff);
							if (bi >= 0) {
								rir_shift_failop = bi;
								rir_shift_failkind = rir_ops[bi].p.kind;
							}
						}
					}
					rir_tot_shift_bad++;
				}
			} else {
				rir_shift_verdict = "err";
				rir_tot_shift_bad++;
			}
			rir_delta = 0;
			rir_fail_op = -1;
		}
	}

#if MCC_REPLAY_IR_C2
	if (rir_env >= 5 && faithful && body_len > 0 && !rir_arena_mismatch) {
		rir_tot_c2_try++;
		ind = ast_body_ind_sv;
		rsym = 0;
		if (rsec)
			rsec->data_offset = ast_reloc0_sv;
		nocode_wanted = 0;
		mcc_state->cg_func_alloca = 0;
		ast_fconst_i = 0;
		ast_locrec_i = 0;
		nb_stk_data = stk_data_floor;
		memcpy(arr_temp_local_vars, saved_tlv, sizeof saved_tlv);
		nb_temp_local_vars = saved_ntlv;
		{
		Sym *c2_free = sym_free_first;
		Sym *c2_local = local_stack;
		int c2_anon = anon_sym;
		int c2_stk = nb_stk_data;
		int c2_loc = loc;
		int c2_nlabel = ast_rp_nlabel;
		int c2_lfloor = ast_rp_label_floor;
		int c2_ntlv = nb_temp_local_vars;
		struct temp_local_variable c2_tlv[MAX_TEMP_LOCAL_VARIABLE_NUMBER];
		memcpy(c2_tlv, arr_temp_local_vars, sizeof c2_tlv);
		ast_rp_nlabel = 0;
		ast_rp_label_floor = 0;
		ast_replaying = 0;
		sym_free_first = NULL;
		rir_c2_msg[0] = 0;
		memcpy(vstack - 1, vsave, sizeof(SValue) * (VSTACK_SIZE + 1));
		vtop = vstack + saved_vn - 1;
		loc = saved_loc;
		anon_sym = saved_anon;
		ast_pinned_regs = saved_pin;
		ast_rp_bsym = NULL;
		ast_rp_csym = NULL;
		ast_rp_switch = NULL;
		ast_temp_frontier = 1;
		mcc_state->error_func = rir_c2_sink;
		if (ast_validate(rir_arena, rir_c2_msg, sizeof rir_c2_msg) != 0 ||
				!rir_emit_safe()) {
			rir_tot_c2_invalid++;
			if (rir_env >= 5)
				fprintf(stderr, "[rir-c2] %s\tINVALID %s\n", funcname, rir_c2_msg);
		} else if (setjmp(mcc_state->error_jmp_buf) == 0) {
			ast_replay_body(rir_arena);
			if (ind - ast_body_ind_sv == body_len &&
					memcmp(cur_text_section->data + ast_body_ind_sv, orig,
								 (size_t)body_len) == 0)
				rir_tot_c2_ok++;
			else if (ind - ast_body_ind_sv == body_len)
				rir_tot_c2_bytes++;
			else {
				rir_tot_c2_len++;
				if (rir_env >= 5) {
					int q, gl = ind - ast_body_ind_sv;
					fprintf(stderr, "[rir-c2len] %s want=%d got=%d\n  parser:", funcname,
									body_len, gl);
					for (q = 0; q < body_len && q < 48; q++)
						fprintf(stderr, " %02x", orig[q]);
					fprintf(stderr, "\n  rir   :");
					for (q = 0; q < gl && q < 48; q++)
						fprintf(stderr, " %02x",
										cur_text_section->data[ast_body_ind_sv + q]);
					fprintf(stderr, "\n");
				}
			}
		} else {
			rir_tot_c2_err++;
			if (rir_env >= 5)
				fprintf(stderr, "[rir-c2] %s\t%s\n", funcname,
								rir_c2_msg[0] ? rir_c2_msg : "(no message)");
		}
		mcc_state->error_func = ast_error_sink;
		sym_free_first = c2_free;
		local_stack = c2_local;
		anon_sym = c2_anon;
		nb_stk_data = c2_stk;
		loc = c2_loc;
		ast_rp_nlabel = c2_nlabel;
		ast_rp_label_floor = c2_lfloor;
		nb_temp_local_vars = c2_ntlv;
		memcpy(arr_temp_local_vars, c2_tlv, sizeof c2_tlv);
		}
	}
#endif

	jrn_replaying = 0;
	mcc_state->cg_func_alloca = saved_func_alloca;
	ast_active = sv_ast_active;
	ast_capture = sv_ast_capture;
	ast_replaying = sv_ast_replaying;
	ast_fconst_i = 0;
	ast_locrec_i = 0;
	memcpy(mcc_state->error_jmp_buf, outer, sizeof(jmp_buf));
	mcc_state->error_set_jmp_enabled = outer_en;
	mcc_state->error_func = sv_efunc;
	mcc_state->error_opaque = sv_eop;
	nb_stk_data = stk_data_floor;
	stk_data_floor = saved_floor;
	mcc_state->nb_errors = saved_nberr;
	sym_free_first = saved_free;
	mcc_state->warn_none = sv_warn;

	memcpy(cur_text_section->data + ast_body_ind_sv, orig, (size_t)body_len);
	if (rel_len > 0)
		memcpy(rsec->data + ast_reloc0_sv, orig_rel, (size_t)rel_len);
	if (rsec)
		rsec->data_offset = rel1;
	ind = orig_ind;
	rsym = orig_rsym;
	loc = saved_loc;
	anon_sym = saved_anon;
	nocode_wanted = saved_nocode;
	ast_pinned_regs = saved_pin;
	nb_temp_local_vars = saved_ntlv;
	memcpy(arr_temp_local_vars, saved_tlv, sizeof saved_tlv);
	memcpy(vstack - 1, vsave, sizeof(SValue) * (VSTACK_SIZE + 1));
	vtop = vstack + saved_vn - 1;
	mcc_free(vsave);

	if (errored) {
		verdict = "rerror";
	} else if (rir_fail_op >= 0) {
		snprintf(vbuf, sizeof vbuf, "rdiverge:%s@%d", jrn_op_name(rir_fail_kind),
						 rir_fail_op);
		verdict = vbuf;
	} else if (faithful) {
		verdict = "rfaithful";
		rir_tot_faithful++;
	} else {
		int k, lim = new_len_fin < body_len ? new_len_fin : body_len;
		int d = -1, bi;
		for (k = 0; repl && k < lim; k++) {
			if (repl[k] != orig[k]) {
				d = k;
				break;
			}
		}
		if (d < 0)
			d = lim;
		bi = rir_blame(d);
		snprintf(vbuf, sizeof vbuf, "runfaithful:%s@%d",
						 bi >= 0 ? jrn_op_name(rir_ops[bi].p.kind)
										 : (new_len_fin == body_len ? "reloc" : "len"),
						 bi);
		verdict = vbuf;
	}
	rir_emit_line(verdict, nops, nregions);
	mcc_free(orig);
	mcc_free(orig_rel);
	mcc_free(repl);
}

static void rir_report(void) {
	int k;
	FILE *f = stderr;
	if (rir_out && rir_out[0]) {
		f = fopen(rir_out, "a");
		if (!f)
			f = stderr;
	}
	fprintf(f,
					"[rir-total] fn=%ld faithful=%ld ops=%ld regions=%ld labels=%ld "
					"jumps=%ld fallback=%ld fallbackfn=%ld fbchain=%ld fbpoint=%ld "
					"unbal=%ld ovf=%ld jmpsv=%ld jmpsvfb=%ld shiftok=%ld shiftbad=%ld "
					"shiftskip=%ld shiftopen=%ld arenafn=%ld arenanodes=%ld arenacmp=%ld "
					"arenacounteq=%ld arenahasheq=%ld treenodes=%ld cmpnodes=%ld "
					"c2try=%ld c2ok=%ld c2bytes=%ld c2len=%ld c2err=%ld c2invalid=%ld\n",
					rir_tot_fn, rir_tot_faithful, rir_tot_ops, rir_tot_regions,
					rir_tot_labels, rir_tot_jumps, rir_tot_fallback,
					rir_tot_fallback_fn, rir_tot_fb_chain, rir_tot_fb_point,
					rir_tot_unbal, rir_tot_ovf, rir_tot_jmpsv, rir_tot_jmpsv_fb,
					rir_tot_shift_ok, rir_tot_shift_bad, rir_tot_shift_skip,
					rir_tot_shift_open, rir_tot_arena_fn, rir_tot_arena_nodes,
					rir_tot_arena_cmp, rir_tot_arena_count_eq, rir_tot_arena_hash_eq,
					rir_tot_tree_nodes, rir_tot_arena_cmp_nodes, rir_tot_c2_try,
					rir_tot_c2_ok, rir_tot_c2_bytes, rir_tot_c2_len, rir_tot_c2_err,
					rir_tot_c2_invalid);
	fprintf(f, "[rir-kind]");
	for (k = 0; k < AST_KIND_COUNT; k++)
		if (rir_kindhist[k] || rir_treekindhist[k])
			fprintf(f, " %s=%ld/%ld", ast_kind_name((uint16_t)k), rir_kindhist[k],
							rir_treekindhist[k]);
	fprintf(f, "\n");
	fprintf(f, "[rir-region]");
	for (k = 1; k < RIR_R_COUNT; k++)
		if (rir_reghist[k])
			fprintf(f, " %s=%ld", rir_region_name(k), rir_reghist[k]);
	fprintf(f, "\n");
	if (f != stderr)
		fclose(f);
}

void rir_configure(void) {
	static int done;
	if (done)
		return;
	done = 1;
	rir_env = ast_env_int("MCC_REPLAY_IR", 0);
	rir_out = getenv("MCC_REPLAY_IR_OUT");
	if (rir_env)
		atexit(rir_report);
}

#endif
#endif
