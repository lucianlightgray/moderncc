#if MCC_CONFIG_OPTIMIZER && (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))
#if MCC_REPLAY_IR

enum { RIR_T_OP = 0, RIR_T_RBEGIN, RIR_T_REND, RIR_T_MARK };

#define RIR_NOEVAL_MASK 0x0000FFFF

#define RIR_PT_NONE (-1)
#define RIR_PT_HERE (-2)
#define RIR_SHIFT 64

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
	int rnocode;
	int rind;
	int rinop;
	long long rv1, rv2, rv3;
	int mvs_off, mvs_n;
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
	int nocode;
	int inop;
	long long v1, v2, v3;
	int at;
	int ind;
	int vs_off, vs_n;
} RirMark;

int rir_env;
int rir_active;
#define RIR_LOCREC_MAX 512
static int rir_locrec[RIR_LOCREC_MAX];
static int rir_locrec_pos[RIR_LOCREC_MAX];
static int rir_locrec_nc[RIR_LOCREC_MAX];
static int rir_locrec_n, rir_locrec_i;

void rir_loc_record(int loc_in) {
	if (rir_locrec_n >= RIR_LOCREC_MAX)
		return;
	rir_locrec_pos[rir_locrec_n] = ind;
	rir_locrec_nc[rir_locrec_n] = nocode_wanted;
	rir_locrec[rir_locrec_n++] = loc_in;
}

int rir_loc_replay(int *loc_out) {
	while (rir_locrec_i + 1 < rir_locrec_n && rir_locrec_pos[rir_locrec_i + 1] <= ind &&
	       ((rir_locrec_nc[rir_locrec_i] & RIR_NOEVAL_MASK) ||
	        rir_locrec_pos[rir_locrec_i + 1] > rir_locrec_pos[rir_locrec_i]))
		rir_locrec_i++;
	if (rir_locrec_i >= rir_locrec_n)
		return 0;
	*loc_out = rir_locrec[rir_locrec_i++];
	return 1;
}

static int rir_fcrec[RIR_LOCREC_MAX];
static int rir_fcrec_pos[RIR_LOCREC_MAX];
static int rir_fcrec_nc[RIR_LOCREC_MAX];
static int rir_fcrec_n, rir_fcrec_i;

void rir_hook_fconst_record(int c) {
	if (!rir_capture_live() || rir_fcrec_n >= RIR_LOCREC_MAX)
		return;
	rir_fcrec_pos[rir_fcrec_n] = ind;
	rir_fcrec_nc[rir_fcrec_n] = nocode_wanted;
	rir_fcrec[rir_fcrec_n++] = c;
}

int rir_hook_fconst_reuse(void) {
	if (!rir_c2_active)
		return -1;
	while (rir_fcrec_i + 1 < rir_fcrec_n && rir_fcrec_pos[rir_fcrec_i + 1] <= ind &&
	       ((rir_fcrec_nc[rir_fcrec_i] & RIR_NOEVAL_MASK) ||
	        rir_fcrec_pos[rir_fcrec_i + 1] > rir_fcrec_pos[rir_fcrec_i]))
		rir_fcrec_i++;
	if (rir_fcrec_i >= rir_fcrec_n)
		return 0;
	return rir_fcrec[rir_fcrec_i++];
}

static int rir_slotrec[RIR_LOCREC_MAX];
static int rir_slotrec_pos[RIR_LOCREC_MAX];
static int rir_slotrec_nc[RIR_LOCREC_MAX];
static int rir_slotrec_n, rir_slotrec_i;

void rir_slot_record(int loc_in) {
	if (rir_slotrec_n >= RIR_LOCREC_MAX)
		return;
	rir_slotrec_pos[rir_slotrec_n] = ind;
	rir_slotrec_nc[rir_slotrec_n] = nocode_wanted;
	rir_slotrec[rir_slotrec_n++] = loc_in;
}

int rir_slot_replay(int *loc_out) {
	while (rir_slotrec_i + 1 < rir_slotrec_n &&
				 rir_slotrec_pos[rir_slotrec_i + 1] <= ind &&
				 ((rir_slotrec_nc[rir_slotrec_i] & RIR_NOEVAL_MASK) ||
					rir_slotrec_pos[rir_slotrec_i + 1] > rir_slotrec_pos[rir_slotrec_i]))
		rir_slotrec_i++;
	if (rir_slotrec_i >= rir_slotrec_n)
		return 0;
	*loc_out = rir_slotrec[rir_slotrec_i++];
	return 1;
}

static int rir_tvrec[RIR_LOCREC_MAX], rir_tvrec_r2[RIR_LOCREC_MAX];
static int rir_tvrec_pos[RIR_LOCREC_MAX];
static int rir_tvrec_n, rir_tvrec_i;

void rir_tvar_record(int loc_in, int r2) {
	if (rir_tvrec_n >= RIR_LOCREC_MAX)
		return;
	rir_tvrec_pos[rir_tvrec_n] = ind;
	rir_tvrec_r2[rir_tvrec_n] = r2;
	rir_tvrec[rir_tvrec_n++] = loc_in;
}

int rir_tvar_replay(int *loc_out, int *r2_out) {
	while (rir_tvrec_i + 1 < rir_tvrec_n && rir_tvrec_pos[rir_tvrec_i + 1] <= ind)
		rir_tvrec_i++;
	if (rir_tvrec_i >= rir_tvrec_n)
		return 0;
	*r2_out = rir_tvrec_r2[rir_tvrec_i];
	*loc_out = rir_tvrec[rir_tvrec_i++];
	return 1;
}
int rir_c2_active;
int rir_started;
int rir_body_loc_sv;

static const char *rir_out;
static RirOp *rir_ops;
static int rir_n, rir_cap;
static RirMark *rir_marks;
static int rir_markn, rir_markcap;
static SValue *rir_mvs;
static int rir_mvsn, rir_mvscap;
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
static unsigned char *rir_vscapt;
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
			"body",	 "incr", "synth", "inc", "member", "tarm", "lsup",
			"lopnd", "vstore", "vla", "cplx", "cvt", "acas", "cplxb"};
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

static void rir_mark_v2(int tag, int kind, int val, long long a, long long b);

static void rir_mark_v(int tag, int kind, int val) {
	rir_mark_v2(tag, kind, val, 0, 0);
}

static void rir_mark_v2(int tag, int kind, int val, long long a, long long b) {
	RirMark *m;
	if (rir_markn >= rir_markcap) {
		rir_markcap = rir_markcap ? rir_markcap * 2 : 128;
		rir_marks = mcc_realloc(rir_marks, (size_t)rir_markcap * sizeof *rir_marks);
	}
	m = &rir_marks[rir_markn++];
	m->tag = tag;
	m->kind = kind;
	m->val = val;
	m->nocode = nocode_wanted;
	m->ind = ind;
	m->inop = jrn_depth;
	m->v1 = a;
	m->v2 = b;
	m->at = jrn_n;
	{
		int n = (int)(vtop - vstack + 1);
		if (n < 0)
			n = 0;
		if (n > VSTACK_SIZE)
			n = VSTACK_SIZE;
		if (rir_mvsn + n > rir_mvscap) {
			int ncap = rir_mvscap ? rir_mvscap * 2 : 1024;
			while (ncap < rir_mvsn + n)
				ncap *= 2;
			rir_mvs = mcc_realloc(rir_mvs, (size_t)ncap * sizeof *rir_mvs);
			rir_mvscap = ncap;
		}
		if (n)
			memcpy(rir_mvs + rir_mvsn, vstack, (size_t)n * sizeof(SValue));
		m->vs_off = rir_mvsn;
		m->vs_n = n;
		rir_mvsn += n;
	}
	rir_tot_regions++;
	if (tag != RIR_T_MARK && kind >= 0 && kind < RIR_R_COUNT)
		rir_reghist[kind]++;
}

static void rir_mark(int tag, int kind) { rir_mark_v(tag, kind, 0); }

void rir_rbegin_val(int kind, int val) {
	if (!rir_active)
		return;
	if (rir_stackn >= (int)(sizeof rir_stack / sizeof rir_stack[0])) {
		rir_ovf = 1;
		return;
	}
	rir_stack[rir_stackn++] = kind;
	rir_mark_v(RIR_T_RBEGIN, kind, val);
}

void rir_rbegin(int kind) { rir_rbegin_val(kind, 0); }

void rir_rend_to_val(int kind, int val) {
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
		rir_mark_v(RIR_T_REND, k, k == kind ? val : 0);
		if (k == kind)
			return;
	}
}

void rir_rend_to(int kind) { rir_rend_to_val(kind, 0); }

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
	if (!rir_stackn)
		return;
	if (rir_stack[rir_stackn - 1] == RIR_R_IF)
		rir_rbegin(RIR_R_THEN);
	else if (rir_stack[rir_stackn - 1] == RIR_R_WHILE)
		rir_rbegin(RIR_R_BODY);
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

void rir_mark_val2(int kind, long long a, long long b) {
	if (!rir_active)
		return;
	rir_mark_v2(RIR_T_MARK, kind, 0, a, b);
}

void rir_mark_vla(int t, uint64_t ref, int addr, int new_save, int locorig,
									int align, int result) {
	RirMark *m;
	if (!rir_active)
		return;
	rir_mark_v2(RIR_T_MARK, RIR_M_VLA, (new_save ? 1 : 0) | (align << 1),
							(long long)(unsigned)t | (((long long)(unsigned)result) << 32),
							(long long)ref);
	m = &rir_marks[rir_markn - 1];
	m->v3 = ((long long)(unsigned)addr) |
					(((long long)(unsigned)locorig) << 32);
}

void rir_vla_begin(void) {
	if (!rir_active)
		return;
	rir_rbegin(RIR_R_VLA);
}

void rir_hook_if_begin(void) {
	rir_rbegin(RIR_R_IF);
	rir_rbegin(RIR_R_COND);
}

void rir_hook_if_gvtst_done(void) { rir_rcond_done(); }

void rir_hook_if_else(void) {
	rir_rend_to(RIR_R_THEN);
	rir_rbegin(RIR_R_ELSE);
}

void rir_hook_if_end(void) { rir_rend_to(RIR_R_IF); }

void rir_hook_while_cond_start(void) { rir_mark_pt(RIR_M_WHILECOND); }

void rir_hook_while_begin(void) {
	rir_rbegin(RIR_R_WHILE);
	rir_rbegin(RIR_R_COND);
}

void rir_hook_while_end(void) { rir_rend_to(RIR_R_WHILE); }

void rir_hook_do_begin(void) {
	rir_rbegin(RIR_R_DO);
	rir_rbegin(RIR_R_BODY);
}

void rir_hook_do_body_end(void) { rir_rend_to(RIR_R_BODY); }

void rir_hook_do_cond(void) { rir_rbegin(RIR_R_COND); }

void rir_hook_do_end(void) { rir_rend_to(RIR_R_DO); }

void rir_hook_for_begin(void) { rir_rbegin(RIR_R_FOR); }

void rir_hook_for_cond(void) { rir_rbegin(RIR_R_COND); }

void rir_hook_for_incr_begin(void) { rir_rbegin_val(RIR_R_INCR, 1); }

void rir_hook_for_incr_end(void) { rir_rend_to(RIR_R_INCR); }

void rir_hook_for_no_incr(void) {
	rir_rbegin(RIR_R_INCR);
	rir_rend_to(RIR_R_INCR);
}

void rir_hook_for_body_begin(void) { rir_rbegin(RIR_R_BODY); }

void rir_hook_for_end(void) { rir_rend_to(RIR_R_FOR); }

void rir_hook_switch_begin(void) {
	rir_rbegin(RIR_R_SWITCH);
	rir_rbegin(RIR_R_BODY);
}

void rir_hook_switch_end(void) { rir_rend_to(RIR_R_SWITCH); }

void rir_hook_case(long long v1, long long v2) {
	rir_mark_val2(RIR_M_CASE, v1, v2);
}

void rir_hook_default(void) { rir_mark_pt(RIR_M_DEFAULT); }

void rir_hook_label(int v) { rir_mark_val(RIR_M_LABEL, v); }

void rir_hook_goto(int v) { rir_mark_val(RIR_M_GOTO, v); }

/* nc_pre is nocode_wanted as it stood BEFORE the gjmp, which is the only way
   to tell a live break from a dead one: gjmp_acs sets CODE_OFF_BIT on the way
   out, so by the time this hook runs every break looks unreachable. A break in
   statically dead code emits no jump, and an arena Jump for it would make
   replay write one the parser never did. */
void rir_hook_break_continue(int is_continue, int nc_pre) {
	if (nc_pre)
		return;
	rir_mark_val(RIR_M_JUMP, is_continue);
}

void rir_hook_call_begin(void) { rir_rbegin(RIR_R_CALL); }

void rir_hook_call_end(void) { rir_rend_to(RIR_R_CALL); }

int rir_cast_seq;

void rir_hook_convert(void) { rir_cast_seq++; }

void rir_hook_call_argcast(int pre_seq) {
	rir_mark_val(RIR_M_ARGCAST, rir_cast_seq != pre_seq);
}

void rir_hook_call_noreturn(void) { rir_mark_pt(RIR_M_NORETURN); }

void rir_hook_call_effect_end(void) { rir_rend_to_val(RIR_R_CALL, 1); }

void rir_hook_vstore(void) { rir_rbegin(RIR_R_VSTORE); }

void rir_hook_vstore_end(void) { rir_rend_to(RIR_R_VSTORE); }

void rir_hook_ret_expr_done(void) { rir_mark_val(RIR_M_RETEXPR, 0); }

void rir_hook_return(int has_val) { rir_mark_val(RIR_M_RETURN, has_val); }

void rir_hook_return_jmp(int jumps) { rir_mark_val(RIR_M_RETJMP, jumps); }

void rir_hook_implicit_return(void) { rir_mark_pt(RIR_M_IRETURN); }

void rir_hook_synth_begin(void) { rir_rbegin(RIR_R_SYNTH); }

void rir_hook_synth_end(void) { rir_rend_to(RIR_R_SYNTH); }

void rir_hook_castlower_begin(struct CType *type) {
	rir_rbegin_val(RIR_R_CVT, type->t);
}

void rir_hook_castlower_end(void) { rir_rend_to(RIR_R_CVT); }

static int rir_member_arrow;
static int rir_bcplx_low;

void rir_hook_member_begin(int is_arrow) {
	rir_rbegin(RIR_R_MEMBER);
	rir_member_arrow = is_arrow;
}

void rir_hook_member_end(int cumofs, int nonlval) {
	rir_rend_to_val(RIR_R_MEMBER, ((int)((unsigned)cumofs << 2)) |
																		(rir_member_arrow ? 2 : 0) |
																		(nonlval ? 1 : 0));
}

void rir_hook_builtin_complex_lower(void) {
	rir_bcplx_low = 1;
	rir_rbegin(RIR_R_CPLXB);
}

void rir_hook_builtin_complex_end(void) {
	if (rir_bcplx_low) {
		rir_bcplx_low = 0;
		rir_rend_to(RIR_R_CPLXB);
	}
}

static unsigned char rir_tern_on[16];
static unsigned char rir_tern_live1[16];
static int rir_tern_n;
static unsigned char rir_lor_on[16];
static unsigned char rir_lor_late[16];
static int rir_lor_n;

void rir_hook_body_begin(void) {
	rir_body_loc_sv = loc;
	rir_started = 0;
	rir_tern_n = 0;
	rir_lor_n = 0;
}

int rir_dbg_on(void) {
	const char *e = getenv("RIRDBG");
	return e && funcname && !strcmp(e, funcname);
}

int rir_capture_live(void) {
	return rir_active && !ast_replaying && !jrn_replaying;
}

int rir_hook_slot_replay(void) {
	int rl;
	if (rir_c2_active && rir_slot_replay(&rl)) {
		loc = rl;
		return 1;
	}
	return 0;
}

void rir_hook_slot_record(void) {
	if (rir_capture_live())
		rir_slot_record(loc);
}

void rir_hook_ternary_begin(int c, int g) {
	if (rir_tern_n < 16) {
		rir_tern_on[rir_tern_n] = (unsigned char)(c < 0 ? (g ? 2 : 1) : 0);
		rir_tern_live1[rir_tern_n] = (unsigned char)(c == 1 && !g);
		if (rir_tern_on[rir_tern_n]) {
			rir_rbegin_val(RIR_R_TERNARY, rir_tern_on[rir_tern_n] == 2);
			rir_rbegin(RIR_R_COND);
		}
		rir_tern_n++;
	}
}

void rir_hook_ternary_branch(int which) {
	if (rir_tern_n && rir_tern_on[rir_tern_n - 1] &&
			(rir_tern_on[rir_tern_n - 1] == 1 || which == 1)) {
		rir_rend_to(RIR_R_COND);
		rir_rbegin(RIR_R_TARM);
	}
}

void rir_hook_ternary_branch_done(int which) {
	if (rir_tern_n && rir_tern_on[rir_tern_n - 1] &&
			(rir_tern_on[rir_tern_n - 1] == 1 || which == 1)) {
		rir_rend_to_val(RIR_R_TARM, which);
		rir_rbegin(RIR_R_COND);
	}
	if (rir_tern_n && rir_tern_live1[rir_tern_n - 1] && which == 0)
		rir_mark_pt(RIR_M_TERNHOLD);
}

void rir_hook_ternary_pick(void) {
	if (rir_tern_n && rir_tern_live1[rir_tern_n - 1])
		rir_mark_pt(RIR_M_TERNPICK);
}

void rir_hook_ternary_end(void) {
	if (rir_tern_n) {
		rir_tern_n--;
		if (rir_tern_on[rir_tern_n])
			rir_rend_to(RIR_R_TERNARY);
	}
}

void rir_hook_landor_operand(int op, int c, int first) {
	if (first) {
		if (rir_lor_n < 16) {
			rir_lor_on[rir_lor_n] = (unsigned char)(c < 0 ? 1
					: (c == (op == TOK_LAND) && tok == op) ? 2 : 0);
			rir_lor_late[rir_lor_n] = 0;
			if (rir_lor_on[rir_lor_n] == 1)
				rir_rbegin_val(RIR_R_LANDOR, op);
			rir_lor_n++;
		}
	} else if (rir_lor_n && rir_lor_on[rir_lor_n - 1] == 2 && c < 0 &&
			tok == op) {
		rir_lor_on[rir_lor_n - 1] = 1;
		rir_lor_late[rir_lor_n - 1] = 1;
		rir_rbegin_val(RIR_R_LANDOR, op);
	}
	if (rir_lor_n && rir_lor_on[rir_lor_n - 1] == 1) {
		rir_rbegin(RIR_R_LOPND);
		rir_rend_to_val(RIR_R_LOPND, 0);
		rir_rbegin(RIR_R_LSUP);
	}
}

void rir_hook_landor_next(void) {
	if (rir_lor_n && rir_lor_on[rir_lor_n - 1] == 1)
		rir_rend_to(RIR_R_LSUP);
}

void rir_hook_landor_end(int materialized) {
	if (rir_lor_n) {
		rir_lor_n--;
		if (rir_lor_on[rir_lor_n] == 1)
			rir_rend_to_val(RIR_R_LANDOR,
					(materialized ? 1 : 0) | (rir_lor_late[rir_lor_n] ? 2 : 0) |
							((materialized & 1) ? 4 : 0));
	}
}

void rir_hook_cplx_begin(void) { rir_rbegin(RIR_R_CPLX); }

void rir_hook_cplx_end(void) { rir_rend_to(RIR_R_CPLX); }

void rir_hook_acas_begin(int val) { rir_rbegin_val(RIR_R_ACAS, val); }

void rir_hook_acas_end(int val) { rir_rend_to_val(RIR_R_ACAS, val); }

void rir_hook_vla_alloc_begin(void) { rir_vla_begin(); }

void rir_hook_vla_alloc_end(struct CType *type, int addr, int new_save,
														int locorig, int align, int result) {
	rir_rend_to(RIR_R_VLA);
	rir_mark_vla(type->t, (uint64_t)(uintptr_t)type->ref, addr, new_save,
							 locorig, align, result);
}

void rir_hook_vla_restore(int loc) { rir_mark_val(RIR_M_VLARESTORE, loc); }

void rir_hook_store_addr_late(void) { rir_mark_pt(RIR_M_ADDRLATE); }

void rir_hook_asm_operands(int nb_operands, uint64_t gvmask) {
	rir_mark_val2(RIR_M_ASMOPS, nb_operands, (long long)gvmask);
}

void rir_hook_inc(int post, int c) {
	rir_rbegin_val(RIR_R_INC, (c << 1) | (post ? 1 : 0));
}

void rir_hook_inc_end(void) { rir_rend_to(RIR_R_INC); }

void rir_hook_vdup(void) { rir_mark_pt(RIR_M_OPASSIGN); }

void rir_hook_indir(void) { rir_mark_pt(RIR_M_LOAD); }

void rir_hook_bfgv(int tt) { rir_mark_val(RIR_M_BFGV, tt); }

void rir_hook_cmp_invert(void) { rir_mark_pt(RIR_M_CMPINV); }

void rir_hook_cast_gv(void) { rir_mark_pt(RIR_M_CASTGV); }

void rir_hook_cleanup_goto(void *pcl) {
	rir_mark_val2(RIR_M_CLGOTO, (long long)(uintptr_t)pcl, 0);
}

void rir_hook_cleanup_thunk(void *pcl, int v, int end) {
	rir_mark_val2(end ? RIR_M_CLJMP : RIR_M_CLTHUNK, (long long)(uintptr_t)pcl,
								(long long)v);
}

#define RIR_XT_MAX 16384
static Sym rir_xt[RIR_XT_MAX];
static Sym *rir_xt_src[RIR_XT_MAX];
static int rir_xt_c[RIR_XT_MAX];
static int rir_xt_t[RIR_XT_MAX];
static int rir_xt_v[RIR_XT_MAX];
static void *rir_xt_nx[RIR_XT_MAX];
static void *rir_xt_tr[RIR_XT_MAX];
static int rir_xtn;

#define RIR_PT_MAX 4096
static Sym rir_pt[RIR_PT_MAX];
static int rir_ptn;

static Sym *rir_ptr_sym(const CType *t) {
	Sym *s;
	int k;
	for (k = 0; k < rir_ptn; k++)
		if (rir_pt[k].type.t == t->t && rir_pt[k].type.ref == t->ref)
			return &rir_pt[k];
	if (rir_ptn >= RIR_PT_MAX)
		return NULL;
	s = &rir_pt[rir_ptn++];
	memset(s, 0, sizeof *s);
	s->v = SYM_FIELD;
	s->c = -1;
	s->type = *t;
	return s;
}

static int rir_body_hasheq;

void rir_reset(void) {
	rir_locrec_n = 0;
	rir_locrec_i = 0;
	rir_slotrec_n = 0;
	rir_slotrec_i = 0;
	rir_tvrec_n = 0;
	rir_tvrec_i = 0;
	rir_fcrec_n = 0;
	rir_fcrec_i = 0;
	rir_body_hasheq = 0;
	rir_xtn = 0;
	rir_ptn = 0;
	rir_n = 0;
	rir_markn = 0;
	rir_mvsn = 0;
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

static int rir_chain_adopt(int addr, int opi, unsigned char *fl,
													 unsigned char bit) {
	int L;
	if (!addr)
		return -1;
	L = rir_cmap_find(addr);
	if (L >= 0 || opi <= 0)
		return L;
	L = rir_nlbl++;
	rir_cmap_bind(addr, L);
	*fl |= bit;
	return L;
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
		rir_vscapt = mcc_realloc(rir_vscapt, (size_t)rir_vscap * sizeof *rir_vscapt);
	}
	rir_cmapn = 0;
	rir_nlbl = 0;
	rir_fallback = 0;
	rir_jmpsv_fb = 0;
	for (i = 0; i < jrn_vsn; i++) {
		rir_vslbl[i] = rir_vslbl2[i] = -1;
		rir_vscapt[i] = 0;
	}
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
			unsigned char *fl = &rir_vscapt[o->vs_off + k];
			if (vv == VT_JMP || vv == VT_JMPI) {
				rir_tot_jmpsv++;
				rir_vslbl[o->vs_off + k] = rir_chain_adopt((int)v->c.i, i, fl, 1);
				if (v->c.i && rir_vslbl[o->vs_off + k] < 0) {
					rir_jmpsv_fb++;
					rir_tot_jmpsv_fb++;
				}
			} else if (v->r == VT_CMP) {
				rir_tot_jmpsv++;
				rir_vslbl[o->vs_off + k] = rir_chain_adopt(v->jtrue, i, fl, 1);
				rir_vslbl2[o->vs_off + k] = rir_chain_adopt(v->jfalse, i, fl, 2);
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
			o->rnocode = rir_marks[m].nocode;
			o->rind = rir_marks[m].ind;
			o->rinop = rir_marks[m].inop;
			o->rv3 = rir_marks[m].v3;
			o->rv1 = rir_marks[m].v1;
			o->rv2 = rir_marks[m].v2;
			o->mvs_off = rir_marks[m].vs_off;
			o->mvs_n = rir_marks[m].vs_n;
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
static CType rir_pvt[VSTACK_SIZE + 1];
static int rir_pvr[VSTACK_SIZE + 1];
static CValue rir_pvc[VSTACK_SIZE + 1];
static unsigned char rir_pvok[VSTACK_SIZE + 1];
static AstLocal rir_bb[64];
static int rir_bbn;
static AstLocal rir_cf[64];
static int rir_cfkind[64];
static int rir_cfcond[64];
static AstLocal rir_cfpfx[64];
static int rir_cfind[64];
static AstLocal rir_while_pfx = AST_NONE;
static int rir_cfn;
static int rir_arena_mismatch;
static int rir_cplx_depth;
static int rir_cplxb_depth;
static int rir_cplxb_on;
static int rir_acas_depth;
static int rir_vsup_depth;
static int rir_vsup_nest;
static int rir_acas_val;
static int rir_after_ret;
static long rir_tot_arena_fn, rir_tot_arena_nodes, rir_tot_arena_hash_eq;
static long rir_tot_arena_cmp, rir_tot_arena_count_eq;
static long rir_tot_tree_nodes, rir_tot_arena_cmp_nodes;
static long rir_tot_c2_skip;
static long rir_tot_leaf, rir_tot_refill;
static long rir_tot_c2_try, rir_tot_c2_ok, rir_tot_c2_bytes, rir_tot_c2_len,
		rir_tot_c2_err;
static char rir_c2_msg[256];
static long rir_tot_c2_invalid;
static long rir_tot_c3_try, rir_tot_c3_ran, rir_tot_c3_folds, rir_tot_c3_broke;
static long rir_tot_c3_pair, rir_tot_c3_same_folds, rir_tot_c3_same_hash;
static long rir_tot_c3_pair_fired;

static void rir_c2_sink(void *opaque, const char *msg) {
	(void)opaque;
	snprintf(rir_c2_msg, sizeof rir_c2_msg, "%s", msg ? msg : "?");
}
static long rir_kindhist[AST_KIND_COUNT], rir_treekindhist[AST_KIND_COUNT];

static int rir_xt_chain(int t) {
	return (t & VT_BTYPE) == VT_STRUCT || (t & VT_BTYPE) == VT_FUNC;
}

static Sym *rir_xtype_ref(Sym *s, int depth, int chain) {
	int k;
	Sym *c;
	if (!s || depth > 64 || (s->type.t & VT_VLA))
		return s;
	for (k = 0; k < rir_xtn; k++)
		if (rir_xt_src[k] == s && rir_xt_c[k] == s->c && rir_xt_t[k] == s->type.t &&
				rir_xt_v[k] == s->v && rir_xt_nx[k] == (void *)s->next &&
				rir_xt_tr[k] == (void *)s->type.ref)
			return &rir_xt[k];
	if (rir_xtn >= RIR_XT_MAX)
		return s;
	k = rir_xtn++;
	rir_xt_src[k] = s;
	rir_xt_c[k] = s->c;
	rir_xt_t[k] = s->type.t;
	rir_xt_v[k] = s->v;
	rir_xt_nx[k] = (void *)s->next;
	rir_xt_tr[k] = (void *)s->type.ref;
	c = &rir_xt[k];
	*c = *s;
	c->type.ref = rir_xtype_ref(s->type.ref, depth + 1, rir_xt_chain(s->type.t));
	if (chain)
		c->next = rir_xtype_ref(s->next, depth + 1, chain);
	return c;
}

void rir_snap_types(SValue *sv, int n) {
	int i;
	if (!rir_env)
		return;
	for (i = 0; i < n; i++)
		if ((sv[i].type.t & VT_BTYPE) == VT_STRUCT)
			sv[i].type.ref = rir_xtype_ref(sv[i].type.ref, 0, 1);
}

static int rir_tcore(int t) {
	return t & ~(unsigned)(VT_DEFSIGN | VT_LONG | VT_STORAGE);
}

static int rir_same_width(const CType *a, const CType *b) {
	int al, bl;
	CType x = *a, y = *b;
	return type_size(&x, &al) == type_size(&y, &bl);
}

static int rir_prov_ok(int slot, const SValue *sv) {
	int pt, st;
	if (slot < 0 || slot > VSTACK_SIZE || !rir_pvok[slot])
		return 0;
	if (rir_pvr[slot] != sv->r || rir_pvc[slot].i != sv->c.i)
		return 0;
	pt = rir_pvt[slot].t;
	st = sv->type.t;
	if (rir_tcore(pt) == rir_tcore(st))
		return 0;
	if ((pt & (VT_BTYPE | VT_ARRAY | VT_VLA | VT_BITFIELD)) != (pt & VT_BTYPE) ||
			(st & (VT_BTYPE | VT_ARRAY | VT_VLA | VT_BITFIELD)) != (st & VT_BTYPE))
		return 0;
	if ((pt & VT_BTYPE) == VT_STRUCT || (pt & VT_BTYPE) == VT_FUNC ||
			(pt & VT_BTYPE) == VT_VOID || (st & VT_BTYPE) == VT_STRUCT ||
			(st & VT_BTYPE) == VT_FUNC || (st & VT_BTYPE) == VT_VOID)
		return 0;
	if (is_float(pt) != is_float(st))
		return 0;
	return 1;
}

static AstLocal rir_leaf_slot(const SValue *sv, int slot) {
	int is_const = (sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	AstLocal n = ast_node(rir_arena, is_const ? AST_Literal : AST_Ref);
	int prov = rir_prov_ok(slot, sv);
	ast_set_op(rir_arena, n, sv->r);
	if (sv->sym && (sv->r & (VT_VALMASK | VT_SYM)) == (VT_CONST | VT_SYM) &&
		(sv->sym->type.t & VT_ARRAY) &&
		(sv->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR)
		ast_set_type(rir_arena, n, sv->type.t | VT_ARRAY,
			(uint64_t)(uintptr_t)sv->sym->type.ref);
	else
		ast_set_type(rir_arena, n, sv->type.t,
			(uint64_t)(uintptr_t)sv->type.ref);
	ast_set_ival(rir_arena, n, (uint64_t)sv->c.i);
	ast_set_wide(rir_arena, n, ast_sv_hi(sv),
							 sv->r2 >= VT_CONST ? (unsigned)VT_CONST : (unsigned)sv->r2);
	ast_set_sym(rir_arena, n, (uint64_t)(uintptr_t)sv->sym);
	if (sv->sym && (sv->r & VT_LVAL) && !(sv->sym->type.t & (VT_ARRAY | VT_VLA)) &&
			(sv->sym->type.t & VT_BTYPE) != VT_STRUCT &&
			(sv->sym->type.t & VT_BTYPE) != VT_FUNC &&
			(sv->type.t & VT_BTYPE) != VT_STRUCT &&
			(sv->type.t & VT_BTYPE) != VT_FUNC && !(sv->type.t & VT_ARRAY) &&
			is_float(sv->sym->type.t) == is_float(sv->type.t) &&
			(sv->sym->type.t != sv->type.t ||
			 sv->sym->type.ref != sv->type.ref) &&
			rir_same_width(&sv->sym->type, &sv->type)) {
		AstLocal cv;
		ast_set_type(rir_arena, n, sv->sym->type.t,
								 (uint64_t)(uintptr_t)sv->sym->type.ref);
		cv = ast_node(rir_arena, AST_Convert);
		ast_set_type(rir_arena, cv, sv->type.t, (uint64_t)(uintptr_t)sv->type.ref);
		ast_add_child(rir_arena, cv, n);
		if (prov)
			rir_pvok[slot] = 0;
		return cv;
	}
	if (prov) {
		AstLocal cv;
		ast_set_type(rir_arena, n, rir_pvt[slot].t,
								 (uint64_t)(uintptr_t)rir_pvt[slot].ref);
		cv = ast_node(rir_arena, AST_Convert);
		ast_set_type(rir_arena, cv, sv->type.t, (uint64_t)(uintptr_t)sv->type.ref);
		ast_add_child(rir_arena, cv, n);
		rir_pvok[slot] = 0;
		return cv;
	}
	return n;
}

static AstLocal rir_leaf(const SValue *sv) { return rir_leaf_slot(sv, -1); }

#ifdef MCC_JRN_HAVE_X86_PRIMS
static int rir_has_atomic(AstLocal n, int depth) {
	int i, nc, op;
	if (n == AST_NONE || depth > 8)
		return 0;
	op = ast_op(rir_arena, n);
	if (ast_kind(rir_arena, n) == AST_Binary &&
			(op == AST_OP_AXADD || op == AST_OP_AXCHG || op == AST_OP_ACMPXCHG))
		return 1;
	nc = (int)ast_nchild(rir_arena, n);
	for (i = 0; i < nc; i++)
		if (rir_has_atomic(ast_child(rir_arena, n, i), depth + 1))
			return 1;
	return 0;
}
#endif

static int rir_effectful(AstLocal n) {
	uint16_t k;
	if (n == AST_NONE)
		return 0;
	k = ast_kind(rir_arena, n);
	if (k == AST_Store || k == AST_Invoke)
		return 1;
	if (k == AST_If && ast_op(rir_arena, n) == 5 && ast_nchild(rir_arena, n) == 3)
		return 1;
	if (k == AST_Binary && (ast_fbits(rir_arena, n) & AST_FB_LANDOR_MATERIAL))
		return 1;
#ifdef MCC_JRN_HAVE_X86_PRIMS
	if (rir_has_atomic(n, 0))
		return 1;
#endif
	if (k == AST_Binary && ast_op(rir_arena, n) == AST_OP_ACASRMW)
		return 1;
	if (k == AST_Convert && ast_nchild(rir_arena, n) == 1 &&
			ast_kind(rir_arena, ast_child(rir_arena, n, 0)) == AST_Invoke)
		return 1;
	return k == AST_Unary && ast_op(rir_arena, n) < AST_OP_ADDR;
}

static int rir_c3_pipeline(AstArena *a) {
	int n = 0;
	n += ast_sccp_run(a);
	n += ast_dse_run(a);
	n += ast_jt_run(a);
	n += ast_sethi_run(a);
	return n;
}

static AstLocal rir_pending_ret = AST_NONE;

#define RIR_CLG_MAX 32
static void *rir_clg_key[RIR_CLG_MAX];
static AstLocal rir_clg_node[RIR_CLG_MAX];
static int rir_clg_n;
static void *rir_clg_pending;
static int rir_clg_syn;

static void rir_clg_bind(void *k, AstLocal n) {
	int i;
	for (i = 0; i < rir_clg_n; i++)
		if (rir_clg_key[i] == k) {
			rir_clg_node[i] = n;
			return;
		}
	if (rir_clg_n >= RIR_CLG_MAX)
		return;
	rir_clg_key[rir_clg_n] = k;
	rir_clg_node[rir_clg_n++] = n;
}

static AstLocal rir_clg_get(void *k) {
	int i;
	for (i = 0; i < rir_clg_n; i++)
		if (rir_clg_key[i] == k)
			return rir_clg_node[i];
	return AST_NONE;
}

#define RIR_IHOLD_MAX 32
static AstLocal rir_ihold[RIR_IHOLD_MAX];
static short rir_iholdd[RIR_IHOLD_MAX];
static int rir_iholdn;
static void rir_stmt(AstLocal n);

static int rir_ihold_off;

static void rir_ihold_flush(void) {
	int q, k = rir_iholdn;
	rir_iholdn = 0;
	rir_ihold_off++;
	for (q = 0; q < k; q++)
		rir_stmt(rir_ihold[q]);
	rir_ihold_off--;
}

static int rir_ihold_arm;
static int rir_lorn;

static int rir_callee_pending(void) {
	int k;
	for (k = 0; k < rir_shn; k++)
		if (rir_sh[k] != AST_NONE &&
				(ast_type_t(rir_arena, rir_sh[k]) & VT_BTYPE) == VT_FUNC)
			return 1;
	return 0;
}

static int rir_hold_inline(AstLocal n) {
	uint16_t k;
	if (n == AST_NONE || rir_shn <= 0 || !rir_bbn || rir_ihold_off ||
			rir_iholdn >= RIR_IHOLD_MAX || rir_pending_ret != AST_NONE || rir_lorn)
		return 0;
	if (!rir_iholdn && !rir_callee_pending())
		return 0;
	k = ast_kind(rir_arena, n);
	if (k == AST_Invoke) {
		if (!rir_ihold_arm)
			return 0;
	} else if (k == AST_Store) {
		if (!rir_iholdn || rir_shn < rir_iholdd[rir_iholdn - 1])
			return 0;
	} else {
		return 0;
	}
	rir_iholdd[rir_iholdn] = (short)rir_shn;
	rir_ihold[rir_iholdn++] = n;
	return 1;
}

static AstLocal rir_ihold_bind(AstLocal n) {
	AstLocal bb;
	int q, i, r;
	if (!rir_iholdn || n == AST_NONE)
		return n;
	r = rir_shn + 1;
	for (i = rir_iholdn; i > 0 && rir_iholdd[i - 1] >= r; i--)
		;
	if (i == rir_iholdn)
		return n;
	if (i == rir_iholdn - 1 &&
			ast_kind(rir_arena, rir_ihold[i]) == AST_Invoke) {
		rir_iholdn = i;
		rir_ihold_off++;
		rir_stmt(rir_ihold[i]);
		rir_ihold_off--;
		return n;
	}
	bb = ast_node(rir_arena, AST_BasicBlock);
	for (q = i; q < rir_iholdn; q++)
		ast_add_child(rir_arena, bb, rir_ihold[q]);
	ast_add_child(rir_arena, bb, n);
	rir_iholdn = i;
	return bb;
}

#if RIR_DBG_OPTRACE
static int rir_dbg_ent;
#endif
static void rir_stmt(AstLocal n) {
	if (n == AST_NONE || !rir_bbn)
		return;
#if RIR_DBG_OPTRACE
	{
		const char *e = getenv("RIRDBG");
		if (e && funcname && !strcmp(e, funcname))
			fprintf(stderr, "[stmt] ent=%d node=%d kind=%s nc=%d\n", rir_dbg_ent, (int)n,
							ast_kind_name(ast_kind(rir_arena, n)), ast_nchild(rir_arena, n));
	}
#endif
	if (rir_hold_inline(n))
		return;
	if (rir_iholdn && rir_shn <= 0)
		rir_ihold_flush();
	if (rir_pending_ret != AST_NONE && n != rir_pending_ret &&
			ast_kind(rir_arena, n) == AST_Jump) {
		AstLocal held = rir_pending_ret;
		rir_pending_ret = AST_NONE;
		rir_stmt(held);
	}
	if (ast_kind(rir_arena, n) == AST_If && ast_op(rir_arena, n) == 5)
		ast_set_op(rir_arena, n, 7);
	ast_add_child(rir_arena, rir_bb[rir_bbn - 1], n);
}

static int rir_lorn;
#define RIR_LHELD_MAX 8
static AstLocal rir_lheld[RIR_LHELD_MAX];
static int rir_lheldn;

static int rir_synth_depth;
static AstLocal rir_fcs_node = AST_NONE;

#define RIR_DHELD_MAX 8
static int rir_docond;
static AstLocal rir_dheld[RIR_DHELD_MAX];
static int rir_dheldn;

static void rir_drop(AstLocal d) {
	if (!rir_effectful(d))
		return;
	if (rir_lorn && rir_lheldn < RIR_LHELD_MAX &&
			((ast_kind(rir_arena, d) == AST_Store && ast_nchild(rir_arena, d) == 2 &&
				ast_fbits(rir_arena, d) == 0 && ast_op(rir_arena, d) == 0) ||
			 ast_kind(rir_arena, d) == AST_Invoke)) {
		rir_lheld[rir_lheldn++] = d;
		return;
	}
	if (rir_docond && rir_dheldn < RIR_DHELD_MAX &&
			ast_kind(rir_arena, d) == AST_Store && ast_nchild(rir_arena, d) == 2 &&
			ast_fbits(rir_arena, d) == 0 && ast_op(rir_arena, d) == 0) {
		rir_dheld[rir_dheldn++] = d;
		return;
	}
	rir_stmt(d);
}

static void rir_dheld_flush(void) {
	int q;
	for (q = 0; q < rir_dheldn; q++)
		rir_stmt(rir_dheld[q]);
	rir_dheldn = 0;
}

static int rir_ret_spilled;

static void rir_ret_follow_spill(AstLocal t, AstLocal v) {
	AstLocal rv;
	if (rir_pending_ret == AST_NONE || rir_ret_spilled ||
			ast_nchild(rir_arena, rir_pending_ret) != 1)
		return;
	rv = ast_child(rir_arena, rir_pending_ret, 0);
	while (v != AST_NONE && ast_kind(rir_arena, v) == AST_Convert &&
				 ast_nchild(rir_arena, v) == 1)
		v = ast_child(rir_arena, v, 0);
	if (rv == AST_NONE || t == AST_NONE || v == AST_NONE ||
			ast_nchild(rir_arena, rv) != 0 || ast_nchild(rir_arena, t) != 0 ||
			ast_kind(rir_arena, rv) != ast_kind(rir_arena, v) ||
			ast_op(rir_arena, rv) != ast_op(rir_arena, v) ||
			ast_ival(rir_arena, rv) != ast_ival(rir_arena, v) ||
			ast_sym(rir_arena, rv) != ast_sym(rir_arena, v))
		return;
	ast_set_kind(rir_arena, rv, ast_kind(rir_arena, t));
	ast_set_op(rir_arena, rv, ast_op(rir_arena, t));
	ast_set_type(rir_arena, rv, ast_type_t(rir_arena, t),
							 ast_type_ref(rir_arena, t));
	ast_set_ival(rir_arena, rv, ast_ival(rir_arena, t));
	ast_set_sym(rir_arena, rv, ast_sym(rir_arena, t));
	rir_ret_spilled = 1;
}

static void rir_spill_follow_sh(AstLocal t, AstLocal v) {
	int k, hit = -1;
	if (rir_pending_ret != AST_NONE || t == AST_NONE || v == AST_NONE ||
			ast_nchild(rir_arena, t) != 0 || ast_nchild(rir_arena, v) != 0 ||
			ast_kind(rir_arena, t) != ast_kind(rir_arena, v) ||
			ast_sym(rir_arena, t) != 0 ||
			ast_ival(rir_arena, t) == ast_ival(rir_arena, v))
		return;
	for (k = 0; k < rir_shn; k++) {
		AstLocal s = rir_sh[k];
		if (s == AST_NONE || ast_nchild(rir_arena, s) != 0 ||
				ast_parent(rir_arena, s) != AST_NONE)
			continue;
		if (ast_kind(rir_arena, s) == ast_kind(rir_arena, v) &&
				ast_op(rir_arena, s) == ast_op(rir_arena, v) &&
				ast_ival(rir_arena, s) == ast_ival(rir_arena, v) &&
				ast_sym(rir_arena, s) == ast_sym(rir_arena, v)) {
			if (hit >= 0)
				return;
			hit = k;
		}
	}
	if (hit < 0)
		return;
	ast_set_op(rir_arena, rir_sh[hit], ast_op(rir_arena, t));
	ast_set_type(rir_arena, rir_sh[hit], ast_type_t(rir_arena, t),
							 ast_type_ref(rir_arena, t));
	ast_set_ival(rir_arena, rir_sh[hit], ast_ival(rir_arena, t));
	ast_set_sym(rir_arena, rir_sh[hit], ast_sym(rir_arena, t));
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

static void rir_push_typed(AstLocal n) {
	if (rir_shn > VSTACK_SIZE)
		return;
	n = rir_ihold_bind(n);
	rir_shtype[rir_shn] = 1;
	rir_sh[rir_shn++] = n;
}

static void rir_push_typed_addr(AstLocal n) {
	if (rir_shn > VSTACK_SIZE)
		return;
	rir_shtype[rir_shn] = 3;
	rir_sh[rir_shn++] = n;
}

#define RIR_ARGCAST_MAX 64
static unsigned char rir_argcast_ch[RIR_ARGCAST_MAX];
static int rir_argcast_n;

static AstLocal rir_pending_call = AST_NONE;
static unsigned char rir_vst_seen[16];
static unsigned char rir_vst_ok[16];
static short rir_vst_shn[16];
static unsigned char rir_vst_sup[16];
static long long rir_vst_tc[16], rir_vst_vc[16];
static unsigned char rir_vst_bf[16];
static unsigned char rir_vst_cx[16];
static unsigned char rir_vst_nc[16];
static int rir_cx_depth;
static unsigned char rir_vst_gret[16];
static int rir_gret_depth;
static int rir_vbf_depth;
static int rir_call_depth;
static int rir_vstn;
static AstLocal rir_spill_node = AST_NONE;
static long long rir_spill_addr;

static void rir_flush_pending_call(void) {
	if (rir_pending_call == AST_NONE)
		return;
	rir_stmt(rir_pending_call);
	rir_pending_call = AST_NONE;
}

static int rir_cvt_next;
static int rir_castgv_pend;
static AstLocal rir_castgv_top;
static int rir_castgv_t;
static uint64_t rir_castgv_ref;

static int rir_is_cvt(int kind) {
	switch (kind) {
	case JOP_CVT_ITOF:
	case JOP_CVT_FTOF:
	case JOP_CVT_FTOI:
	case JOP_CVT_SXTW:
	case JOP_CVT_TRUNC32:
	case JOP_CVT_CSTI:
		return 1;
	default:
		return 0;
	}
}

static int rir_is_cmp_binary(AstLocal n) {
	if (n == AST_NONE || ast_kind(rir_arena, n) != AST_Binary)
		return 0;
	switch (ast_op(rir_arena, n)) {
	case TOK_ULT: case TOK_UGE: case TOK_EQ: case TOK_NE:
	case TOK_ULE: case TOK_UGT: case TOK_LT: case TOK_GE:
	case TOK_LE: case TOK_GT:
		return 1;
	default:
		return 0;
	}
}

static int rir_const_subtree(AstLocal n, int depth) {
	int i, nc;
	if (n == AST_NONE || depth > 8)
		return 0;
	switch (ast_kind(rir_arena, n)) {
	case AST_Literal:
		return (ast_op(rir_arena, n) & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	case AST_Binary:
	case AST_Unary:
	case AST_Convert:
		break;
	default:
		return 0;
	}
	nc = (int)ast_nchild(rir_arena, n);
	if (nc == 0)
		return 0;
	for (i = 0; i < nc; i++)
		if (!rir_const_subtree(ast_child(rir_arena, n, i), depth + 1))
			return 0;
	return 1;
}

static int rir_child_has_type(AstLocal n, int st) {
	int i, nc = ast_nchild(rir_arena, n);
	for (i = 0; i < nc; i++) {
		AstLocal c = ast_child(rir_arena, n, i);
		if (c == AST_NONE)
			continue;
		if (rir_tcore(ast_type_t(rir_arena, c)) == rir_tcore(st))
			return 1;
	}
	return 0;
}

static int rir_eff_size(AstLocal n, int depth) {
	int i, nc, best = 0, al, t;
	CType a1;
	if (n == AST_NONE || depth > 8)
		return 0;
	t = ast_type_t(rir_arena, n);
	if (t != 0) {
		if ((t & VT_BTYPE) == VT_STRUCT || (t & VT_BTYPE) == VT_FUNC)
			return 0;
		a1.t = t;
		a1.ref = (Sym *)(uintptr_t)ast_type_ref(rir_arena, n);
		return type_size(&a1, &al);
	}
	if (ast_kind(rir_arena, n) != AST_Binary &&
			ast_kind(rir_arena, n) != AST_Unary)
		return 0;
	nc = (int)ast_nchild(rir_arena, n);
	for (i = 0; i < nc; i++) {
		int w = rir_eff_size(ast_child(rir_arena, n, i), depth + 1);
		if (w > best)
			best = w;
	}
	return best;
}

static int rir_type_size(int st, uint64_t ref) {
	int al;
	CType b1;
	b1.t = st;
	b1.ref = (Sym *)(uintptr_t)ref;
	if (IS_ENUM(st) && !b1.ref)
		b1.t = (st & ~(unsigned)VT_BTYPE) | VT_INT;
	return type_size(&b1, &al);
}

static int rir_int_kind(int t) {
	int bt = t & VT_BTYPE;
	return !is_float(t) && bt != VT_PTR && bt != VT_STRUCT && bt != VT_FUNC &&
				 bt != VT_VOID;
}

static int rir_eff_unsigned(AstLocal n, int depth) {
	int i, nc, best = -1, t;
	if (n == AST_NONE || depth > 8)
		return -1;
	t = ast_type_t(rir_arena, n);
	if (t != 0)
		return rir_int_kind(t) ? ((t & VT_UNSIGNED) ? 1 : 0) : -1;
	if (ast_kind(rir_arena, n) != AST_Binary &&
			ast_kind(rir_arena, n) != AST_Unary)
		return -1;
	nc = (int)ast_nchild(rir_arena, n);
	for (i = 0; i < nc; i++) {
		int u = rir_eff_unsigned(ast_child(rir_arena, n, i), depth + 1);
		if (u > best)
			best = u;
	}
	return best;
}

static int rir_child_uns_to_signed(AstLocal n, int st, uint64_t ref) {
	int i, nc = ast_nchild(rir_arena, n), ss;
	if (!rir_int_kind(st) || (st & VT_UNSIGNED))
		return 0;
	ss = rir_type_size(st, ref);
	for (i = 0; i < nc; i++) {
		AstLocal c = ast_child(rir_arena, n, i);
		if (rir_eff_unsigned(c, 0) == 1 && rir_eff_size(c, 0) == ss)
			return 1;
	}
	return 0;
}

static int rir_const_eval(AstLocal n, long long *out, int depth) {
	long long a, b;
	int op;
	if (n == AST_NONE || depth > 8)
		return 0;
	if (is_float(ast_type_t(rir_arena, n)))
		return 0;
	switch (ast_kind(rir_arena, n)) {
	case AST_Literal:
		*out = (long long)ast_ival(rir_arena, n);
		return 1;
	case AST_Convert:
		return ast_nchild(rir_arena, n) == 1 &&
					 rir_const_eval(ast_first_child(rir_arena, n), out, depth + 1);
	case AST_Binary:
		break;
	default:
		return 0;
	}
	if (ast_nchild(rir_arena, n) != 2)
		return 0;
	if (!rir_const_eval(ast_child(rir_arena, n, 0), &a, depth + 1) ||
			!rir_const_eval(ast_child(rir_arena, n, 1), &b, depth + 1))
		return 0;
	op = ast_op(rir_arena, n);
	if (op == '+')
		*out = a + b;
	else if (op == '-')
		*out = a - b;
	else if (op == '*')
		*out = a * b;
	else if (op == '&')
		*out = a & b;
	else if (op == '|')
		*out = a | b;
	else if (op == '^')
		*out = a ^ b;
	else
		return 0;
	return 1;
}

/* The parser folds a constant through its cast CHAIN and keeps only the answer;
   the arena keeps the arithmetic and whichever Convert left an op. Evaluating
   the subtree as if no conversion happened and comparing against the recorded
   constant is exactly the test for "a conversion in that chain changed the
   value", which is when the parser's own folded constant has to be used. */
static int rir_const_val_differs(AstLocal n, long long want) {
	long long v;
	return rir_const_eval(n, &v, 0) && v != want;
}

static int rir_ptr_elem_size(int t, uint64_t ref) {
	Sym *r = (Sym *)(uintptr_t)ref;
	CType pt;
	int al;
	if ((t & VT_BTYPE) != VT_PTR || !r)
		return -1;
	pt = r->type;
	if ((pt.t & VT_BTYPE) == VT_FUNC || (pt.t & VT_BTYPE) == VT_VOID)
		return -1;
	return type_size(&pt, &al);
}

static int rir_node_wider(AstLocal n, int st, uint64_t ref) {
	return rir_eff_size(n, 0) > rir_type_size(st, ref);
}

static int rir_child_wider(AstLocal n, int st, uint64_t ref) {
	int i, nc = ast_nchild(rir_arena, n), ss = rir_type_size(st, ref);
	for (i = 0; i < nc; i++)
		if (rir_eff_size(ast_child(rir_arena, n, i), 0) > ss)
			return 1;
	return 0;
}

static int rir_child_width_differs(AstLocal n, int st, uint64_t ref) {
	int i, nc = ast_nchild(rir_arena, n), al, ss = rir_type_size(st, ref);
	for (i = 0; i < nc; i++) {
		AstLocal c = ast_child(rir_arena, n, i);
		CType a1;
		if (c == AST_NONE)
			continue;
		a1.t = ast_type_t(rir_arena, c);
		if (a1.t == 0 || (a1.t & VT_BTYPE) == VT_STRUCT ||
				(a1.t & VT_BTYPE) == VT_FUNC)
			continue;
		a1.ref = (Sym *)(uintptr_t)ast_type_ref(rir_arena, c);
		if (type_size(&a1, &al) != ss)
			return 1;
	}
	return 0;
}

static int rir_sh_unbound_invoke(void) {
	int k;
	for (k = 0; k < rir_shn; k++)
		if (rir_sh[k] != AST_NONE &&
				ast_kind(rir_arena, rir_sh[k]) == AST_Invoke &&
				ast_parent(rir_arena, rir_sh[k]) == AST_NONE)
			return 1;
	return 0;
}

static AstLocal rir_val_node(AstLocal n) {
	int guard = 0;
	while (n != AST_NONE && ast_kind(rir_arena, n) == AST_BasicBlock &&
				 ast_nchild(rir_arena, n) > 0 && ++guard < 16)
		n = ast_child(rir_arena, n, ast_nchild(rir_arena, n) - 1);
	return n;
}

static void rir_stamp_sv(const SValue *base, int n) {
	int k, want;
	if (n < 0)
		return;
	want = n - ast_base_depth;
	for (k = 0; k < rir_shn && k < want; k++) {
		const SValue *v;
		AstLocal sk;
		if (!rir_shtype[k])
			continue;
		v = &base[ast_base_depth + k];
		sk = rir_val_node(rir_sh[k]);
		ast_set_type(rir_arena, sk, v->type.t,
								 (uint64_t)(uintptr_t)v->type.ref);
		if (rir_sh[k] == rir_fcs_node) {
			rir_fcs_node = AST_NONE;
			if ((v->type.t & VT_BTYPE) == VT_BOOL) {
				ast_set_type(rir_arena, rir_sh[k], VT_BYTE | VT_UNSIGNED, 0);
				ast_set_fbits(rir_arena, rir_sh[k],
											ast_fbits(rir_arena, rir_sh[k]) | AST_FB_CONVERT_FCS);
			}
		}
		if (rir_shtype[k] == 3) {
			AstLocal c = ast_first_child(rir_arena, sk);
			uint16_t ck = c == AST_NONE ? 0 : ast_kind(rir_arena, c);
			if (ck == AST_Ref || ck == AST_Literal)
				ast_set_type(rir_arena, c, v->type.t,
										 (uint64_t)(uintptr_t)v->type.ref);
		}
		if (rir_shtype[k] == 2) {
			ast_set_op(rir_arena, sk, v->r);
			ast_set_ival(rir_arena, sk, (uint64_t)v->c.i);
			ast_set_sym(rir_arena, sk, (uint64_t)(uintptr_t)v->sym);
			ast_set_wide(rir_arena, sk, ast_sv_hi(v),
									 v->r2 >= VT_CONST ? (unsigned)VT_CONST : (unsigned)v->r2);
		}
		rir_shtype[k] = 0;
	}
	if (rir_cvt_next)
		return;
	for (k = 0; k < rir_shn && k < want; k++) {
		const SValue *v = &base[ast_base_depth + k];
		AstLocal cur = rir_sh[k];
		int ct, cs, vs2, al;
		CType a1, b1;
		if (cur == AST_NONE || rir_shtype[k])
			continue;
		ct = ast_type_t(rir_arena, cur);
		if (ct == 0 || ct == v->type.t)
			continue;
		if ((ct & VT_BTYPE) == VT_STRUCT || (v->type.t & VT_BTYPE) == VT_STRUCT)
			continue;
		if (is_float(ct) != is_float(v->type.t))
			continue;
		if ((ct & VT_BTYPE) == VT_FUNC || (v->type.t & VT_BTYPE) == VT_FUNC)
			continue;
		if ((ct & VT_BTYPE) == VT_PTR || (v->type.t & VT_BTYPE) == VT_PTR)
			continue;
		if (ast_kind(rir_arena, cur) == AST_StoreVal)
			continue;
		a1.t = ct;
		a1.ref = (Sym *)(uintptr_t)ast_type_ref(rir_arena, cur);
		b1.t = v->type.t;
		b1.ref = v->type.ref;
		cs = type_size(&a1, &al);
		vs2 = type_size(&b1, &al);
		if (cs >= vs2) {
#ifdef MCC_JRN_HAVE_X86_PRIMS
			int bop = ast_op(rir_arena, cur);
			if (cs > vs2 && ast_kind(rir_arena, cur) == AST_Binary &&
					(bop == AST_OP_AXADD || bop == AST_OP_AXCHG ||
					 bop == AST_OP_ACMPXCHG))
				ast_set_type(rir_arena, cur, v->type.t,
										 (uint64_t)(uintptr_t)v->type.ref);
#endif
			continue;
		}
		{
			AstLocal cv = ast_node(rir_arena, AST_Convert);
			ast_set_type(rir_arena, cv, v->type.t,
									 (uint64_t)(uintptr_t)v->type.ref);
			ast_add_child(rir_arena, cv, cur);
			rir_sh[k] = cv;
		}
	}
	for (k = 0; k < rir_shn && k < want; k++) {
		const SValue *v = &base[ast_base_depth + k];
		AstLocal cur = rir_sh[k], cv, ld;
		int ct;
		uint16_t ck;
		Sym *ps;
		CType pt;
		if (cur == AST_NONE || rir_shtype[k])
			continue;
		if (!(v->r & VT_LVAL) || (v->r & VT_VALMASK) >= VT_CONST || v->c.i != 0)
			continue;
		if ((v->type.t & (VT_BTYPE | VT_ARRAY | VT_VLA)) != (v->type.t & VT_BTYPE))
			continue;
		if ((v->type.t & VT_BTYPE) == VT_STRUCT ||
				(v->type.t & VT_BTYPE) == VT_FUNC || (v->type.t & VT_BTYPE) == VT_VOID)
			continue;
		ck = ast_kind(rir_arena, cur);
		if (ck == AST_Load || ck == AST_Invoke || ck == AST_StoreVal)
			continue;
		ct = ast_type_t(rir_arena, cur);
		if (ct != 0 && (ct & (VT_BTYPE | VT_ARRAY)) != VT_PTR)
			continue;
		if (ct == (int)v->type.t)
			continue;
		if (ct == 0 && ck != AST_Binary)
			continue;
		if (ct != 0) {
			const Sym *pr = (const Sym *)(uintptr_t)ast_type_ref(rir_arena, cur);
			if (pr && pr->type.t == v->type.t && pr->type.ref == v->type.ref)
				continue;
		}
		pt.t = v->type.t;
		pt.ref = v->type.ref;
		ps = rir_ptr_sym(&pt);
		if (!ps)
			continue;
		cv = ast_node(rir_arena, AST_Convert);
		ast_set_type(rir_arena, cv, VT_PTR, (uint64_t)(uintptr_t)ps);
		ast_add_child(rir_arena, cv, cur);
		ld = ast_node(rir_arena, AST_Load);
		ast_add_child(rir_arena, ld, cv);
		rir_sh[k] = ld;
	}
}

static void rir_stamp_call_top(const SValue *base, int n) {
	int k = n - ast_base_depth - 1;
	const SValue *v;
	AstLocal sk;
	if (k < 0 || k != rir_shn - 1 || rir_shn <= 0)
		return;
	sk = rir_sh[k] == AST_NONE ? AST_NONE : rir_val_node(rir_sh[k]);
	if (rir_shtype[k] != 2 || sk == AST_NONE ||
			ast_kind(rir_arena, sk) != AST_Invoke)
		return;
	v = &base[ast_base_depth + k];
	ast_set_type(rir_arena, sk, v->type.t, (uint64_t)(uintptr_t)v->type.ref);
	ast_set_op(rir_arena, sk, v->r);
	ast_set_ival(rir_arena, sk, (uint64_t)v->c.i);
	ast_set_sym(rir_arena, sk, (uint64_t)(uintptr_t)v->sym);
	ast_set_wide(rir_arena, sk, ast_sv_hi(v),
							 v->r2 >= VT_CONST ? (unsigned)VT_CONST : (unsigned)v->r2);
	rir_shtype[k] = 0;
}

static AstLocal rir_spill_take(const SValue *sv) {
	AstLocal n;
	int al, sz;
	CType st2;
	if (rir_spill_node == AST_NONE || sv->sym ||
			(sv->r & VT_VALMASK) != VT_LOCAL || !(sv->r & VT_LVAL) ||
			(sv->type.t & VT_BTYPE) != VT_STRUCT)
		return AST_NONE;
	st2.t = sv->type.t;
	st2.ref = sv->type.ref;
	sz = type_size(&st2, &al);
	if (rir_spill_addr < (long long)sv->c.i ||
			rir_spill_addr >= (long long)sv->c.i + (sz > 0 ? sz : 1))
		return AST_NONE;
	n = rir_spill_node;
	rir_spill_node = AST_NONE;
	if (ast_parent(rir_arena, n) != AST_NONE)
		return AST_NONE;
	{
		AstLocal iv = rir_val_node(n);
		ast_set_type(rir_arena, iv, sv->type.t, (uint64_t)(uintptr_t)sv->type.ref);
		ast_set_op(rir_arena, iv, sv->r);
		ast_set_ival(rir_arena, iv, (uint64_t)sv->c.i);
		ast_set_sym(rir_arena, iv, 0);
	}
	return n;
}

static void rir_reconcile_sv(const SValue *base, int n) {
	int want, k;
	if (n < 0)
		return;
	rir_stamp_sv(base, n);
	want = n - ast_base_depth;
	if (want < 0)
		want = 0;
	if (want > VSTACK_SIZE)
		want = VSTACK_SIZE;
	while (rir_shn > want) {
		AstLocal d = rir_pop();
		if (rir_effectful(d))
			rir_stmt(d);
	}
	if (rir_after_ret && rir_shn == 0)
		return;
	if (rir_shn < want)
		rir_tot_refill++;
	for (k = rir_shn; k < want; k++) {
		const SValue *sv3 = &base[ast_base_depth + k];
		AstLocal sp = rir_spill_take(sv3);
		rir_tot_leaf++;
		rir_push(sp == AST_NONE ? rir_leaf_slot(sv3, ast_base_depth + k) : sp);
		if (k < want - 1 && rir_pending_ret == AST_NONE && !rir_gret_depth)
			rir_arena_mismatch++;
	}
}

static void rir_stamp_types(const JrnOp *o) {
	if (o->vs_n < 0)
		return;
	rir_stamp_sv(jrn_vs + o->vs_off, o->vs_n);
}

static void rir_reconcile(const JrnOp *o) {
	if (o->vs_n < 0)
		return;
	rir_reconcile_sv(jrn_vs + o->vs_off, o->vs_n);
}

static int rir_opassign_pending;
static int rir_addr_late;

static int rir_ternn;
static int rir_lorn;

static void rir_op_effect(const RirOp *ro) {
	const JrnOp *o = &ro->p;
	int k;
	if (o->kind != JOP_VSETC && o->kind != JOP_PUSHLIT)
		rir_flush_pending_call();
	if (o->nocode)
		return;
	switch (o->kind) {
	case JOP_GENOP:
	case JOP_OPI:
	case JOP_OPL:
	case JOP_OPF: {
		AstLocal b, a, n;
		if (rir_after_ret && rir_shn == 0)
			break;
		if (o->kind == JOP_OPF && o->a0 == TOK_NEG) {
			a = rir_pop();
			if (a == AST_NONE) {
				rir_arena_mismatch++;
				break;
			}
			n = ast_node(rir_arena, AST_Unary);
			ast_set_op(rir_arena, n, AST_OP_FNEG);
			ast_add_child(rir_arena, n, a);
			rir_push_typed(n);
			break;
		}
		if (o->vs_n - ast_base_depth >= 2 && rir_shn >= 2) {
			int q;
			for (q = 0; q < 2; q++) {
				AstLocal cur = rir_sh[rir_shn - 1 - q], ch;
				if (cur == AST_NONE || rir_shtype[rir_shn - 1 - q] ||
						ast_kind(rir_arena, cur) != AST_Convert ||
						(ast_type_t(rir_arena, cur) & (VT_BTYPE | VT_UNSIGNED)) !=
								(VT_INT | VT_UNSIGNED))
					continue;
				ch = ast_first_child(rir_arena, cur);
				if (ch == AST_NONE || !(ast_type_t(rir_arena, ch) & VT_BITFIELD) ||
						BIT_SIZE(ast_type_t(rir_arena, ch)) == 32)
					continue;
				{
					AstLocal cv = ast_node(rir_arena, AST_Convert);
					ast_set_type(rir_arena, cv,
											 ast_type_t(rir_arena, cur) & ~(unsigned)VT_UNSIGNED, 0);
					ast_add_child(rir_arena, cv, cur);
					rir_sh[rir_shn - 1 - q] = cv;
				}
			}
		}
		if (o->vs_n - ast_base_depth >= 2 && rir_shn >= 2) {
			int q, opdiff = rir_tcore(jrn_vs[o->vs_off + o->vs_n - 1].type.t) !=
											rir_tcore(jrn_vs[o->vs_off + o->vs_n - 2].type.t);
			for (q = 0; q < 2; q++) {
				AstLocal cur = rir_sh[rir_shn - 1 - q];
				const SValue *sv2 = &jrn_vs[o->vs_off + o->vs_n - 1 - q];
				int st = sv2->type.t;
				if (cur == AST_NONE || rir_shtype[rir_shn - 1 - q])
					continue;
				if (ast_type_t(rir_arena, cur) != 0 ||
						ast_kind(rir_arena, cur) != AST_Binary)
					continue;
				if (TOK_ISCOND(ast_op(rir_arena, cur)))
					continue;
				if (st == 0 || (st & VT_BTYPE) == VT_STRUCT ||
						(st & VT_BTYPE) == VT_FUNC || is_float(st))
					continue;
#if RIR_DBG_OPTRACE
				{
					const char *e = getenv("RIRDBG");
					if (e && funcname && !strcmp(e, funcname))
						fprintf(stderr,
										"[gop] ent=%d q=%d op=%d st=%x curt=%x hastype=%d opdiff=%d widthdiff=%d\n",
										rir_dbg_ent, q, o->a0, (unsigned)st,
										(unsigned)ast_type_t(rir_arena, cur),
										rir_child_has_type(cur, st), opdiff,
										rir_child_width_differs(cur, st, (uint64_t)(uintptr_t)sv2->type.ref));
				}
#endif
				if (rir_child_has_type(cur, st) && !rir_child_wider(cur, st, (uint64_t)(uintptr_t)sv2->type.ref))
					continue;
				if (!opdiff && !rir_child_width_differs(cur, st, (uint64_t)(uintptr_t)sv2->type.ref) &&
						!rir_child_wider(cur, st, (uint64_t)(uintptr_t)sv2->type.ref) &&
						!rir_child_uns_to_signed(cur, st, (uint64_t)(uintptr_t)sv2->type.ref))
					continue;
				{
					AstLocal cv = ast_node(rir_arena, AST_Convert);
					ast_set_type(rir_arena, cv, st, (uint64_t)(uintptr_t)sv2->type.ref);
					ast_add_child(rir_arena, cv, cur);
					rir_sh[rir_shn - 1 - q] = cv;
				}
			}
		}
		if ((o->a0 == '+' || o->a0 == '-') &&
				o->vs_n - ast_base_depth >= 2 && rir_shn >= 2) {
			int q;
			for (q = 0; q < 2; q++) {
				int si = rir_shn - 1 - q, ct;
				AstLocal cur = rir_sh[si];
				const SValue *sv3 = &jrn_vs[o->vs_off + o->vs_n - 1 - q];
				int st = sv3->type.t;
				if (cur == AST_NONE || rir_shtype[si])
					continue;
				ct = ast_type_t(rir_arena, cur);
				if ((ct & VT_BTYPE) != VT_PTR || (st & VT_BTYPE) != VT_PTR)
					continue;
				if (rir_ptr_elem_size(ct, ast_type_ref(rir_arena, cur)) ==
						rir_ptr_elem_size(st, (uint64_t)(uintptr_t)sv3->type.ref))
					continue;
				{
					AstLocal cv = ast_node(rir_arena, AST_Convert);
					ast_set_type(rir_arena, cv, st, (uint64_t)(uintptr_t)sv3->type.ref);
					ast_add_child(rir_arena, cv, cur);
					rir_sh[si] = cv;
				}
			}
		}
		b = rir_pop();
		a = rir_pop();
		if (a == AST_NONE || b == AST_NONE) {
			rir_arena_mismatch++;
			rir_push(ast_node(rir_arena, AST_Poison));
			break;
		}
		n = ast_node(rir_arena, AST_Binary);
		ast_set_op(rir_arena, n, o->a0);
		if (o->vs_n - ast_base_depth >= 2) {
			const SValue *rs = &jrn_vs[o->vs_off + o->vs_n - 1];
			const SValue *ls = &jrn_vs[o->vs_off + o->vs_n - 2];
			if ((rs->r & VT_VALMASK) < VT_CONST && !(rs->r & VT_LVAL) &&
					(ls->r & VT_LVAL) && (ls->r & VT_VALMASK) >= VT_CONST &&
					b != AST_NONE && (ast_kind(rir_arena, b) == AST_Load ||
														ast_kind(rir_arena, b) == AST_Ref))
				ast_set_fbits(rir_arena, n,
											ast_fbits(rir_arena, n) | AST_FB_BINARY_RHS_GV);
		}
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
#if RIR_DBG_OPTRACE
		{
			const char *e = getenv("RIRDBG");
			const SValue *ds = o->vs_n - ast_base_depth >= 2
													 ? &jrn_vs[o->vs_off + o->vs_n - 2] : 0;
			if (e && funcname && !strcmp(e, funcname))
				fprintf(stderr,
								"[vst] ent=%d cd=%d vkind=%s vpar=%d depth=%d tgt(r=%x sym=%d c=%lld) tkind=%s\n",
								rir_dbg_ent, rir_call_depth,
								ast_kind_name(ast_kind(rir_arena, rir_val_node(v))),
								(int)ast_parent(rir_arena, v), o->vs_n - ast_base_depth,
								ds ? (unsigned)ds->r : 0u, ds && ds->sym ? 1 : 0,
								ds ? (long long)ds->c.i : 0LL,
								ast_kind_name(ast_kind(rir_arena, t)));
		}
#endif
		if (rir_call_depth && ast_parent(rir_arena, v) == AST_NONE &&
				o->vs_n - ast_base_depth >= 2) {
			const SValue *ts = &jrn_vs[o->vs_off + o->vs_n - 2];
			int isinv = ast_kind(rir_arena, rir_val_node(v)) == AST_Invoke;
			int istail = !isinv && ast_kind(rir_arena, v) == AST_Ref &&
									 ast_nchild(rir_arena, v) == 0 && rir_sh_unbound_invoke();
			if ((isinv || istail) && !ts->sym &&
					(ts->r & VT_VALMASK) == VT_LOCAL && (ts->r & VT_LVAL)) {
				if (isinv) {
					rir_spill_node = v;
					rir_spill_addr = (long long)ts->c.i;
				}
				rir_push(AST_NONE);
				break;
			}
		}
		if (ast_kind(rir_arena, t) == AST_Binary &&
				ast_op(rir_arena, t) == '+' && ast_nchild(rir_arena, t) == 2 &&
				o->vs_n - ast_base_depth >= 2) {
			AstLocal ad = ast_child(rir_arena, t, 0);
			AstLocal lt = ast_child(rir_arena, t, 1);
			if (ad != AST_NONE && lt != AST_NONE &&
					ast_kind(rir_arena, ad) == AST_Unary &&
					ast_op(rir_arena, ad) == AST_OP_ADDR &&
					ast_nchild(rir_arena, ad) == 1 &&
					ast_kind(rir_arena, lt) == AST_Literal &&
					(ast_op(rir_arena, lt) & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
				const SValue *tsv = &jrn_vs[o->vs_off + o->vs_n - 2];
				AstLocal mb = ast_node(rir_arena, AST_Unary);
				ast_set_op(rir_arena, mb, AST_OP_MEMBER);
				ast_set_ival(rir_arena, mb, ast_ival(rir_arena, lt));
				ast_set_type(rir_arena, mb, tsv->type.t,
										 (uint64_t)(uintptr_t)tsv->type.ref);
				ast_add_child(rir_arena, mb,
											ast_dup_sub(rir_arena, ast_first_child(rir_arena, ad)));
				t = mb;
			}
		}
		{
			int chained = 0;
			if (ast_kind(rir_arena, v) == AST_StoreVal &&
					ast_nchild(rir_arena, v) == 0) {
				AstLocal src = (AstLocal)ast_ival(rir_arena, v);
				if (src < ast_count(rir_arena) &&
						ast_kind(rir_arena, src) == AST_Store &&
						ast_nchild(rir_arena, src) == 2) {
					v = ast_dup_sub(rir_arena, ast_child(rir_arena, src, 1));
					chained = 1;
				}
			}
			n = ast_node(rir_arena, AST_Store);
			if (chained)
				ast_set_fbits(rir_arena, n, ast_fbits(rir_arena, n) | 1u);
			if (rir_is_cmp_binary(v) && o->vs_n - ast_base_depth >= 2 &&
					jrn_vs[o->vs_off + o->vs_n - 1].r != VT_CMP &&
					(jrn_vs[o->vs_off + o->vs_n - 1].type.t & VT_BTYPE) != VT_BOOL &&
					(jrn_vs[o->vs_off + o->vs_n - 2].type.t & VT_BTYPE) == VT_BOOL)
				ast_set_fbits(rir_arena, n,
											ast_fbits(rir_arena, n) | AST_FB_STORE_CMP_GV);
			if (rir_addr_late)
				ast_set_fbits(rir_arena, n,
											ast_fbits(rir_arena, n) | AST_FB_STORE_ADDR_LATE);
		}
		rir_addr_late = 0;
		if (rir_lorn || rir_ternn || rir_docond) {
			ast_add_child(rir_arena, n, t);
			ast_add_child(rir_arena, n, v);
			rir_push(n);
			break;
		}
		if (rir_vstn)
			rir_vst_seen[rir_vstn - 1] = 1;
		if (rir_opassign_pending)
			ast_set_op(rir_arena, n, AST_OP_OPASSIGN);
		rir_opassign_pending = 0;
		ast_add_child(rir_arena, n, t);
		ast_add_child(rir_arena, n, v);
		rir_stmt(n);
		rir_ret_follow_spill(t, v);
		rir_spill_follow_sh(t, v);
		{
			AstLocal mv = ast_node(rir_arena, AST_StoreVal);
			ast_set_type(rir_arena, mv, ast_type_t(rir_arena, v),
									 ast_type_ref(rir_arena, v));
			ast_set_ival(rir_arena, mv, (uint64_t)n);
			rir_push(mv);
		}
		break;
	}
#ifdef MCC_JRN_HAVE_X86_PRIMS
	case JOP_SIGNBIT:
	case JOP_FFS:
	case JOP_BITSCAN:
	case JOP_BSWAP: {
		AstLocal v = rir_shn ? rir_pop() : AST_NONE;
		AstLocal n;
		if (v == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, n,
			o->kind == JOP_BSWAP ? AST_OP_BSWAP
			: o->kind == JOP_SIGNBIT ? AST_OP_SIGNBIT
			: o->kind == JOP_FFS ? AST_OP_FFS
			: AST_OP_BITSCAN);
		ast_set_ival(rir_arena, n,
			(uint64_t)(unsigned)o->a0 | ((uint64_t)(unsigned)o->a1 << 32));
		ast_add_child(rir_arena, n, v);
		rir_push_typed(n);
		break;
	}
	case JOP_ATOMIC_XADD:
	case JOP_ATOMIC_XCHG:
	case JOP_ATOMIC_CMPXCHG: {
		int na = o->kind == JOP_ATOMIC_CMPXCHG ? 3 : 2;
		AstLocal aops[3], n;
		int q, bad = 0;
		if (rir_shn < na) {
			rir_arena_mismatch++;
			break;
		}
		for (q = na - 1; q >= 0; q--) {
			aops[q] = rir_pop();
			if (aops[q] == AST_NONE)
				bad = 1;
		}
		if (bad) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Binary);
		ast_set_op(rir_arena, n,
							 o->kind == JOP_ATOMIC_XADD
									 ? AST_OP_AXADD
									 : o->kind == JOP_ATOMIC_XCHG ? AST_OP_AXCHG
																								: AST_OP_ACMPXCHG);
		ast_set_ival(rir_arena, n, (uint64_t)(unsigned)o->a0);
		for (q = 0; q < na; q++)
			ast_add_child(rir_arena, n, aops[q]);
		for (q = 1; q < na; q++)
			rir_push(AST_NONE);
		rir_push_typed(n);
		break;
	}
#endif
	case JOP_BITBUILTIN: {
		AstLocal v = rir_shn ? rir_pop() : AST_NONE;
		AstLocal n;
		if (v == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, n, AST_OP_BITB);
		ast_set_ival(rir_arena, n,
			(uint64_t)(unsigned)o->a0 | ((uint64_t)(unsigned)o->a1 << 32));
		ast_add_child(rir_arena, n, v);
		rir_push_typed(n);
		break;
	}
	case JOP_CALL: {
		AstLocal n;
		int na = o->a0;
		AstLocal args[32];
		int nfixed = -1;
		if (na < 0 || na > 32) {
			rir_arena_mismatch++;
			na = 0;
		}
		if (o->vs_n - ast_base_depth >= na + 1 && rir_shn >= na + 1) {
			int q;
			int hidden = -1;
			{
				const SValue *cs = &jrn_vs[o->vs_off + o->vs_n - 1 - na];
				const Sym *fs = (const Sym *)(uintptr_t)cs->type.ref;
				if (fs && (cs->type.t & VT_BTYPE) == VT_PTR)
					fs = fs->type.ref;
				nfixed = -1;
				if (fs) {
					const Sym *pa;
					int nparam = 0;
					for (pa = fs->next; pa; pa = pa->next)
						nparam++;
					if (fs->f.func_type == FUNC_ELLIPSIS)
						nfixed = nparam;
					if ((fs->type.t & VT_BTYPE) == VT_STRUCT && na == nparam + 1)
						hidden = na - 1;
				}
			}
			for (q = 0; q <= na && rir_argcast_n; q++) {
				int si = rir_shn - 1 - q;
				if (q == hidden)
					continue;
				if (nfixed >= 0 && q < na && na - 1 - q >= nfixed &&
					rir_sh[si] != AST_NONE &&
					ast_type_t(rir_arena, rir_sh[si]) ==
						jrn_vs[o->vs_off + o->vs_n - 1 - q].type.t &&
					!((jrn_vs[o->vs_off + o->vs_n - 1 - q].r &
						 (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST &&
						rir_const_val_differs(rir_sh[si],
																	 jrn_vs[o->vs_off + o->vs_n - 1 - q].c.i)))
					continue;
				AstLocal cur = rir_sh[si];
				const SValue *sv2 = &jrn_vs[o->vs_off + o->vs_n - 1 - q];
				int st = sv2->type.t;
				int ai = rir_argcast_n - 1 - q;
				if (q < na && cur != AST_NONE &&
						ast_kind(rir_arena, cur) == AST_Load &&
						ai >= 0 && ai < RIR_ARGCAST_MAX && !rir_argcast_ch[ai])
					continue;
#if RIR_DBG_OPTRACE
				{
					const char *e = getenv("RIRDBG");
					if (e && funcname && !strcmp(e, funcname))
						fprintf(stderr,
										"[arg] ent=%d q=%d na=%d nfixed=%d hidden=%d si=%d cur=%s shtype=%d curt=%x st=%x\n",
										rir_dbg_ent, q, na, nfixed, hidden, si,
										cur == AST_NONE ? "-" : ast_kind_name(ast_kind(rir_arena, cur)),
										si >= 0 ? rir_shtype[si] : -1,
										cur == AST_NONE ? 0 : (unsigned)ast_type_t(rir_arena, cur),
										(unsigned)st);
				}
#endif
				if (cur == AST_NONE || rir_shtype[si])
					continue;
				if (ast_kind(rir_arena, cur) == AST_Convert &&
						!((st & VT_BTYPE) != VT_STRUCT && (st & VT_BTYPE) != VT_FUNC &&
							rir_node_wider(cur, st, (uint64_t)(uintptr_t)sv2->type.ref)))
					continue;
				if (st == 0 || (st & VT_BTYPE) == VT_FUNC)
					continue;
				if ((ast_type_t(rir_arena, cur) & VT_BTYPE) == VT_VOID &&
						ast_kind(rir_arena, cur) != AST_Binary &&
						ast_kind(rir_arena, cur) != AST_Load)
					continue;
				if ((sv2->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST &&
						rir_const_subtree(cur, 0) &&
						(((st & VT_BTYPE) == VT_INT128 || (st & VT_BTYPE) == VT_QLONG) ||
						 (!is_float(st) && rir_const_val_differs(cur, sv2->c.i)))) {
					rir_sh[si] = rir_leaf(sv2);
					continue;
				}
				{
					AstLocal cv = ast_node(rir_arena, AST_Convert);
					ast_set_type(rir_arena, cv, st, (uint64_t)(uintptr_t)sv2->type.ref);
					ast_add_child(rir_arena, cv, cur);
					rir_sh[si] = cv;
				}
			}
		}
		rir_argcast_n = 0;
		n = ast_node(rir_arena, AST_Invoke);
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
		rir_pending_call = n;
		break;
	}
	case JOP_PUSHLIT:
	case JOP_VSETC:
		if (o->vs_n >= 0 && o->vs_n <= VSTACK_SIZE) {
			int q;
			rir_pvt[o->vs_n] = o->ctype;
			rir_pvr[o->vs_n] = o->a0;
			rir_pvc[o->vs_n] = o->cval;
			rir_pvok[o->vs_n] = 1;
			for (q = o->vs_n + 1; q <= VSTACK_SIZE; q++)
				rir_pvok[q] = 0;
		}
		if (rir_pending_call != AST_NONE) {
			if (o->kind == JOP_PUSHLIT || (o->a0 & VT_VALMASK) < VT_CONST ||
					((o->ctype.t & VT_BTYPE) == VT_STRUCT &&
					 (o->a0 & VT_VALMASK) == VT_LOCAL && (o->a0 & VT_LVAL))) {
				rir_push_typed(rir_pending_call);
				if (rir_shn > 0)
					rir_shtype[rir_shn - 1] = 2;
				rir_pending_call = AST_NONE;
			} else {
				rir_flush_pending_call();
			}
		}
		break;
	case JOP_STORE: {
		AstLocal v, n, ad, ld;
		const SValue *spv = NULL;
		SValue tsv;
		CType pt;
		Sym *ps;
		int q, slot = -1, lv = 0;
		if (o->vs_n <= 0 ||
				(o->svarg.r & (VT_VALMASK | VT_LVAL | VT_SYM)) !=
						(VT_LOCAL | VT_LVAL) ||
				o->svarg.sym)
			break;
		for (q = 0; q <= o->vs_n - 1 - ast_base_depth; q++) {
			const SValue *sv4 = &jrn_vs[o->vs_off + ast_base_depth + q];
			if ((sv4->r & VT_VALMASK) == (o->a0 & VT_VALMASK)) {
				slot = q;
				lv = (sv4->r & VT_LVAL) != 0;
				spv = sv4;
				break;
			}
		}
		if (slot < 0 || slot >= rir_shn)
			break;
		/* The bottom of the shadow stack during an assignment is the TARGET, and
		   the emitter's own save_regs spills it at the branch. Modelling that
		   spill puts a Store at the head of the block and a Load in the target's
		   place, so the trial spills and reloads before the condition where the
		   parser spills after the compare. */
		if (slot == 0 && rir_shn >= 2 && lv && !ast_func_has_asm &&
				rir_sh[slot] != AST_NONE &&
				ast_kind(rir_arena, rir_sh[slot]) == AST_Load)
			break;
		v = rir_sh[slot];
		if (v == AST_NONE || ast_parent(rir_arena, v) != AST_NONE)
			break;
		if (!lv) {
			if (!is_float(o->svarg.type.t))
				break;
			n = ast_node(rir_arena, AST_Store);
			ast_add_child(rir_arena, n, rir_leaf(&o->svarg));
			ast_add_child(rir_arena, n, v);
			rir_stmt(n);
			rir_sh[slot] = rir_leaf(&o->svarg);
			rir_shtype[slot] = 0;
			break;
		}
		/* save_reg_upstack builds its spill target as a stack SValue and never
		 * sets type.ref, so the snapshot's pointee is indeterminate -- reading
		 * it back at the Load below dereferences whatever the stack held.  The
		 * slot takes the ADDRESS of the spilled lvalue, so the pointee is that
		 * entry's own type; intern it in Replay_IR's own pointer pool rather
		 * than trusting the capture. */
		pt.t = spv->type.t;
		pt.ref = spv->type.ref;
		ps = rir_ptr_sym(&pt);
		if (!ps)
			break;
		tsv = o->svarg;
		tsv.type.ref = ps;
		ad = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, ad, AST_OP_ADDR);
		ast_set_type(rir_arena, ad, tsv.type.t, (uint64_t)(uintptr_t)tsv.type.ref);
		ast_add_child(rir_arena, ad, v);
		n = ast_node(rir_arena, AST_Store);
		ast_add_child(rir_arena, n, rir_leaf(&tsv));
		ast_add_child(rir_arena, n, ad);
		rir_stmt(n);
		ld = ast_node(rir_arena, AST_Load);
		ast_add_child(rir_arena, ld, rir_leaf(&tsv));
		rir_sh[slot] = ld;
		rir_shtype[slot] = 0;
		break;
	}
	case JOP_VPOP: {
		AstLocal d = rir_pop();
		if (d != AST_NONE && rir_shn == 0 &&
				ast_parent(rir_arena, d) == AST_NONE &&
				ast_kind(rir_arena, d) == AST_Binary) {
			ast_set_fbits(rir_arena, d,
										ast_fbits(rir_arena, d) | AST_FB_STMT_DISCARD);
			rir_stmt(d);
			break;
		}
		rir_drop(d);
		break;
	}
	case JOP_VROTB: {
		int m = o->a0 - 1;
		if (m >= 1 && rir_shn > m) {
			AstLocal tmp = rir_sh[rir_shn - 1 - m];
			memmove(&rir_sh[rir_shn - 1 - m], &rir_sh[rir_shn - m],
							sizeof(AstLocal) * (size_t)m);
			rir_sh[rir_shn - 1] = tmp;
			memmove(&rir_shtype[rir_shn - 1 - m], &rir_shtype[rir_shn - m],
							sizeof(unsigned char) * (size_t)m);
			rir_shtype[rir_shn - 1] = 0;
		}
		break;
	}
	case JOP_VROTT: {
		int m = o->a0 - 1;
		if (m >= 1 && rir_shn > m) {
			AstLocal tmp = rir_sh[rir_shn - 1];
			memmove(&rir_sh[rir_shn - m], &rir_sh[rir_shn - 1 - m],
							sizeof(AstLocal) * (size_t)m);
			rir_sh[rir_shn - 1 - m] = tmp;
			memmove(&rir_shtype[rir_shn - m], &rir_shtype[rir_shn - 1 - m],
							sizeof(unsigned char) * (size_t)m);
			rir_shtype[rir_shn - 1 - m] = 0;
		}
		break;
	}
	case JOP_VREV: {
		int i, j;
		if (o->a0 >= 2 && rir_shn >= o->a0) {
			for (i = rir_shn - o->a0, j = rir_shn - 1; i < j; i++, j--) {
				AstLocal t2 = rir_sh[i];
				unsigned char c2 = rir_shtype[i];
				rir_sh[i] = rir_sh[j];
				rir_sh[j] = t2;
				rir_shtype[i] = rir_shtype[j];
				rir_shtype[j] = c2;
			}
		}
		break;
	}
	case JOP_GV: {
		AstLocal top;
		/* A gv on a sub-int integer lvalue IS the promotion: it emits the
		   sign- or zero-extending load right here. Inside a ternary arm that
		   placement is the whole answer -- the arm's value has to be in a
		   register before the merge, and an arena that only carries the char
		   Load leaves replay to materialise it after the merge point, moving
		   the movsx past the arm's jump. The tree records this as a Convert
		   over the Load; record the same. */
		if (rir_shn >= 1 && o->vs_n > 0) {
			const SValue *tv = &jrn_vs[o->vs_off + o->vs_n - 1];
			int bt = tv->type.t & VT_BTYPE;
			AstLocal t2 = rir_sh[rir_shn - 1];
			if ((tv->r & VT_LVAL) && !(tv->type.t & (VT_ARRAY | VT_BITFIELD)) &&
					(bt == VT_BYTE || bt == VT_SHORT || bt == VT_BOOL ||
					 bt == VT_INT) &&
					t2 != AST_NONE && ast_kind(rir_arena, t2) == AST_Load &&
					ast_parent(rir_arena, t2) == AST_NONE) {
				int nt = (int)ast_type_t(rir_arena, t2);
				int want = -1;
				/* The snapshot is what the emitter is about to widen FROM, and a
				   cast that only flipped signedness leaves no op behind -- the
				   node still says char where the parser has unsigned char, and
				   replay writes movsx for the parser's movzx. */
				if (!nt && ((tv->type.t & VT_UNSIGNED) || bt == VT_INT))
					want = tv->type.t;
				else if (rir_ternn)
					want = VT_INT;
				if (want >= 0) {
					AstLocal cv = ast_node(rir_arena, AST_Convert);
					ast_set_type(rir_arena, cv, want,
											 want == VT_INT
													 ? (uint64_t)0
													 : (uint64_t)(uintptr_t)tv->type.ref);
					ast_set_fbits(rir_arena, cv, AST_FB_CONVERT_GV);
					ast_add_child(rir_arena, cv, t2);
					rir_sh[rir_shn - 1] = cv;
					rir_shtype[rir_shn - 1] = 1;
					break;
				}
			}
		}
		if (rir_shn < 1 || !rir_callee_pending())
			break;
		top = rir_sh[rir_shn - 1];
		if (top == AST_NONE || rir_shtype[rir_shn - 1] ||
				ast_parent(rir_arena, top) != AST_NONE)
			break;
		if (ast_kind(rir_arena, top) == AST_Ref &&
				!(ast_type_t(rir_arena, top) & (VT_BITFIELD | VT_ARRAY)) &&
				(ast_type_t(rir_arena, top) & VT_BTYPE) == VT_PTR) {
			AstLocal cv = ast_node(rir_arena, AST_Convert);
			ast_set_type(rir_arena, cv, ast_type_t(rir_arena, top),
									 ast_type_ref(rir_arena, top));
			ast_set_fbits(rir_arena, cv, AST_FB_CONVERT_GV);
			ast_add_child(rir_arena, cv, top);
			rir_sh[rir_shn - 1] = cv;
		}
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
		ast_add_child(rir_arena, n, a);
		if (o->kind == JOP_CVT_CSTI && rir_synth_depth)
			rir_fcs_node = n;
		rir_push_typed(n);
		break;
	}
#if MCC_CONFIG_ASM
	case JOP_ASMGEN:
	case JOP_ASM: {
		AstLocal u = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, u,
							 o->kind == JOP_ASMGEN ? AST_OP_ASMGEN : AST_OP_ASM);
		ast_set_ival(rir_arena, u,
								 (uint64_t)(unsigned)o->raw_off |
										 ((uint64_t)(unsigned)o->raw_len << 32));
		ast_set_sym(rir_arena, u,
								(uint64_t)(unsigned)o->a0 | ((uint64_t)(unsigned)o->a1 << 32));
		ast_set_fbits(rir_arena, u,
									(uint64_t)(unsigned)o->vs_off |
											((uint64_t)(unsigned)(o->vs_n > 0 ? o->vs_n : 0) << 32));
		rir_stmt(u);
		break;
	}
#endif
#ifdef MCC_JRN_VA_START_VOID
	case JOP_VA_START: {
		AstLocal b = rir_pop(), a2 = rir_pop(), n;
		if (a2 == AST_NONE || b == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Binary);
		ast_set_op(rir_arena, n, AST_OP_VASTART);
		ast_add_child(rir_arena, n, a2);
		ast_add_child(rir_arena, n, b);
		rir_stmt(n);
		break;
	}
#endif
#if defined(MCC_JRN_HAVE_VA_START) && !defined(MCC_JRN_VA_START_VOID)
	case JOP_VA_START: {
		AstLocal b = rir_pop(), n;
		if (b == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, n, AST_OP_VASTART);
		ast_add_child(rir_arena, n, b);
		rir_push(n);
		break;
	}
#endif
#ifdef MCC_JRN_HAVE_VA_ARG
	case JOP_VA_ARG: {
		AstLocal a = rir_pop(), n;
		if (a == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, n, AST_OP_VAARG);
		ast_set_type(rir_arena, n, o->ctype.t, (uint64_t)(uintptr_t)o->ctype.ref);
		ast_add_child(rir_arena, n, a);
		rir_push(n);
		break;
	}
#endif
	case JOP_GGOTO: {
		AstLocal a = rir_pop(), n;
		if (a == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, n, AST_OP_GGOTO);
		ast_add_child(rir_arena, n, a);
		rir_stmt(n);
		break;
	}
	case JOP_ADDROF: {
		AstLocal a;
		AstLocal n;
		if (rir_after_ret && rir_shn == 0)
			break;
		a = rir_pop();
		if (a == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, n, AST_OP_ADDR);
		ast_add_child(rir_arena, n, a);
		rir_push_typed_addr(n);
		break;
	}
	default:
		break;
	}
}

static int rir_cond_depth, rir_synth_depth, rir_call_depth, rir_inc_depth;
#define RIR_CVT_MAX 32
static int rir_cvt_depth, rir_cvt_n;
static unsigned char rir_cvt_on[RIR_CVT_MAX];
static int rir_member_depth;
static int rir_vstruct_depth;
static int rir_vla_depth;
static AstLocal rir_incr_bb = AST_NONE;
static int rir_incr_live;
static AstLocal rir_tern[16];
static int rir_tern_cf[16];
static AstLocal rir_lor[16];
#define RIR_TMASK                                                              \
	(~(unsigned)(VT_ARRAY | VT_CONSTANT | VT_VOLATILE | VT_NONCONST |            \
							 VT_NONLVAL | VT_DEFSIGN))

static int rir_ptr_arith(AstLocal n, const SValue *pv) {
	int i, nc = ast_nchild(rir_arena, n);
	for (i = 0; i < nc; i++) {
		AstLocal c = ast_child(rir_arena, n, i);
		if (c != AST_NONE &&
				(ast_type_t(rir_arena, c) & RIR_TMASK) ==
						((unsigned)pv->type.t & RIR_TMASK) &&
				ast_type_ref(rir_arena, c) == (uint64_t)(uintptr_t)pv->type.ref)
			return 1;
	}
	return 0;
}

static AstLocal rir_thold[16];
static int rir_tholdn;

static AstLocal rir_last_return = AST_NONE;
static AstLocal rir_retexpr = AST_NONE;
static int rir_retexpr_depth;
static int rir_retexpr_pending;

/* A call whose value is discarded becomes a statement only when the shadow
   stack drops it, and inside a statement expression the parser's vpop for it
   lands AFTER the following goto's and label's marks -- so the arena reads
   Jump, Jump, Invoke where the parser emitted the call first. A goto or a
   label is a point no value crosses in the emitted stream, so an unparented
   effectful top here has already been emitted and belongs before the jump.
   The entry is blanked rather than left in place: rir_drop re-stmts any
   effectful node it pops without checking whether it already has a parent,
   and a node added to one BasicBlock twice reads as "nchild disagrees with
   sibling chain". Blanking keeps the depth and drops the second add. */
static void rir_flush_effect_top(void) {
	AstLocal t;
	if (rir_shn <= 0 || rir_call_depth || rir_cond_depth || rir_lorn ||
			rir_ternn || rir_iholdn)
		return;
	t = rir_sh[rir_shn - 1];
	if (t == AST_NONE || ast_parent(rir_arena, t) != AST_NONE ||
			ast_kind(rir_arena, t) != AST_Invoke)
		return;
	rir_stmt(t);
	rir_sh[rir_shn - 1] = AST_NONE;
	rir_shtype[rir_shn - 1] = 0;
}

static void rir_mark_apply(const RirOp *ro) {
	AstLocal a, n;
	switch (ro->rkind) {
	case RIR_M_RETEXPR:
		if (rir_shn > 0) {
			rir_retexpr = rir_sh[rir_shn - 1];
			rir_retexpr_depth = rir_shn;
			rir_retexpr_pending = 1;
		}
		break;
	case RIR_M_WHILECOND:
		rir_while_pfx = AST_NONE;
		if (rir_bbn && rir_bbn < 64) {
			AstLocal pfx = ast_node(rir_arena, AST_BasicBlock);
			rir_while_pfx = pfx;
			rir_bb[rir_bbn++] = pfx;
		}
		break;
	case RIR_M_RETURN:
		rir_ret_spilled = 0;
		n = ast_node(rir_arena, AST_Return);
		if (rir_retexpr_pending && ro->rval && rir_retexpr != AST_NONE) {
			rir_shn = rir_retexpr_depth - 1;
			a = rir_retexpr;
		} else {
			a = rir_shn ? rir_pop() : AST_NONE;
		}
		rir_retexpr_pending = 0;
		rir_retexpr = AST_NONE;
		if (a != AST_NONE)
			ast_add_child(rir_arena, n, a);
		if (rir_bbn == 1) {
			if (rir_pending_ret != AST_NONE)
				rir_stmt(rir_pending_ret);
			rir_pending_ret = n;
		} else {
			rir_stmt(n);
		}
		rir_last_return = n;
		rir_after_ret = 1;
		break;
	case RIR_M_IRETURN: {
		AstLocal lit;
		n = ast_node(rir_arena, AST_Return);
		lit = ast_node(rir_arena, AST_Literal);
		ast_set_op(rir_arena, lit, VT_CONST);
		ast_set_ival(rir_arena, lit, 0);
		ast_set_type(rir_arena, lit, VT_INT, 0);
		ast_add_child(rir_arena, n, lit);
		rir_stmt(n);
		rir_last_return = n;
		rir_after_ret = 1;
		break;
	}
	case RIR_M_CMPINV:
		if (rir_shn > 0) {
			AstLocal top = rir_sh[rir_shn - 1];
			if (top != AST_NONE && ast_kind(rir_arena, top) == AST_Binary) {
				int bop = ast_op(rir_arena, top);
				int inflags = ro->mvs_n > 0 &&
						(rir_mvs[ro->mvs_off + ro->mvs_n - 1].r & VT_VALMASK) == VT_CMP;
				if (bop == TOK_LAND || bop == TOK_LOR)
					ast_set_fbits(rir_arena, top,
												ast_fbits(rir_arena, top) ^ AST_FB_LANDOR_INVERT);
				else if (inflags || ast_cmp_invert_late(rir_arena, top, bop))
					ast_set_fbits(rir_arena, top,
												ast_fbits(rir_arena, top) ^ AST_FB_CMP_INVERT_LATE);
				else
					ast_set_op(rir_arena, top, bop ^ 1);
			}
		}
		break;
	case RIR_M_OPASSIGN:
		rir_opassign_pending = 1;
		break;
	case RIR_M_RETJMP:
		if (rir_last_return != AST_NONE)
			ast_set_op(rir_arena, rir_last_return, ro->rval ? 1 : 0);
		rir_last_return = AST_NONE;
		rir_after_ret = 0;
		break;
	case RIR_M_JUMP:
		n = ast_node(rir_arena, AST_Jump);
		ast_set_op(rir_arena, n, ro->rval ? 1 : 0);
		rir_stmt(n);
		break;
	case RIR_M_GOTO:
		rir_flush_effect_top();
		n = ast_node(rir_arena, AST_Jump);
		ast_set_op(rir_arena, n, 5);
		ast_set_ival(rir_arena, n, (uint64_t)(unsigned)ro->rval);
		rir_stmt(n);
		if (rir_clg_pending) {
			rir_clg_bind(rir_clg_pending, n);
			rir_clg_pending = NULL;
		}
		break;
	case RIR_M_CLGOTO:
		rir_clg_pending = (void *)(uintptr_t)ro->rv1;
		break;
	case RIR_M_CLTHUNK: {
		AstLocal g = rir_clg_get((void *)(uintptr_t)ro->rv1);
		int sid = --rir_clg_syn;
		if (g != AST_NONE)
			ast_set_ival(rir_arena, g, (uint64_t)(unsigned)sid);
		n = ast_node(rir_arena, AST_Jump);
		ast_set_op(rir_arena, n, 4);
		ast_set_ival(rir_arena, n, (uint64_t)(unsigned)sid);
		rir_stmt(n);
		break;
	}
	case RIR_M_CLJMP:
		n = ast_node(rir_arena, AST_Jump);
		ast_set_op(rir_arena, n, 5);
		ast_set_ival(rir_arena, n, (uint64_t)(unsigned)(int)ro->rv2);
		rir_stmt(n);
		rir_clg_bind((void *)(uintptr_t)ro->rv1, n);
		break;
	case RIR_M_CASE:
		rir_after_ret = 0;
		n = ast_node(rir_arena, AST_Jump);
		ast_set_op(rir_arena, n, 2);
		ast_set_ival(rir_arena, n, (uint64_t)ro->rv1);
		ast_set_fbits(rir_arena, n, (uint64_t)ro->rv2);
		rir_stmt(n);
		break;
	case RIR_M_DEFAULT:
		rir_after_ret = 0;
		n = ast_node(rir_arena, AST_Jump);
		ast_set_op(rir_arena, n, 3);
		rir_stmt(n);
		break;
	case RIR_M_LABEL:
		rir_after_ret = 0;
		rir_flush_effect_top();
		n = ast_node(rir_arena, AST_Jump);
		ast_set_op(rir_arena, n, 4);
		ast_set_ival(rir_arena, n, (uint64_t)(unsigned)ro->rval);
		rir_stmt(n);
		break;
	case RIR_M_BFGV:
		if (rir_shn > 0) {
			AstLocal top = rir_sh[rir_shn - 1];
			if (top != AST_NONE && (ast_type_t(rir_arena, top) & VT_BITFIELD) &&
					ast_parent(rir_arena, top) == AST_NONE) {
				AstLocal cv = ast_node(rir_arena, AST_Convert);
				ast_set_type(rir_arena, cv, ro->rval, 0);
				ast_add_child(rir_arena, cv, top);
				rir_sh[rir_shn - 1] = cv;
				rir_shtype[rir_shn - 1] = 0;
			} else if (top != AST_NONE &&
								 ast_kind(rir_arena, top) == AST_StoreVal) {
				AstLocal st = (AstLocal)ast_ival(rir_arena, top);
				AstLocal tgt = st == AST_NONE ? AST_NONE
																			: ast_child(rir_arena, st, 0);
				if (st != AST_NONE && ast_kind(rir_arena, st) == AST_Store &&
						tgt != AST_NONE && (ast_type_t(rir_arena, tgt) & VT_BITFIELD))
					ast_set_fbits(rir_arena, st,
												ast_fbits(rir_arena, st) | AST_FB_STORE_BF_GV);
			}
		}
		break;
	case RIR_M_LOAD:
		if (rir_after_ret && rir_shn == 0)
			break;
		if (rir_shn > 0 && ro->mvs_n - ast_base_depth > 0) {
			const SValue *pv = &rir_mvs[ro->mvs_off + ro->mvs_n - 1];
			AstLocal top = rir_sh[rir_shn - 1];
			if (top != AST_NONE && ast_type_t(rir_arena, top) == 0 &&
					ast_kind(rir_arena, top) == AST_Binary &&
					(pv->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR &&
					!rir_ptr_arith(top, pv)) {
				AstLocal cv = ast_node(rir_arena, AST_Convert);
				ast_set_type(rir_arena, cv, pv->type.t,
										 (uint64_t)(uintptr_t)pv->type.ref);
				ast_add_child(rir_arena, cv, top);
				rir_sh[rir_shn - 1] = cv;
				rir_shtype[rir_shn - 1] = 0;
			}
			else if (top != AST_NONE && ast_type_t(rir_arena, top) != 0 &&
							 (ast_type_t(rir_arena, top) & (VT_BTYPE | VT_ARRAY)) != VT_PTR &&
							 !is_float(ast_type_t(rir_arena, top)) &&
							 (ast_type_t(rir_arena, top) & VT_BTYPE) != VT_STRUCT &&
							 (pv->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR) {
				AstLocal cv = ast_node(rir_arena, AST_Convert);
				ast_set_type(rir_arena, cv, pv->type.t,
										 (uint64_t)(uintptr_t)pv->type.ref);
				ast_add_child(rir_arena, cv, top);
				rir_sh[rir_shn - 1] = cv;
				rir_shtype[rir_shn - 1] = 0;
			}
			else if (top != AST_NONE && ast_kind(rir_arena, top) == AST_Convert &&
							 (ast_type_t(rir_arena, top) & (VT_BTYPE | VT_ARRAY)) == VT_PTR &&
							 (pv->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR &&
							 ast_type_ref(rir_arena, top) !=
									 (uint64_t)(uintptr_t)pv->type.ref) {
				const Sym *ps = (const Sym *)(uintptr_t)ast_type_ref(rir_arena, top);
				if (ps && (ps->type.t & VT_BTYPE) == VT_VOID)
					ast_set_type(rir_arena, top, pv->type.t,
											 (uint64_t)(uintptr_t)pv->type.ref);
			}
			else if (top != AST_NONE &&
							 (ast_type_t(rir_arena, top) & (VT_BTYPE | VT_ARRAY)) == VT_PTR &&
							 (pv->type.t & (VT_BTYPE | VT_ARRAY)) == VT_PTR &&
							 ast_type_ref(rir_arena, top) !=
									 (uint64_t)(uintptr_t)pv->type.ref) {
				const Sym *ps = (const Sym *)(uintptr_t)ast_type_ref(rir_arena, top);
				if (ps && (ps->type.t & VT_BTYPE) == VT_VOID) {
					AstLocal cv = ast_node(rir_arena, AST_Convert);
					ast_set_type(rir_arena, cv, pv->type.t,
											 (uint64_t)(uintptr_t)pv->type.ref);
					if (rir_castgv_pend && rir_castgv_t == pv->type.t &&
							rir_castgv_ref == (uint64_t)(uintptr_t)pv->type.ref) {
						ast_set_fbits(rir_arena, cv, AST_FB_CONVERT_GV);
						rir_castgv_pend = 0;
					}
					ast_add_child(rir_arena, cv, top);
					rir_sh[rir_shn - 1] = cv;
					rir_shtype[rir_shn - 1] = 0;
				}
			}
		}
		a = rir_pop();
		if (a == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		n = ast_node(rir_arena, AST_Load);
		ast_add_child(rir_arena, n, a);
		rir_push(n);
		break;
	case RIR_M_NORETURN:
		{
			AstLocal inv = rir_pending_call;
			if (inv == AST_NONE && rir_shn > 0 &&
					ast_kind(rir_arena, rir_sh[rir_shn - 1]) == AST_Invoke)
				inv = rir_sh[rir_shn - 1];
			if (inv != AST_NONE)
				ast_set_fbits(rir_arena, inv,
											ast_fbits(rir_arena, inv) | AST_FB_CALL_NORETURN);
		}
		break;
	case RIR_M_VLA: {
		AstLocal u = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, u, AST_OP_VLA);
		ast_set_type(rir_arena, u, (int)(ro->rv1 & 0xffffffff), (uint64_t)ro->rv2);
		ast_set_ival(rir_arena, u,
								 ((uint64_t)(uint32_t)(int)(ro->rv3 & 0xffffffff)) |
										 (((uint64_t)(uint32_t)(int)((ro->rv1 >> 32) & 0xffffffff))
											<< 32));
		ast_set_sym(rir_arena, u,
								(uint64_t)(int64_t)(int)((ro->rv3 >> 32) & 0xffffffff));
		ast_set_fbits(rir_arena, u, (uint64_t)(unsigned)(ro->rval & 1));
		ast_set_wide(rir_arena, u, (uint64_t)(unsigned)(ro->rval >> 1),
								 AST_R2_NONE);
		rir_stmt(u);
		break;
	}
	case RIR_M_VLARESTORE: {
		AstLocal u;
		if (!ro->rval)
			break;
		if (rir_pending_ret != AST_NONE) {
			ast_set_ival(rir_arena, rir_pending_ret, (uint64_t)(int64_t)ro->rval);
			break;
		}
		if (rir_last_return != AST_NONE) {
			ast_set_ival(rir_arena, rir_last_return, (uint64_t)(int64_t)ro->rval);
			break;
		}
		u = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, u, AST_OP_VLA_RESTORE);
		ast_set_ival(rir_arena, u, (uint64_t)(int64_t)ro->rval);
		rir_stmt(u);
		break;
	}
	case RIR_M_ARGCAST:
		if (rir_argcast_n < RIR_ARGCAST_MAX)
			rir_argcast_ch[rir_argcast_n] = ro->rval ? 1 : 0;
		rir_argcast_n++;
		break;
	case RIR_M_CASTGV:
		if (rir_shn > 0) {
			AstLocal top = rir_sh[rir_shn - 1];
			if (top != AST_NONE && ast_kind(rir_arena, top) == AST_Convert)
				ast_set_fbits(rir_arena, top,
											ast_fbits(rir_arena, top) | AST_FB_CONVERT_GV);
			else if (ro->mvs_n - ast_base_depth > 0) {
				const SValue *pv = &rir_mvs[ro->mvs_off + ro->mvs_n - 1];
				if (top != AST_NONE && !rir_shtype[rir_shn - 1] &&
						ast_parent(rir_arena, top) == AST_NONE &&
						(ast_kind(rir_arena, top) == AST_Ref ||
						 ast_kind(rir_arena, top) == AST_Literal) &&
						ast_type_t(rir_arena, top) == (int)pv->type.t &&
						ast_type_ref(rir_arena, top) == (uint64_t)(uintptr_t)pv->type.ref) {
					AstLocal cv = ast_node(rir_arena, AST_Convert);
					ast_set_type(rir_arena, cv, pv->type.t,
											 (uint64_t)(uintptr_t)pv->type.ref);
					ast_set_fbits(rir_arena, cv, AST_FB_CONVERT_GV);
					ast_add_child(rir_arena, cv, top);
					rir_sh[rir_shn - 1] = cv;
					break;
				}
				rir_castgv_pend = 2;
				rir_castgv_top = top;
				rir_castgv_t = pv->type.t;
				rir_castgv_ref = (uint64_t)(uintptr_t)pv->type.ref;
			}
		}
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
	case RIR_M_TERNHOLD:
		if (rir_tholdn < (int)(sizeof rir_thold / sizeof rir_thold[0]))
			rir_thold[rir_tholdn++] = rir_pop();
		break;
	case RIR_M_TERNPICK:
		if (rir_tholdn > 0) {
			AstLocal keep = rir_thold[--rir_tholdn];
			AstLocal drop = rir_pop();
			(void)drop;
			if (keep != AST_NONE && ast_parent(rir_arena, keep) == AST_NONE)
				rir_push(keep);
			else
				rir_push(drop);
		}
		break;
	case RIR_M_ADDRLATE:
		rir_addr_late = 1;
		break;
#if MCC_CONFIG_ASM
	case RIR_M_ASMOPS: {
		/* asm_instr has parsed every operand and is about to save_regs(0); the
		   top nb_operands shadow entries ARE those operands.  Taking them here,
		   before the spills, both gives replay something to emit for them and
		   stops the spill model below from consuming them one at a time -- the
		   ASMOPS replay calls save_regs itself. */
		int nb = (int)ro->rv1, q;
		/* The shadow stack is only synced at ops that carry a vstack snapshot,
		   and the last operand's push may still be outstanding here -- reconcile
		   against the mark's own snapshot before counting. */
		rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
		if (nb <= 0 || nb > rir_shn)
			break;
		/* An operand the shadow stack could not reconstruct leaves the body no
		   worse off under the old spill model than it would be under a partial
		   ASMOPS, so decline rather than mismatch the whole body. */
		for (q = rir_shn - nb; q < rir_shn; q++) {
			a = rir_sh[q];
			if (a == AST_NONE || ast_parent(rir_arena, a) != AST_NONE)
				return;
		}
		n = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, n, AST_OP_ASMOPS);
		ast_set_ival(rir_arena, n, (uint64_t)ro->rv2);
		for (q = rir_shn - nb; q < rir_shn; q++)
			ast_add_child(rir_arena, n, rir_sh[q]);
		/* Leave the entries in place: the parser's vstack still holds them
		   through save_regs and both asm_gen_code halves, and a shorter shadow
		   stack reads as a depth mismatch at the next op.  Parenting them is
		   what takes them out of play -- every consumer declines a node that
		   already has a parent, the spill model included. */
		rir_stmt(n);
		break;
	}
#endif
	default:
		break;
	}
}

static int rir_cf_op(int rkind) {
	switch (rkind) {
	case RIR_R_WHILE:
		return 2;
	case RIR_R_FOR:
		return 3;
	case RIR_R_DO:
		return 4;
	case RIR_R_SWITCH:
		return 6;
	default:
		return 0;
	}
}

static void rir_cf_cond(void) {
	AstLocal cond;
	if (!rir_cfn || rir_cfcond[rir_cfn - 1])
		return;
	cond = rir_shn ? rir_pop() : AST_NONE;
	if (cond == AST_NONE)
		return;
	if (rir_dheldn && rir_docond && rir_cfkind[rir_cfn - 1] == RIR_R_DO) {
		AstLocal bb = ast_node(rir_arena, AST_BasicBlock);
		int q;
		for (q = 0; q < rir_dheldn; q++)
			ast_add_child(rir_arena, bb, rir_dheld[q]);
		ast_add_child(rir_arena, bb, cond);
		rir_dheldn = 0;
		cond = bb;
	}
	rir_docond = 0;
	ast_add_child(rir_arena, rir_cf[rir_cfn - 1], cond);
	rir_cfcond[rir_cfn - 1] = 1;
}

static void rir_region(const RirOp *ro) {
	int after_ret = rir_after_ret;
	rir_after_ret = 0;
	if (ro->tag == RIR_T_RBEGIN) {
		switch (ro->rkind) {
		case RIR_R_IF:
		case RIR_R_WHILE:
		case RIR_R_DO:
		case RIR_R_FOR:
		case RIR_R_SWITCH: {
			AstLocal n;
			AstLocal pfx = AST_NONE;
			if (ro->rkind == RIR_R_WHILE && rir_while_pfx != AST_NONE) {
				pfx = rir_while_pfx;
				rir_while_pfx = AST_NONE;
				if (rir_bbn > 1 && rir_bb[rir_bbn - 1] == pfx)
					rir_bbn--;
				if (ast_first_child(rir_arena, pfx) == AST_NONE)
					pfx = AST_NONE;
			}
			n = ast_node(rir_arena, AST_If);
			ast_set_op(rir_arena, n, rir_cf_op(ro->rkind));
			rir_stmt(n);
			if (rir_cfn < 64) {
				rir_cf[rir_cfn] = n;
				rir_cfkind[rir_cfn] = ro->rkind;
				rir_cfind[rir_cfn] = ro->rind;
				rir_cfcond[rir_cfn] = 0;
				rir_cfpfx[rir_cfn] = pfx;
				rir_cfn++;
			}
			if (ro->rkind != RIR_R_FOR && ro->rkind != RIR_R_DO)
				rir_cf_cond();
			break;
		}
		case RIR_R_COND:
			rir_cond_depth++;
			if (rir_cfn && (rir_cfkind[rir_cfn - 1] == RIR_R_FOR ||
											rir_cfkind[rir_cfn - 1] == RIR_R_DO) &&
					!(rir_ternn && rir_tern_cf[rir_ternn - 1] == rir_cfn)) {
				rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
				rir_cf_cond();
			}
			break;
		case RIR_R_SYNTH:
			rir_synth_depth++;
			break;
		case RIR_R_CALL:
			rir_call_depth++;
			break;
		case RIR_R_INC: {
			AstLocal a = rir_pop(), u;
			rir_inc_depth++;
			if (a == AST_NONE) {
				rir_arena_mismatch++;
				break;
			}
			u = ast_node(rir_arena, AST_Unary);
			ast_set_op(rir_arena, u, ro->rval >> 1);
			ast_set_ival(rir_arena, u, (uint64_t)(ro->rval & 1));
			ast_add_child(rir_arena, u, a);
			rir_push(u);
			break;
		}
		case RIR_R_MEMBER:
			rir_member_depth++;
			break;
		case RIR_R_CVT: {
			int act = 0;
			if (!ro->rinop && !rir_cvt_depth && !rir_cond_depth && !rir_inc_depth &&
					!rir_member_depth && !rir_retexpr_pending && !rir_vstruct_depth &&
					!rir_vla_depth && rir_cvt_n < RIR_CVT_MAX && rir_shn > 0 &&
					rir_sh[rir_shn - 1] != AST_NONE &&
					ast_parent(rir_arena, rir_sh[rir_shn - 1]) == AST_NONE) {
				AstLocal a = rir_pop();
				AstLocal cv = ast_node(rir_arena, AST_Convert);
				ast_set_type(rir_arena, cv, ro->rval, 0);
				ast_add_child(rir_arena, cv, a);
				rir_push(cv);
				act = 1;
			}
			if (rir_cvt_n < RIR_CVT_MAX)
				rir_cvt_on[rir_cvt_n] = (unsigned char)act;
			rir_cvt_n++;
			if (act)
				rir_cvt_depth++;
			break;
		}
		case RIR_R_VLA:
			rir_vla_depth++;
			break;
		case RIR_R_LSUP:
			rir_cond_depth++;
			break;
		case RIR_R_VSTORE:
			if (rir_vstn < 16) {
				int n2 = ro->mvs_n - ast_base_depth;
				int allow = 0, fit;
				rir_vst_tc[rir_vstn] = 0;
				rir_vst_vc[rir_vstn] = 0;
				if (n2 == 2 && rir_shn == 1 && !after_ret &&
						rir_pending_ret == AST_NONE && rir_pending_call == AST_NONE &&
						!rir_cond_depth && !rir_synth_depth && !rir_call_depth &&
						!rir_inc_depth && !rir_member_depth && !rir_vstruct_depth &&
						!rir_vbf_depth && !rir_vla_depth && !rir_retexpr_pending &&
						(rir_mvs[ro->mvs_off + ro->mvs_n - 1].type.t & VT_BTYPE) ==
								VT_STRUCT &&
						(rir_mvs[ro->mvs_off + ro->mvs_n - 2].type.t & VT_BTYPE) ==
								VT_STRUCT)
					rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
				fit = (n2 >= 2 && !rir_call_depth && !rir_vstruct_depth &&
							 !rir_cx_depth);
				if (fit &&
						(rir_mvs[ro->mvs_off + ro->mvs_n - 1].type.t & VT_BTYPE) ==
								VT_STRUCT &&
						(rir_mvs[ro->mvs_off + ro->mvs_n - 2].type.t & VT_BTYPE) ==
								VT_STRUCT) {
					rir_stamp_call_top(rir_mvs + ro->mvs_off, ro->mvs_n);
					rir_vstruct_depth++;
					rir_vst_sup[rir_vstn] = 1;
					rir_vst_tc[rir_vstn] =
							(long long)rir_mvs[ro->mvs_off + ro->mvs_n - 2].c.i;
					rir_vst_vc[rir_vstn] =
							(long long)rir_mvs[ro->mvs_off + ro->mvs_n - 1].c.i;
				} else {
					rir_vst_sup[rir_vstn] = 0;
				}
				rir_vst_bf[rir_vstn] = 0;
				if (n2 == 2 && rir_shn >= 1 && !rir_vst_sup[rir_vstn] &&
						!rir_cond_depth && !rir_synth_depth && !rir_call_depth &&
						!rir_inc_depth && !rir_member_depth && !rir_vstruct_depth &&
						!rir_vbf_depth && !rir_vla_depth && !rir_retexpr_pending &&
						rir_pending_ret == AST_NONE) {
					const SValue *v = &rir_mvs[ro->mvs_off + ro->mvs_n - 1];
					const SValue *t = &rir_mvs[ro->mvs_off + ro->mvs_n - 2];
					if ((t->type.t & VT_BITFIELD) &&
							(t->type.t & VT_BTYPE) != VT_STRUCT &&
							!((v->type.t & VT_BTYPE) == VT_STRUCT ||
								(v->type.t & VT_ARRAY) || (v->type.t & VT_BITFIELD))) {
						rir_vst_bf[rir_vstn] = 1;
						rir_vbf_depth++;
						rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
					}
				}
				rir_vst_cx[rir_vstn] = 0;
				if (n2 == 2 && !rir_vst_sup[rir_vstn] && !rir_vst_bf[rir_vstn] &&
						!after_ret && rir_pending_ret == AST_NONE &&
						rir_pending_call == AST_NONE && !rir_cond_depth &&
						!rir_synth_depth && !rir_call_depth && !rir_inc_depth &&
						!rir_member_depth && !rir_vstruct_depth && !rir_vbf_depth &&
						!rir_cx_depth && !rir_vla_depth && !rir_retexpr_pending) {
					SValue *v = &rir_mvs[ro->mvs_off + ro->mvs_n - 1];
					SValue *t = &rir_mvs[ro->mvs_off + ro->mvs_n - 2];
					int ct = is_complex_type(&t->type), cv = is_complex_type(&v->type);
					if (ct != cv &&
							((ct ? v : t)->type.t & VT_BTYPE) != VT_STRUCT &&
							!((v->type.t | t->type.t) & (VT_ARRAY | VT_BITFIELD))) {
						rir_vst_cx[rir_vstn] = 1;
						rir_cx_depth++;
						rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
					}
				}
				if (n2 >= 2) {
					const SValue *v = &rir_mvs[ro->mvs_off + ro->mvs_n - 1];
					const SValue *t = &rir_mvs[ro->mvs_off + ro->mvs_n - 2];
					allow = (((v->type.t & VT_ARRAY) != 0 &&
										(v->type.t & VT_BTYPE) != VT_STRUCT &&
										(t->type.t & VT_BTYPE) != VT_STRUCT) ||
										(fit && (v->type.t & VT_BTYPE) == VT_STRUCT &&
											(t->type.t & VT_BTYPE) == VT_STRUCT)) &&
									!((v->type.t | t->type.t) & VT_BITFIELD);
				}
				if (rir_vst_bf[rir_vstn] || rir_vst_cx[rir_vstn])
					allow = 1;
				rir_vst_nc[rir_vstn] = (unsigned char)(ro->rnocode != 0);
				rir_vst_gret[rir_vstn] = (unsigned char)(after_ret != 0);
				if (after_ret)
					rir_gret_depth++;
				rir_vst_ok[rir_vstn] = (unsigned char)allow;
				rir_vst_shn[rir_vstn] = (short)rir_shn;
				rir_vst_seen[rir_vstn++] = 0;
				if (rir_vst_sup[rir_vstn - 1] || rir_vst_cx[rir_vstn - 1]) {
					rir_vsup_depth = 1;
					rir_vsup_nest = 0;
				}
			}
			break;
		case RIR_R_LANDOR: {
			AstLocal n = ast_node(rir_arena, AST_Binary);
			ast_set_op(rir_arena, n, ro->rval);
			if (rir_lorn < 16)
				rir_lor[rir_lorn++] = n;
			break;
		}
		case RIR_R_TERNARY: {
			AstLocal n = ast_node(rir_arena, AST_If);
			AstLocal cond = rir_shn ? rir_pop() : AST_NONE;
			ast_set_op(rir_arena, n, ro->rval ? 9 : 5);
			if (cond != AST_NONE)
				ast_add_child(rir_arena, n, cond);
			else
				rir_arena_mismatch++;
			if (rir_ternn < 16) {
				rir_tern_cf[rir_ternn] = rir_cfn;
				rir_tern[rir_ternn++] = n;
			}
			break;
		}
		case RIR_R_INCR:
		case RIR_R_BODY:
		case RIR_R_THEN:
		case RIR_R_ELSE: {
			AstLocal bb = ast_node(rir_arena, AST_BasicBlock);
			if (rir_cfn)
				ast_add_child(rir_arena, rir_cf[rir_cfn - 1], bb);
			if (rir_bbn < 64)
				rir_bb[rir_bbn++] = bb;
			if (ro->rkind == RIR_R_INCR) {
				rir_incr_bb = bb;
				rir_incr_live = ro->rval;
			}
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
	case RIR_R_CVT:
		if (rir_cvt_n > 0) {
			rir_cvt_n--;
			if (rir_cvt_n < RIR_CVT_MAX && rir_cvt_on[rir_cvt_n] && rir_cvt_depth)
				rir_cvt_depth--;
		}
		break;
	case RIR_R_CALL:
		if (rir_call_depth)
			rir_call_depth--;
		if (ro->rval && !rir_call_depth) {
			AstLocal inv = rir_pending_call;
			if (inv == AST_NONE && rir_shn > 0 &&
					ast_kind(rir_arena, rir_val_node(rir_sh[rir_shn - 1])) == AST_Invoke)
				inv = rir_pop();
			else if (inv != AST_NONE)
				rir_pending_call = AST_NONE;
			if (inv != AST_NONE) {
				ast_set_type(rir_arena, rir_val_node(inv), VT_VOID, 0);
				rir_ihold_arm = 1;
				rir_stmt(inv);
				rir_ihold_arm = 0;
			}
		}
		break;
	case RIR_R_INC:
		if (rir_inc_depth)
			rir_inc_depth--;
		break;
	case RIR_R_LSUP:
		if (rir_cond_depth)
			rir_cond_depth--;
		break;
	case RIR_R_VSTORE:
		if (!rir_vstn && rir_vstruct_depth)
			rir_vstruct_depth--;
		if (rir_vstn) {
			int seen = rir_vst_seen[--rir_vstn];
			int allow = rir_vst_ok[rir_vstn];
			if (rir_vst_sup[rir_vstn] || rir_vst_cx[rir_vstn])
				rir_vsup_depth = 0;
			if (rir_vst_sup[rir_vstn] && rir_vstruct_depth)
				rir_vstruct_depth--;
			if (rir_vst_bf[rir_vstn] && rir_vbf_depth)
				rir_vbf_depth--;
			if (rir_vst_cx[rir_vstn] && rir_cx_depth)
				rir_cx_depth--;
			if (rir_vst_gret[rir_vstn] && rir_gret_depth)
				rir_gret_depth--;
			if ((rir_pending_ret != AST_NONE || rir_vst_gret[rir_vstn]) &&
					(rir_ret_spilled || rir_shn < 2 ||
					 (ast_kind(rir_arena, rir_sh[rir_shn - 1]) == AST_Unary &&
						ast_op(rir_arena, rir_sh[rir_shn - 1]) == AST_OP_ADDR) ||
					 (ast_kind(rir_arena, rir_sh[rir_shn - 2]) == AST_Unary &&
						ast_op(rir_arena, rir_sh[rir_shn - 2]) == AST_OP_ADDR))) {
				if (rir_vst_shn[rir_vstn] >= 0 && rir_shn > rir_vst_shn[rir_vstn])
					rir_shn = rir_vst_shn[rir_vstn];
				break;
			}
			if (rir_vst_shn[rir_vstn] >= 2 && rir_shn > rir_vst_shn[rir_vstn])
				rir_shn = rir_vst_shn[rir_vstn];
			if (!seen && allow && rir_shn >= 2) {
				AstLocal t = rir_pop(), v = rir_pop(), n;
				if (rir_vst_bf[rir_vstn] || rir_vst_cx[rir_vstn] ||
						rir_vst_nc[rir_vstn]) {
					AstLocal sw = t;
					t = v;
					v = sw;
				} else if (rir_vst_sup[rir_vstn]) {
					long long tc = rir_vst_tc[rir_vstn], vc = rir_vst_vc[rir_vstn];
					long long ti = (long long)ast_ival(rir_arena, t);
					long long vi = (long long)ast_ival(rir_arena, v);
					if (!(tc != vc && ti == tc && vi == vc)) {
						AstLocal sw = t;
						t = v;
						v = sw;
					}
				}
				n = ast_node(rir_arena, AST_Store);
				ast_add_child(rir_arena, n, t);
				ast_add_child(rir_arena, n, v);
				rir_stmt(n);
				rir_ret_follow_spill(t, v);
				{
					AstLocal mv = ast_node(rir_arena, AST_StoreVal);
					ast_set_type(rir_arena, mv, ast_type_t(rir_arena, v),
											 ast_type_ref(rir_arena, v));
					ast_set_ival(rir_arena, mv, (uint64_t)n);
					rir_push(mv);
				}
			}
		}
		break;
	case RIR_R_LOPND: {
		AstLocal v;
		if (!rir_lorn)
			break;
		rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
		v = rir_shn ? rir_pop() : AST_NONE;
		if (v == AST_NONE) {
			rir_arena_mismatch++;
			rir_lheldn = 0;
			break;
		}
		if (rir_lheldn) {
			AstLocal bb = ast_node(rir_arena, AST_BasicBlock);
			int q;
			for (q = 0; q < rir_lheldn; q++)
				ast_add_child(rir_arena, bb, rir_lheld[q]);
			ast_add_child(rir_arena, bb, v);
			rir_lheldn = 0;
			v = bb;
		}
		ast_add_child(rir_arena, rir_lor[rir_lorn - 1], v);
		break;
	}
	case RIR_R_LANDOR:
		rir_lheldn = 0;
		if (rir_lorn) {
			AstLocal n = rir_lor[--rir_lorn];
			if ((ro->rval & 1) && ast_nchild(rir_arena, n) >= 1) {
				ast_set_fbits(rir_arena, n,
											ast_fbits(rir_arena, n) | AST_FB_LANDOR_MATERIAL);
				ast_set_ival(rir_arena, n, (uint64_t)((ro->rval >> 2) & 1));
				if (rir_shn &&
						ast_kind(rir_arena, rir_sh[rir_shn - 1]) == AST_Literal)
					rir_pop();
				rir_push(n);
			} else if ((ro->rval & 1) ||
					ast_nchild(rir_arena, n) < ((ro->rval & 2) ? 1 : 2))
				rir_arena_mismatch++;
			else
				rir_push(n);
		}
		break;
	case RIR_R_TARM: {
		AstLocal v;
		if (!rir_ternn)
			break;
		rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
		v = rir_shn ? rir_pop() : AST_NONE;
		if (v == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		ast_add_child(rir_arena, rir_tern[rir_ternn - 1], v);
		break;
	}
	case RIR_R_TERNARY:
		if (rir_ternn) {
			AstLocal n = rir_tern[--rir_ternn];
			if (ast_nchild(rir_arena, n) !=
					(ast_op(rir_arena, n) == 9 ? 2u : 3u))
				rir_arena_mismatch++;
			rir_push(n);
		}
		break;
	case RIR_R_VLA:
		if (rir_vla_depth)
			rir_vla_depth--;
		break;
	case RIR_R_MEMBER: {
		AstLocal base, m;
		if (rir_member_depth)
			rir_member_depth--;
		base = rir_pop();
		if (base == AST_NONE) {
			rir_arena_mismatch++;
			break;
		}
		m = ast_node(rir_arena, AST_Unary);
		ast_set_op(rir_arena, m,
							 (ro->rval & 2) ? AST_OP_MEMBER_ARROW : AST_OP_MEMBER);
		ast_set_ival(rir_arena, m, (uint64_t)(unsigned)(ro->rval >> 2));
		ast_set_fbits(rir_arena, m, (ro->rval & 1) ? (uint64_t)VT_NONLVAL : 0);
		ast_add_child(rir_arena, m, base);
		if (ro->mvs_n - ast_base_depth > 0) {
			const SValue *v = &rir_mvs[ro->mvs_off + ro->mvs_n - 1];
			ast_set_type(rir_arena, m, v->type.t,
									 (uint64_t)(uintptr_t)v->type.ref);
			rir_push(m);
		} else {
			rir_push_typed(m);
		}
		break;
	}
	case RIR_R_INCR:
		if (rir_incr_live && rir_cfn && rir_incr_bb != AST_NONE &&
				ast_first_child(rir_arena, rir_incr_bb) == AST_NONE)
			ast_set_fbits(rir_arena, rir_cf[rir_cfn - 1],
										ast_fbits(rir_arena, rir_cf[rir_cfn - 1]) |
												AST_FB_FOR_INCR_LIVE);
		rir_incr_live = 0;
		rir_incr_bb = AST_NONE;
		if (rir_bbn > 1)
			rir_bbn--;
		break;
	case RIR_R_BODY:
	case RIR_R_THEN:
	case RIR_R_ELSE:
		if (rir_bbn > 1)
			rir_bbn--;
		if (ro->rkind == RIR_R_BODY && rir_cfn &&
				rir_cfkind[rir_cfn - 1] == RIR_R_DO && !rir_cfcond[rir_cfn - 1])
			rir_docond = 1;
		break;
	case RIR_R_IF:
	case RIR_R_WHILE:
	case RIR_R_DO:
	case RIR_R_FOR:
	case RIR_R_SWITCH:
		rir_docond = 0;
		rir_dheld_flush();
		if (rir_cfn) {
			rir_cfn--;
			/* The parser advanced ind by nothing across this whole construct, so
			   it was statically dead. Its ops are already dropped by
			   rir_op_effect's nocode test, but the construct's own branch -- a
			   switch's dispatch jump, a loop's back edge -- comes from replaying
			   the node, not from an op. Being dead at the RBEGIN alone does not
			   say this: a case label inside turns code back on, and gjmp_acs
			   leaves CODE_OFF set after every break. Zero bytes emitted does. */
			if (ro->rind == rir_cfind[rir_cfn])
				ast_set_fbits(rir_arena, rir_cf[rir_cfn],
											ast_fbits(rir_arena, rir_cf[rir_cfn]) | AST_FB_NOCODE);
			if (rir_cfkind[rir_cfn] == RIR_R_FOR && !rir_cfcond[rir_cfn])
				ast_set_op(rir_arena, rir_cf[rir_cfn], 8);
			if (rir_cfpfx[rir_cfn] != AST_NONE &&
					ast_nchild(rir_arena, rir_cf[rir_cfn]) == 2)
				ast_add_child(rir_arena, rir_cf[rir_cfn], rir_cfpfx[rir_cfn]);
			rir_cfpfx[rir_cfn] = AST_NONE;
		}
		break;
	default:
		break;
	}
}

#if MCC_REPLAY_IR_C2
static int rir_unsafe(const char *why, AstLocal n, uint32_t nc) {
	snprintf(rir_c2_msg, sizeof rir_c2_msg, "arity %s n=%u nc=%u op=%d", why,
					 (unsigned)n, (unsigned)nc, ast_op(rir_arena, n));
	return 0;
}

static int rir_bb_slot(AstLocal n, uint32_t i, uint32_t nc) {
	AstLocal c;
	if (i >= nc)
		return 0;
	c = ast_child(rir_arena, n, i);
	return c != AST_NONE && ast_kind(rir_arena, c) == AST_BasicBlock;
}

static int rir_if_safe(AstLocal n, uint32_t nc) {
	switch (ast_op(rir_arena, n)) {
	case 0:
		return nc >= 2 && rir_bb_slot(n, 1, nc) && (nc < 3 || rir_bb_slot(n, 2, nc));
	case 2:
		return nc >= 2 && rir_bb_slot(n, 1, nc) && (nc < 3 || rir_bb_slot(n, 2, nc));
	case 3:
		return nc >= 3 && rir_bb_slot(n, 1, nc) && rir_bb_slot(n, 2, nc) &&
					 (nc < 4 || rir_bb_slot(n, 3, nc));
	case 4:
		return nc >= 2 && rir_bb_slot(n, 0, nc) && (nc < 3 || rir_bb_slot(n, 2, nc));
	case 6:
		return nc >= 2 && rir_bb_slot(n, 1, nc);
	case 8:
		return nc >= 2 && rir_bb_slot(n, 0, nc) && rir_bb_slot(n, 1, nc);
	case 9:
		if (nc != 2)
			return 0;
		return (ast_type_t(rir_arena, ast_child(rir_arena, n, 0)) & VT_BTYPE) !=
							 VT_FUNC &&
					 (ast_type_t(rir_arena, ast_child(rir_arena, n, 1)) & VT_BTYPE) !=
							 VT_FUNC;
	case 5:
	case 7:
		return nc >= 3;
	default:
		return 0;
	}
}

static int rir_leaf_reg_ok(AstLocal n) {
	int r = ast_op(rir_arena, n), v;
	if (r & VT_LVAL)
		return 1;
	v = r & VT_VALMASK;
	if (v >= MCC_NB_REGS)
		return 1;
	{
		int fl = (reg_classes[v] & MCC_RC_FLOAT) != 0;
#ifdef MCC_RC_ST0
		if (reg_classes[v] & MCC_RC_ST0)
			fl = 1;
#endif
		return fl == !!is_float(ast_type_t(rir_arena, n));
	}
}

static int rir_emit_safe(void) {
	AstLocal n;
	for (n = 0; n < ast_count(rir_arena); n++) {
		uint32_t nc = ast_nchild(rir_arena, n);
		switch (ast_kind(rir_arena, n)) {
		case AST_Literal:
		case AST_Ref:
			if (!rir_leaf_reg_ok(n))
				return rir_unsafe("leaf-regbank", n, nc);
			break;
		case AST_If:
			if (!rir_if_safe(n, nc))
				return rir_unsafe("If", n, nc);
			break;
		case AST_Store:
			if (nc != 2)
				return rir_unsafe("Store", n, nc);
			if (ast_kind(rir_arena, ast_child(rir_arena, n, 0)) == AST_Binary)
				return rir_unsafe("Store-target", n, nc);
			break;
		case AST_Binary: {
			int bop = ast_op(rir_arena, n);
			if (bop == TOK_LAND || bop == TOK_LOR) {
				if (nc < (ast_fbits(rir_arena, n) & AST_FB_LANDOR_MATERIAL ? 1u : 2u))
					return rir_unsafe("Binary-landor", n, nc);
#ifdef MCC_JRN_HAVE_X86_PRIMS
			} else if (bop == AST_OP_AXADD || bop == AST_OP_AXCHG ||
								 bop == AST_OP_ACMPXCHG) {
				if (nc != (bop == AST_OP_ACMPXCHG ? 3u : 2u))
					return rir_unsafe("Binary-atomic", n, nc);
#endif
			} else if (bop == AST_OP_CPLXBUILD) {
				Sym *cr = (Sym *)(uintptr_t)ast_type_ref(rir_arena, n);
				if (nc != 2 || (ast_type_t(rir_arena, n) & VT_BTYPE) != VT_STRUCT ||
						!cr || !cr->a.is_complex || !cr->next)
					return rir_unsafe("Binary-cplxbuild", n, nc);
			} else if (nc != 2) {
				return rir_unsafe("Binary", n, nc);
			}
			break;
		}
		case AST_Convert:
			if (nc != 1)
				return rir_unsafe("Convert", n, nc);
			break;
		case AST_Load:
			if (nc != 1)
				return rir_unsafe("Load", n, nc);
			break;
		case AST_Unary:
			if ((ast_op(rir_arena, n) == AST_OP_VLA ||
					 ast_op(rir_arena, n) == AST_OP_VLA_RESTORE ||
					 ast_op(rir_arena, n) == AST_OP_ASMGEN ||
					 ast_op(rir_arena, n) == AST_OP_ASM) &&
					nc == 0)
				break;
			/* ASMOPS carries one child per asm operand, however many that is. */
			if (ast_op(rir_arena, n) == AST_OP_ASMOPS)
				break;
			if (nc != 1)
				return rir_unsafe("Unary", n, nc);
			if (ast_op(rir_arena, n) == AST_OP_ADDR &&
					is_float(ast_type_t(rir_arena, n)))
				return rir_unsafe("Unary-addr-float", n, nc);
			break;
		case AST_Invoke: {
			AstLocal callee;
			int via_load = 0;
			if (nc < 1)
				return rir_unsafe("Invoke-nc", n, nc);
			callee = ast_child(rir_arena, n, 0);
			if (callee != AST_NONE && ast_type_t(rir_arena, callee) == 0 &&
				ast_kind(rir_arena, callee) == AST_Load &&
				ast_nchild(rir_arena, callee) == 1) {
				callee = ast_child(rir_arena, callee, 0);
				via_load = 1;
			}
			if (callee != AST_NONE && ast_type_t(rir_arena, callee) == 0 &&
				ast_kind(rir_arena, callee) == AST_Binary)
				via_load = 1;
			if (callee != AST_NONE && ast_type_t(rir_arena, callee) == 0 &&
				ast_kind(rir_arena, callee) == AST_If)
				via_load = 1;
			if (callee == AST_NONE ||
				(ast_type_t(rir_arena, callee) == 0 && !via_load))
				return rir_unsafe("Invoke-callee-untyped", n, nc);
			if (!via_load && ast_type_ref(rir_arena, callee) == 0)
				return rir_unsafe("Invoke-callee-noref", n, nc);
			if ((ast_type_t(rir_arena, callee) & VT_BTYPE) != VT_FUNC) {
				const Sym *r =
						(const Sym *)(uintptr_t)ast_type_ref(rir_arena, callee);
				if (!via_load &&
					((ast_type_t(rir_arena, callee) & (VT_BTYPE | VT_ARRAY)) !=
									VT_PTR ||
						(r->type.t & VT_BTYPE) != VT_FUNC))
					return rir_unsafe("Invoke-callee-notfunc", n, nc);
			}
			break;
		}
		case AST_Return: {
			AstLocal v;
			int vb;
			if (nc > 1)
				return rir_unsafe("Return", n, nc);
			if (nc == 0)
				break;
			v = ast_child(rir_arena, n, 0);
			if (v == AST_NONE)
				return rir_unsafe("Return-none", n, nc);
			vb = ast_type_t(rir_arena, v) & VT_BTYPE;
			if (vb == VT_QFLOAT || vb == VT_QLONG)
				return rir_unsafe("Return-wide", n, nc);
			if (((func_vt.t & VT_BTYPE) == VT_STRUCT) != (vb == VT_STRUCT) &&
					!(ast_kind(rir_arena, v) == AST_Load &&
						ast_type_t(rir_arena, v) == 0) &&
					!(is_complex_type(&func_vt) && vb != VT_STRUCT &&
						(vb != VT_VOID || (ast_type_t(rir_arena, v) == 0 &&
															 ast_kind(rir_arena, v) == AST_Binary))))
				return rir_unsafe("Return-struct", n, nc);
			break;
		}
		default:
			break;
		}
	}
	return 1;
}
#endif

static void rir_castgv_apply(void) {
	AstLocal top, cv;
	if (!rir_castgv_pend || --rir_castgv_pend)
		return;
	if (rir_shn <= 0)
		return;
	top = rir_sh[rir_shn - 1];
	if (top == AST_NONE || rir_shtype[rir_shn - 1])
		return;
	if (ast_kind(rir_arena, top) == AST_Convert) {
		ast_set_fbits(rir_arena, top,
									ast_fbits(rir_arena, top) | AST_FB_CONVERT_GV);
		return;
	}
	if (ast_parent(rir_arena, top) != AST_NONE)
		return;
	if (top != rir_castgv_top)
		return;
	cv = ast_node(rir_arena, AST_Convert);
	ast_set_type(rir_arena, cv, rir_castgv_t, rir_castgv_ref);
	ast_set_fbits(rir_arena, cv, AST_FB_CONVERT_GV);
	ast_add_child(rir_arena, cv, top);
	rir_sh[rir_shn - 1] = cv;
	if (rir_retexpr == top)
		rir_retexpr = cv;
	if (rir_pending_call == top)
		rir_pending_call = cv;
}

static void rir_to_arena(void) {
	int i;
	if (!rir_arena)
		rir_arena = ast_arena_new();
	else
		ast_arena_reset(rir_arena);
	rir_shn = 0;
	rir_cfn = 0;
	rir_while_pfx = AST_NONE;
	rir_bbn = 0;
	rir_last_return = AST_NONE;
	rir_pending_ret = AST_NONE;
	rir_ret_spilled = 0;
	rir_after_ret = 0;
	rir_cond_depth = 0;
	rir_synth_depth = 0;
	rir_call_depth = 0;
	rir_inc_depth = 0;
	rir_member_depth = 0;
	rir_vstruct_depth = 0;
	rir_vbf_depth = 0;
	rir_cx_depth = 0;
	rir_gret_depth = 0;
	rir_vla_depth = 0;
	rir_cvt_depth = 0;
	rir_cvt_n = 0;
	rir_argcast_n = 0;
	memset(rir_pvok, 0, sizeof rir_pvok);
	rir_ternn = 0;
	rir_tholdn = 0;
	rir_incr_bb = AST_NONE;
	rir_incr_live = 0;
	rir_lorn = 0;
	rir_docond = 0;
	rir_dheldn = 0;
	rir_fcs_node = AST_NONE;
	rir_iholdn = 0;
	rir_ihold_off = 0;
	rir_pending_call = AST_NONE;
	rir_spill_node = AST_NONE;
	rir_opassign_pending = 0;
	rir_addr_late = 0;
	rir_retexpr = AST_NONE;
	rir_retexpr_pending = 0;
	rir_castgv_pend = 0;
	rir_castgv_top = AST_NONE;
	rir_arena_mismatch = 0;
	rir_cplx_depth = 0;
	rir_cplxb_depth = 0;
	rir_cplxb_on = 0;
	rir_acas_depth = 0;
	rir_vsup_depth = 0;
	rir_vsup_nest = 0;
	rir_clg_n = 0;
	rir_clg_pending = NULL;
	rir_clg_syn = 0;
	rir_bb[rir_bbn++] = ast_node(rir_arena, AST_BasicBlock);
	for (i = 0; i < rir_n; i++) {
		RirOp *ro = &rir_ops[i];
		if ((ro->tag == RIR_T_RBEGIN || ro->tag == RIR_T_REND) &&
				ro->rkind == RIR_R_CPLX) {
			if (ro->tag == RIR_T_RBEGIN)
				rir_cplx_depth++;
			else if (rir_cplx_depth)
				rir_cplx_depth--;
			continue;
		}
		if (rir_cplx_depth)
			continue;
		if ((ro->tag == RIR_T_RBEGIN || ro->tag == RIR_T_REND) &&
				ro->rkind == RIR_R_ACAS) {
			if (ro->tag == RIR_T_RBEGIN) {
				if (!rir_acas_depth) {
					rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
					rir_acas_val = ro->rval;
				}
				rir_acas_depth++;
			} else if (rir_acas_depth && !--rir_acas_depth) {
				AstLocal rb = rir_pop(), ra = rir_pop(), rn;
				if (ra == AST_NONE || rb == AST_NONE) {
					rir_arena_mismatch++;
				} else {
					rn = ast_node(rir_arena, AST_Binary);
					ast_set_op(rir_arena, rn, AST_OP_ACASRMW);
					ast_set_ival(rir_arena, rn,
											 (uint64_t)(unsigned)rir_acas_val |
													 ((uint64_t)(unsigned)ro->rval << 32));
					ast_add_child(rir_arena, rn, ra);
					ast_add_child(rir_arena, rn, rb);
					rir_push_typed(rn);
				}
			}
			continue;
		}
		if (rir_acas_depth)
			continue;
		if (rir_vsup_depth) {
			if ((ro->tag == RIR_T_RBEGIN || ro->tag == RIR_T_REND) &&
					ro->rkind == RIR_R_VSTORE) {
				if (ro->tag == RIR_T_RBEGIN) {
					rir_vsup_nest++;
					continue;
				}
				if (rir_vsup_nest) {
					rir_vsup_nest--;
					continue;
				}
			} else {
				continue;
			}
		}
#if RIR_DBG_OPTRACE
		rir_dbg_ent = i;
		{
			const char *e = getenv("RIRDBG");
			if (e && funcname && !strcmp(e, funcname))
				fprintf(stderr, "[ent] %3d %-6s %-10s nc=%x shn=%d lorn=%d ternn=%d cond=%d\n", i,
								ro->tag == RIR_T_OP ? "OP" : ro->tag == RIR_T_MARK ? "MARK"
								: ro->tag == RIR_T_RBEGIN ? "RBEGIN" : "REND",
								ro->tag == RIR_T_OP ? jrn_op_name(ro->p.kind)
																		: rir_region_name(ro->rkind),
								ro->tag == RIR_T_OP ? (unsigned)ro->p.nocode
																		: (unsigned)ro->rnocode,
								rir_shn, rir_lorn, rir_ternn, rir_cond_depth);
		}
#endif
		if (ro->tag != RIR_T_OP && (ro->rnocode & RIR_NOEVAL_MASK))
			continue;
		if ((ro->tag == RIR_T_RBEGIN || ro->tag == RIR_T_REND) &&
				ro->rkind == RIR_R_CPLXB) {
			if (ro->tag == RIR_T_RBEGIN) {
				if (!rir_cplxb_depth) {
					if (!rir_cond_depth && !rir_inc_depth && !rir_member_depth &&
							!rir_retexpr_pending && !rir_vstruct_depth && !rir_vbf_depth &&
							!rir_cx_depth && !rir_vla_depth && !rir_cvt_depth)
						rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
					rir_cplxb_on =
							(rir_shn >= 2 && rir_shn == ro->mvs_n - ast_base_depth);
				}
				rir_cplxb_depth++;
			} else if (rir_cplxb_depth && !--rir_cplxb_depth && rir_cplxb_on) {
				AstLocal ci = rir_pop(), cr = rir_pop(), cn;
				if (cr == AST_NONE || ci == AST_NONE ||
						ro->mvs_n - ast_base_depth <= 0) {
					rir_arena_mismatch++;
				} else {
					const SValue *cv = &rir_mvs[ro->mvs_off + ro->mvs_n - 1];
					cn = ast_node(rir_arena, AST_Binary);
					ast_set_op(rir_arena, cn, AST_OP_CPLXBUILD);
					ast_add_child(rir_arena, cn, cr);
					ast_add_child(rir_arena, cn, ci);
					ast_set_type(rir_arena, cn, cv->type.t,
											 (uint64_t)(uintptr_t)cv->type.ref);
					rir_push(cn);
				}
			}
			continue;
		}
		if (rir_cplxb_depth && rir_cplxb_on)
			continue;
		if (ro->tag != RIR_T_MARK || ro->rkind != RIR_M_CASTGV)
			rir_castgv_apply();
		if (ro->tag == RIR_T_MARK) {
			int bound = rir_retexpr_pending && ro->rkind == RIR_M_RETURN && ro->rval;
			if ((ro->rkind == RIR_M_NORETURN ||
					 (!rir_cond_depth && !rir_synth_depth && !rir_call_depth &&
						!rir_inc_depth && !rir_member_depth && !rir_vstruct_depth &&
						!rir_vbf_depth && !rir_vla_depth)) &&
					(!rir_retexpr_pending || ro->rkind == RIR_M_RETURN ||
					 ro->rkind == RIR_M_NORETURN)) {
				if (bound || ro->rkind == RIR_M_NORETURN)
					;
				else if (ro->rkind == RIR_M_BFGV)
					;
				else if (ro->rkind == RIR_M_LOAD || ro->rkind == RIR_M_RETEXPR ||
								 ro->rkind == RIR_M_RETURN ||
								 ro->rkind == RIR_M_TERNHOLD || ro->rkind == RIR_M_TERNPICK)
					rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
				else
					rir_stamp_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
				rir_mark_apply(ro);
			}
			continue;
		}
		if (ro->tag != RIR_T_OP) {
			if (ro->tag == RIR_T_RBEGIN && !rir_synth_depth && !rir_call_depth &&
					!rir_inc_depth && !rir_member_depth &&
					(ro->rkind == RIR_R_IF || ro->rkind == RIR_R_WHILE ||
					 ro->rkind == RIR_R_SWITCH || ro->rkind == RIR_R_TERNARY ||
					 ro->rkind == RIR_R_INC || ro->rkind == RIR_R_MEMBER ||
					 ro->rkind == RIR_R_CVT))
				rir_reconcile_sv(rir_mvs + ro->mvs_off, ro->mvs_n);
			rir_region(ro);
			continue;
		}
		if (rir_cond_depth || rir_inc_depth || rir_member_depth ||
				rir_retexpr_pending || rir_vstruct_depth || rir_vbf_depth ||
				rir_cx_depth || rir_vla_depth || rir_cvt_depth)
			continue;
		rir_cvt_next = rir_is_cvt(ro->p.kind);
		rir_reconcile(&ro->p);
		rir_cvt_next = 0;
		rir_op_effect(ro);
	}
	rir_flush_pending_call();
	if (rir_pending_ret != AST_NONE) {
		rir_stmt(rir_pending_ret);
		rir_pending_ret = AST_NONE;
	}
	while (rir_shn > 0) {
		AstLocal d = rir_pop();
		if (rir_effectful(d))
			rir_stmt(d);
	}
	rir_ihold_flush();
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
				int live = (int)(vtop - vstack);
				for (k = 0; k < o->p.vs_n && k <= live; k++) {
					unsigned char fl = rir_vscapt[o->p.vs_off + k];
					int L = rir_vslbl[o->p.vs_off + k];
					int L2 = rir_vslbl2[o->p.vs_off + k];
					if (!fl)
						continue;
					if ((fl & 1) && L >= 0)
						rir_lblhead[L] = vstack[k].r == VT_CMP ? vstack[k].jtrue
																									: (int)vstack[k].c.i;
					if ((fl & 2) && L2 >= 0 && vstack[k].r == VT_CMP)
						rir_lblhead[L2] = vstack[k].jfalse;
				}
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
							ast_fbits(rir_arena, q) == ast_fbits(ast_cur, q) &&
							ast_nchild(rir_arena, q) == ast_nchild(ast_cur, q))
						continue;
					if (ast_kind(rir_arena, q) == ast_kind(ast_cur, q) &&
							ast_fbits(rir_arena, q) != ast_fbits(ast_cur, q))
						fprintf(stderr, "[rir-fb] %s n%u %s rir=%llx tree=%llx\n",
										funcname, q, ast_kind_name(ast_kind(rir_arena, q)),
										(unsigned long long)ast_fbits(rir_arena, q),
										(unsigned long long)ast_fbits(ast_cur, q));
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
					ast_intention_hash(ast_cur, ast_root(ast_cur))) {
				rir_tot_arena_hash_eq++;
				rir_body_hasheq = 1;
				if (rir_env >= 6) {
					AstArena *pa = ast_arena_clone(rir_arena);
					AstArena *pb = ast_arena_clone(ast_cur);
					int fa, fb;
					rir_tot_c3_pair++;
					ast_tmpl_folds = 0;
					ast_run_templates(pa);
					fa = ast_tmpl_folds + rir_c3_pipeline(pa);
					ast_tmpl_folds = 0;
					ast_run_templates(pb);
					fb = ast_tmpl_folds + rir_c3_pipeline(pb);
					ast_tmpl_folds = 0;
					if (fa > 0 || fb > 0)
						rir_tot_c3_pair_fired++;
					if (fa == fb)
						rir_tot_c3_same_folds++;
					if (ast_intention_hash(pa, ast_root(pa)) ==
							ast_intention_hash(pb, ast_root(pb)))
						rir_tot_c3_same_hash++;
					else
						fprintf(stderr, "[rir-c3] %s\tPASS-DIVERGED folds %d vs %d\n",
										funcname, fa, fb);
					ast_arena_free(pa);
					ast_arena_free(pb);
				}
			}
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
				{
					int L = mcc_state->cg_func_alloca;
					while (L >= base2 && L + 4 <= ind) {
						int nx = (int)read32le(cur_text_section->data + L);
						write32le(cur_text_section->data + L,
											nx ? (uint32_t)(nx - shift) : 0);
						L = nx;
					}
				}
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
	if (rir_env >= 5 && faithful && body_len > 0 && rir_arena_mismatch) {
		rir_tot_c2_skip++;
		if (getenv("RIRSKIP"))
			fprintf(stderr, "[rir-c2skip] %s mism=%d\n", funcname, rir_arena_mismatch);
	}
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
		loc = rir_body_loc_sv;
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
			if (rir_env >= 6) {
				AstArena *c3 = ast_arena_clone(rir_arena);
				char c3msg[256];
				int folds;
				rir_tot_c3_try++;
				ast_tmpl_folds = 0;
				ast_run_templates(c3);
				folds = ast_tmpl_folds + rir_c3_pipeline(c3);
				rir_tot_c3_ran++;
				rir_tot_c3_folds += folds;
				c3msg[0] = 0;
				if (ast_validate(c3, c3msg, sizeof c3msg) != 0) {
					rir_tot_c3_broke++;
					fprintf(stderr, "[rir-c3] %s\tINVALID-AFTER-PASSES %s\n",
									funcname, c3msg);
				}
				ast_arena_free(c3);
				ast_tmpl_folds = 0;
			}
			rir_locrec_i = 0;
			rir_slotrec_i = 0;
			rir_tvrec_i = 0;
			rir_fcrec_i = 0;
			rir_c2_active = 1;
			ast_replay_body(getenv("RIRC2TREE") && ast_cur && ast_replay_ok(ast_cur)
													? ast_cur
													: rir_arena);
			rir_c2_active = 0;
			if (rir_env >= 5)
				fprintf(stderr, "[rir-c2part] %s heq=%d ok=%d\n", funcname,
					rir_body_hasheq,
					ind - ast_body_ind_sv == body_len &&
						memcmp(cur_text_section->data + ast_body_ind_sv, orig,
							(size_t)body_len) == 0);
			if (ind - ast_body_ind_sv == body_len &&
					memcmp(cur_text_section->data + ast_body_ind_sv, orig,
								 (size_t)body_len) == 0)
				rir_tot_c2_ok++;
			else if (ind - ast_body_ind_sv == body_len) {
				rir_tot_c2_bytes++;
				if (rir_env >= 5) {
					int q, d = -1;
					for (q = 0; q < body_len; q++)
						if (cur_text_section->data[ast_body_ind_sv + q] != orig[q]) {
							d = q;
							break;
						}
					{
						int bi = rir_blame(d);
						fprintf(stderr, "[rir-c2op] %s firstdiff=%d op=%s idx=%d win=[%d,%d)\n",
										funcname, d,
										bi >= 0 ? jrn_op_name(rir_ops[bi].p.kind) : "-", bi,
										bi >= 0 ? rir_ops[bi].p.ind_pre - ast_body_ind_sv : -1,
										bi >= 0 ? rir_ops[bi].p.ind_post - ast_body_ind_sv : -1);
					}
					int w0 = d > 16 ? d - 16 : 0, w1 = d + 32;
					if (w1 > body_len)
						w1 = body_len;
					fprintf(stderr, "[rir-c2byte] %s len=%d firstdiff=%d from=%d\n  parser:",
									funcname, body_len, d, w0);
					for (q = w0; q < w1; q++)
						fprintf(stderr, " %02x", orig[q]);
					fprintf(stderr, "\n  rir   :");
					for (q = w0; q < w1; q++)
						fprintf(stderr, " %02x",
										cur_text_section->data[ast_body_ind_sv + q]);
					fprintf(stderr, "\n");
				}
			} else {
				rir_tot_c2_len++;
				if (rir_env >= 5) {
					int q, gl = ind - ast_body_ind_sv;
					int lim = gl < body_len ? gl : body_len, fd = -1, from, run = 0,
							fb = -1;
					for (q = 0; q < lim; q++)
						if (cur_text_section->data[ast_body_ind_sv + q] != orig[q]) {
							if (fd < 0)
								fd = q;
							if (++run >= 3 && fb < 0)
								fb = q - 2;
						} else {
							run = 0;
						}
					if (fd < 0)
						fd = lim;
					if (fb < 0)
						fb = fd;
					from = fb > 16 ? fb - 16 : 0;
					{
						int bi = rir_blame(fb);
						if (getenv("RIRDUMP")) {
							int z;
							for (z = (bi > 6 ? bi - 6 : 0); z < bi + 3 && z < rir_n; z++)
								fprintf(stderr, "    [%d] tag=%d kind=%s win=[%d,%d)\n", z,
												rir_ops[z].tag,
												rir_ops[z].tag == RIR_T_OP
														? jrn_op_name(rir_ops[z].p.kind) : "MARK",
												rir_ops[z].tag == RIR_T_OP
														? rir_ops[z].p.ind_pre - ast_body_ind_sv : -1,
												rir_ops[z].tag == RIR_T_OP
														? rir_ops[z].p.ind_post - ast_body_ind_sv : -1);
						}
						fprintf(stderr, "[rir-c2op] %s firstdiff=%d firstblk=%d op=%s idx=%d win=[%d,%d)\n",
										funcname, fd, fb,
										bi >= 0 ? jrn_op_name(rir_ops[bi].p.kind) : "-", bi,
														bi >= 0 ? rir_ops[bi].p.ind_pre - ast_body_ind_sv : -1,
														bi >= 0 ? rir_ops[bi].p.ind_post - ast_body_ind_sv : -1);
					}
					fprintf(stderr,
									"[rir-c2len] %s want=%d got=%d firstdiff=%d firstblk=%d\n"
									"  parser:",
									funcname, body_len, gl, fd, fb);
					for (q = from; q < body_len && q < from + 40; q++)
						fprintf(stderr, " %02x", orig[q]);
					fprintf(stderr, "\n  rir   :");
					for (q = from; q < gl && q < from + 40; q++)
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
					"leaf=%ld refill=%ld c2try=%ld c2skip=%ld c2ok=%ld c2bytes=%ld c2len=%ld c2err=%ld c2invalid=%ld\n",
					rir_tot_fn, rir_tot_faithful, rir_tot_ops, rir_tot_regions,
					rir_tot_labels, rir_tot_jumps, rir_tot_fallback,
					rir_tot_fallback_fn, rir_tot_fb_chain, rir_tot_fb_point,
					rir_tot_unbal, rir_tot_ovf, rir_tot_jmpsv, rir_tot_jmpsv_fb,
					rir_tot_shift_ok, rir_tot_shift_bad, rir_tot_shift_skip,
					rir_tot_shift_open, rir_tot_arena_fn, rir_tot_arena_nodes,
					rir_tot_arena_cmp, rir_tot_arena_count_eq, rir_tot_arena_hash_eq,
					rir_tot_tree_nodes, rir_tot_arena_cmp_nodes, rir_tot_leaf, rir_tot_refill, rir_tot_c2_try, rir_tot_c2_skip,
					rir_tot_c2_ok, rir_tot_c2_bytes, rir_tot_c2_len, rir_tot_c2_err,
					rir_tot_c2_invalid);
	if (rir_tot_c3_try || rir_tot_c3_pair)
		fprintf(f,
						"[rir-c3] try=%ld ran=%ld folds=%ld broke=%ld pair=%ld "
						"samefolds=%ld samehash=%ld pairfired=%ld\n",
						rir_tot_c3_try, rir_tot_c3_ran, rir_tot_c3_folds,
						rir_tot_c3_broke, rir_tot_c3_pair, rir_tot_c3_same_folds,
						rir_tot_c3_same_hash, rir_tot_c3_pair_fired);
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
