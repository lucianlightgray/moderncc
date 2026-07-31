#if MCC_CONFIG_OPTIMIZER && (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))
#if MCC_REPLAY_IR

enum { RIR_T_OP = 0, RIR_T_RBEGIN, RIR_T_REND };

typedef struct RirOp {
	int tag;
	int rkind;
	JrnOp p;
} RirOp;

typedef struct RirMark {
	int tag;
	int kind;
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
static long rir_tot_fn, rir_tot_faithful, rir_tot_ops, rir_tot_regions;
static long rir_tot_unbal, rir_tot_ovf;
static long rir_reghist[RIR_R_COUNT];

static const char *rir_region_name(int k) {
	static const char *const n[RIR_R_COUNT] = {
			"none", "if",	 "then",		"else",		"while", "do",
			"for",	"switch", "ternary", "landor", "call"};
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

static void rir_mark(int tag, int kind) {
	RirMark *m;
	if (rir_markn >= rir_markcap) {
		rir_markcap = rir_markcap ? rir_markcap * 2 : 128;
		rir_marks = mcc_realloc(rir_marks, (size_t)rir_markcap * sizeof *rir_marks);
	}
	m = &rir_marks[rir_markn++];
	m->tag = tag;
	m->kind = kind;
	m->at = jrn_n;
	rir_tot_regions++;
	if (kind >= 0 && kind < RIR_R_COUNT)
		rir_reghist[kind]++;
}

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

void rir_reset(void) {
	rir_n = 0;
	rir_markn = 0;
	rir_stackn = 0;
	rir_unbal = 0;
	rir_ovf = 0;
	rir_fail_op = -1;
	rir_fail_kind = -1;
}

static void rir_build(void) {
	int i, m = 0;
	rir_n = 0;
	for (i = 0; i <= jrn_n; i++) {
		while (m < rir_markn && rir_marks[m].at <= i) {
			RirOp *o = rir_new(rir_marks[m].tag);
			o->rkind = rir_marks[m].kind;
			m++;
		}
		if (i < jrn_n) {
			RirOp *o = rir_new(RIR_T_OP);
			o->p = jrn_ops[i];
		}
	}
}

static void rir_run(void) {
	int i;
	rir_fail_op = -1;
	rir_fail_kind = -1;
	for (i = 0; i < rir_n; i++) {
		RirOp *o = &rir_ops[i];
		if (o->tag != RIR_T_OP)
			continue;
		nocode_wanted = o->p.nocode;
		loc = o->p.loc_pre;
		nb_temp_local_vars = o->p.ntlv_pre;
		if (o->p.vs_n >= 0) {
			if (o->p.vs_n)
				memcpy(vstack, jrn_vs + o->p.vs_off,
							 (size_t)o->p.vs_n * sizeof(SValue));
			vtop = vstack + o->p.vs_n - 1;
		}
		if (ind != o->p.ind_pre) {
			rir_fail_op = i;
			rir_fail_kind = o->p.kind;
			return;
		}
		jrn_fc_cur = o->p.fc_off;
		jrn_fc_end = o->p.fc_off + o->p.fc_n;
		jrn_pred_have = o->p.swpred != 0;
		jrn_pred_cur = o->p.swpred - 1;
		jrn_issue(&o->p);
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
			fprintf(f, "%s\t%s\t%s\tops=%d\tregions=%d\tunbal=%d\tovf=%d\n", verdict,
							vf, funcname, ops, regions, rir_unbal, rir_ovf);
			fclose(f);
		}
	} else {
		fprintf(stderr,
						"[rir-verify] %s\t%s\t%s\tops=%d\tregions=%d\tunbal=%d\tovf=%d\n",
						verdict, vf, funcname, ops, regions, rir_unbal, rir_ovf);
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
	rir_build();
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
					"[rir-total] fn=%ld faithful=%ld ops=%ld regions=%ld unbal=%ld "
					"ovf=%ld\n",
					rir_tot_fn, rir_tot_faithful, rir_tot_ops, rir_tot_regions,
					rir_tot_unbal, rir_tot_ovf);
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
