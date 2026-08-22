#if (defined(MCC_INTERNAL) || !defined(MCC_AMALGAMATED))

#pragma push_macro("gjmp")
#pragma push_macro("gjmp_addr")
#undef gjmp
#undef gjmp_addr
#define IR_OP_LIST(X) \
	X(IR_OP_RAW = 0, "raw") \
	X(IR_OP_LOAD, "load") \
	X(IR_OP_STORE, "store") \
	X(IR_OP_OPI, "opi") \
	X(IR_OP_OPL, "opl") \
	X(IR_OP_OPF, "opf") \
	X(IR_OP_CALL, "call") \
	X(IR_OP_JMP, "jmp") \
	X(IR_OP_JMPADDR, "jmpaddr") \
	X(IR_OP_JMPCOND, "jmpcond") \
	X(IR_OP_JMPAPPEND, "jmpappend") \
	X(IR_OP_GSYMADDR, "gsymaddr") \
	X(IR_OP_CVT_ITOF, "cvt_itof") \
	X(IR_OP_CVT_FTOF, "cvt_ftof") \
	X(IR_OP_CVT_FTOI, "cvt_ftoi") \
	X(IR_OP_CVT_SXTW, "cvt_sxtw") \
	X(IR_OP_CVT_ZXTW, "cvt_zxtw") \
	X(IR_OP_CVT_TRUNC32, "cvt_trunc") \
	X(IR_OP_CVT_CSTI, "cvt_csti") \
	X(IR_OP_STRUCTCOPY, "structcpy") \
	X(IR_OP_GGOTO, "ggoto") \
	X(IR_OP_CMOV, "cmov") \
	X(IR_OP_FILLNOPS, "fillnops") \
	X(IR_OP_VLA_SPSAVE, "vla_save") \
	X(IR_OP_VLA_SPREST, "vla_rest") \
	X(IR_OP_VLA_RESULT, "vla_res") \
	X(IR_OP_VLA_ALLOC, "vla_alloc") \
	X(IR_OP_MULH, "mulh") \
	X(IR_OP_MULWIDEN, "mulwiden") \
	X(IR_OP_REGADDI, "regaddi") \
	X(IR_OP_FABS, "fabs") \
	X(IR_OP_BSWAP, "bswap") \
	X(IR_OP_SQRT, "sqrt") \
	X(IR_OP_ROUND, "round") \
	X(IR_OP_COPYSIGN, "copysign") \
	X(IR_OP_SIGNBIT, "signbit") \
	X(IR_OP_FFS, "ffs") \
	X(IR_OP_BITSCAN, "bitscan") \
	X(IR_OP_TRAP, "trap") \
	X(IR_OP_TCOV, "tcov") \
	X(IR_OP_ATOMIC_CMPXCHG, "acmpxchg") \
	X(IR_OP_ATOMIC_XCHG, "axchg") \
	X(IR_OP_ATOMIC_XADD, "axadd") \
	X(IR_OP_ASAN_SHADOW, "asan") \
	X(IR_OP_ASAN_MARK_WRITE, "asanwr") \
	X(IR_OP_UBSAN_NULLPTR, "ubsannull") \
	X(IR_OP_XFERRET, "xferret") \
	X(IR_OP_X87POP, "x87pop") \
	X(IR_OP_VSETC, "vsetc") \
	X(IR_OP_VPUSHSYM, "vpushsym") \
	X(IR_OP_VPUSHV, "vpushv") \
	X(IR_OP_VSWAP, "vswap") \
	X(IR_OP_VPOP, "vpop") \
	X(IR_OP_VROTB, "vrotb") \
	X(IR_OP_VROTT, "vrott") \
	X(IR_OP_VREV, "vrev") \
	X(IR_OP_PUSHLIT, "pushlit") \
	X(IR_OP_GV, "gv") \
	X(IR_OP_VSTORE, "vstore") \
	X(IR_OP_GENOP, "genop") \
	X(IR_OP_MKPTR, "mkptr") \
	X(IR_OP_ADDROF, "addrof") \
	X(IR_OP_RETVAL, "retval") \
	X(IR_OP_VA_START, "va_start") \
	X(IR_OP_VA_ARG, "va_arg") \
	X(IR_OP_ASM, "asm") \
	X(IR_OP_ASMGEN, "asmgen") \
	X(IR_OP_BITBUILTIN, "bitbuiltin")

enum {
#define IR_OP_ENUM(N, S) N,
	IR_OP_LIST(IR_OP_ENUM)
#undef IR_OP_ENUM
	IR_OP_COUNT
};

typedef struct IrCapOp {
	int kind;
	int nocode;
	int a0, a1, a2, a3;
	int64_t d64;
	CType ctype;
	CValue cval;
	Sym *sym;
	SValue svarg;
	int sv_slot;
	int vs_off, vs_n;
	int loc_pre, ntlv_pre;
	int ind_pre, ind_post;
	int rel_pre, rel_post;
	int raw_off, raw_len;
	int rawrel_off, rawrel_len;
	int fc_off, fc_n;
	int swpred;
	int ret;
} IrCapOp;

static int ir_cap_active;
static int ir_cap_depth;
static int ir_cap_bad;
static int ir_cap_asm_n;
static int ir_cap_ind_wm;
static int ir_cap_rel_wm;

static IrCapOp *ir_cap_ops;
static int ir_cap_n, ir_cap_cap;
static SValue *ir_cap_vs;
static int ir_cap_vsn, ir_cap_vscap;
static unsigned char *ir_cap_raw;
static int ir_cap_rawn, ir_cap_rawcap;
static IrCapOp *ir_cap_pending;
#define IR_CAP_REC (ir_cap_pending && ir_cap_depth == 1)

static int *ir_cap_fc;
static int ir_cap_fcn, ir_cap_fccap;

void ir_cap_teardown(void) { MCC_TRACE("enter\n");
	mcc_free(ir_cap_ops);
	mcc_free(ir_cap_vs);
	mcc_free(ir_cap_raw);
	mcc_free(ir_cap_fc);
	ir_cap_ops = NULL;
	ir_cap_vs = NULL;
	ir_cap_raw = NULL;
	ir_cap_fc = NULL;
	ir_cap_n = ir_cap_cap = 0;
	ir_cap_vsn = ir_cap_vscap = 0;
	ir_cap_rawn = ir_cap_rawcap = 0;
	ir_cap_fcn = ir_cap_fccap = 0;
}
static int ir_cap_fc_cur, ir_cap_fc_end;

static int ir_cap_fconst_take(int *out) { MCC_TRACE_WHEN(ir_cap_replaying, "enter\n");
	if (!ir_cap_replaying)
		{ MCC_TRACE_WHEN(ir_cap_replaying, "br\n"); return 0; }
	if (ir_cap_fc_cur >= ir_cap_fc_end)
		{ MCC_TRACE_WHEN(ir_cap_replaying, "br\n"); return 0; }
	*out = ir_cap_fc[ir_cap_fc_cur++];
	return 1;
}

static void ir_cap_fconst_note(int c) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	if (!ir_cap_active || !ir_cap_pending || ir_cap_depth < 1)
		{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); return; }
	if (ir_cap_fcn >= ir_cap_fccap) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		ir_cap_fccap = ir_cap_fccap ? ir_cap_fccap * 2 : 64;
		ir_cap_fc = mcc_realloc(ir_cap_fc, (size_t)ir_cap_fccap * sizeof *ir_cap_fc);
	}
	if (ir_cap_pending->fc_n == 0)
		{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); ir_cap_pending->fc_off = ir_cap_fcn; }
	ir_cap_fc[ir_cap_fcn++] = c;
	ir_cap_pending->fc_n++;
}

static int ir_cap_pred_cur, ir_cap_pred_have;

int ir_cap_pred(int p) { MCC_TRACE_WHEN(ir_cap_active || ir_cap_replaying, "enter\n");
	if (ir_cap_replaying && ir_cap_pred_have)
		{ MCC_TRACE_WHEN(ir_cap_active || ir_cap_replaying, "br\n"); return ir_cap_pred_cur; }
	if (ir_cap_active && ir_cap_pending && ir_cap_depth >= 1)
		{ MCC_TRACE_WHEN(ir_cap_active || ir_cap_replaying, "br\n"); ir_cap_pending->swpred = p + 1; }
	return p;
}

static const char *ir_cap_op_name(int k) { MCC_TRACE("enter\n");
	static const char *const n[IR_OP_COUNT] = {
#define IR_OP_NAME(N, S) S,
			IR_OP_LIST(IR_OP_NAME)
#undef IR_OP_NAME
	};
	if (k < 0 || k >= IR_OP_COUNT)
		{ MCC_TRACE("br\n"); return "?"; }
	return n[k];
}

static int ir_cap_relofs(void) { MCC_TRACE("enter\n");
	Section *rs = cur_text_section ? cur_text_section->reloc : NULL;
	return rs ? (int)rs->data_offset : 0;
}

static IrCapOp *ir_cap_new_op(int kind) { MCC_TRACE("enter\n");
	IrCapOp *o;
	if (ir_cap_n >= ir_cap_cap) { MCC_TRACE("br\n");
		ir_cap_cap = ir_cap_cap ? ir_cap_cap * 2 : 256;
		ir_cap_ops = mcc_realloc(ir_cap_ops, (size_t)ir_cap_cap * sizeof *ir_cap_ops);
	}
	o = &ir_cap_ops[ir_cap_n++];
	memset(o, 0, sizeof *o);
	o->kind = kind;
	o->sv_slot = -1;
	o->loc_pre = loc;
	o->ntlv_pre = nb_temp_local_vars;
	return o;
}

static void ir_cap_snap_vstack(IrCapOp *o) { MCC_TRACE("enter\n");
	int n = (int)(vtop - vstack + 1);
	if (n < 0)
		{ MCC_TRACE("br\n"); n = 0; }
	if (ir_cap_vsn + n > ir_cap_vscap) { MCC_TRACE("br\n");
		int ncap = ir_cap_vscap ? ir_cap_vscap * 2 : 1024;
		while (ncap < ir_cap_vsn + n)
			{ MCC_TRACE("br\n"); ncap *= 2; }
		ir_cap_vs = mcc_realloc(ir_cap_vs, (size_t)ncap * sizeof *ir_cap_vs);
		ir_cap_vscap = ncap;
	}
	if (n)
		{ MCC_TRACE("br\n"); memcpy(ir_cap_vs + ir_cap_vsn, vstack, (size_t)n * sizeof(SValue)); }
	o->vs_off = ir_cap_vsn;
	o->vs_n = n;
	rir_snap_types(ir_cap_vs + ir_cap_vsn, n);
	ir_cap_vsn += n;
}

static int ir_cap_raw_add(const unsigned char *p, int len) { MCC_TRACE("enter\n");
	int off = ir_cap_rawn;
	if (len <= 0)
		{ MCC_TRACE("br\n"); return off; }
	if (ir_cap_rawn + len > ir_cap_rawcap) { MCC_TRACE("br\n");
		int ncap = ir_cap_rawcap ? ir_cap_rawcap * 2 : 4096;
		while (ncap < ir_cap_rawn + len)
			{ MCC_TRACE("br\n"); ncap *= 2; }
		ir_cap_raw = mcc_realloc(ir_cap_raw, (size_t)ncap);
		ir_cap_rawcap = ncap;
	}
	memcpy(ir_cap_raw + off, p, (size_t)len);
	ir_cap_rawn += len;
	return off;
}

static void ir_cap_gap(void) { MCC_TRACE("enter\n");
	int reln = ir_cap_relofs();
	int clen = ind - ir_cap_ind_wm;
	int rlen = reln - ir_cap_rel_wm;
	IrCapOp *o;
	if (clen == 0 && rlen == 0)
		{ MCC_TRACE("br\n"); return; }
	if (clen < 0 || rlen < 0) { MCC_TRACE("br\n");
		ir_cap_bad = 1;
		ir_cap_ind_wm = ind;
		ir_cap_rel_wm = reln;
		return;
	}
	o = ir_cap_new_op(IR_OP_RAW);
	o->ind_pre = ir_cap_ind_wm;
	o->ind_post = ind;
	o->rel_pre = ir_cap_rel_wm;
	o->rel_post = reln;
	o->nocode = nocode_wanted;
	o->vs_off = 0;
	o->vs_n = -1;
	o->raw_len = clen;
	o->raw_off = ir_cap_raw_add(cur_text_section->data + ir_cap_ind_wm, clen);
	o->rawrel_len = rlen;
	if (rlen)
		{ MCC_TRACE("br\n"); o->rawrel_off =
				ir_cap_raw_add(cur_text_section->reloc->data + ir_cap_rel_wm, rlen); }
	ir_cap_ind_wm = ind;
	ir_cap_rel_wm = reln;
}

static void ir_cap_begin(int kind, const SValue *sv) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	IrCapOp *o;
#if MCC_DIAG
	if (rir_dbg_on())
		fprintf(stderr, "[optrace] %s %s ind=%d vn=%d\n",
						rir_c2_active ? "C2   " : (ir_cap_replaying ? "CAP  " : "PARSE"),
						ir_cap_op_name(kind), ind, (int)(vtop - vstack + 1));
#endif
	if (!ir_cap_active)
		{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); return; }
	if (ir_cap_depth++ > 0)
		{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); return; }
	ir_cap_gap();
	o = ir_cap_new_op(kind);
	o->nocode = nocode_wanted;
	o->ind_pre = ind;
	o->rel_pre = ir_cap_relofs();
	ir_cap_snap_vstack(o);
	if (sv) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		o->svarg = *sv;
		if (sv >= vstack && sv <= vtop)
			{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); o->sv_slot = (int)(sv - vstack); }
	}
	ir_cap_pending = o;
}

static void ir_cap_end(void) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	if (!ir_cap_active)
		{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); return; }
	if (--ir_cap_depth > 0)
		{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); return; }
	if (ir_cap_pending) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		ir_cap_pending->ind_post = ind;
		ir_cap_pending->rel_post = ir_cap_relofs();
		ir_cap_pending = NULL;
	}
	ir_cap_ind_wm = ind;
	ir_cap_rel_wm = ir_cap_relofs();
}

#define IR_CAP_W0(name, KIND)                    \
	void ir_cap_##name(void) {                     \
		ir_cap_begin(KIND, NULL);                    \
		(name)();                                 \
		ir_cap_end();                                \
	}
#define IR_CAP_W1(name, KIND)                    \
	void ir_cap_##name(int a0) {                   \
		ir_cap_begin(KIND, NULL);                    \
		if (IR_CAP_REC)                              \
			ir_cap_pending->a0 = a0;                   \
		(name)(a0);                               \
		ir_cap_end();                                \
	}
#define IR_CAP_W2(name, KIND)                    \
	void ir_cap_##name(int a0, int a1) {           \
		ir_cap_begin(KIND, NULL);                    \
		if (IR_CAP_REC) {                            \
			ir_cap_pending->a0 = a0;                   \
			ir_cap_pending->a1 = a1;                   \
		}                                         \
		(name)(a0, a1);                           \
		ir_cap_end();                                \
	}
#define IR_CAP_R1(name, KIND)                    \
	int ir_cap_##name(int a0) {                    \
		int rv;                                   \
		ir_cap_begin(KIND, NULL);                    \
		if (IR_CAP_REC)                              \
			ir_cap_pending->a0 = a0;                   \
		rv = (name)(a0);                          \
		if (IR_CAP_REC)                              \
			ir_cap_pending->ret = rv;                  \
		ir_cap_end();                                \
		return rv;                                \
	}
#define IR_CAP_R2(name, KIND)                    \
	int ir_cap_##name(int a0, int a1) {            \
		int rv;                                   \
		ir_cap_begin(KIND, NULL);                    \
		if (IR_CAP_REC) {                            \
			ir_cap_pending->a0 = a0;                   \
			ir_cap_pending->a1 = a1;                   \
		}                                         \
		rv = (name)(a0, a1);                      \
		if (IR_CAP_REC)                              \
			ir_cap_pending->ret = rv;                  \
		ir_cap_end();                                \
		return rv;                                \
	}
#define IR_CAP_WSV(name, KIND)                   \
	void ir_cap_##name(int a0, SValue *sv) {       \
		ir_cap_begin(KIND, sv);                      \
		if (IR_CAP_REC)                              \
			ir_cap_pending->a0 = a0;                   \
		(name)(a0, sv);                           \
		ir_cap_end();                                \
	}

IR_CAP_WSV(load, IR_OP_LOAD)
IR_CAP_WSV(store, IR_OP_STORE)
IR_CAP_W1(gen_opi, IR_OP_OPI)
IR_CAP_W1(gen_opl, IR_OP_OPL)
IR_CAP_W1(gen_opf, IR_OP_OPF)
IR_CAP_W1(gfunc_call, IR_OP_CALL)
IR_CAP_R1(gjmp, IR_OP_JMP)
IR_CAP_W1(gjmp_addr, IR_OP_JMPADDR)
IR_CAP_R2(gjmp_cond, IR_OP_JMPCOND)
IR_CAP_R2(gjmp_append, IR_OP_JMPAPPEND)
IR_CAP_W2(gsym_addr, IR_OP_GSYMADDR)
IR_CAP_W1(gen_cvt_itof, IR_OP_CVT_ITOF)
IR_CAP_W1(gen_cvt_ftof, IR_OP_CVT_FTOF)
IR_CAP_W1(gen_cvt_ftoi, IR_OP_CVT_FTOI)
#ifdef MCC_IR_HAVE_CVT_SXTW
IR_CAP_W0(gen_cvt_sxtw, IR_OP_CVT_SXTW)
#endif
#ifdef MCC_IR_HAVE_CVT_ZXTW
IR_CAP_W0(gen_cvt_zxtw, IR_OP_CVT_ZXTW)
#endif
#ifdef MCC_IR_HAVE_X86_PRIMS
IR_CAP_W0(gen_cvt_trunc32, IR_OP_CVT_TRUNC32)
#endif
#ifdef MCC_IR_HAVE_CVT_CSTI
IR_CAP_W1(gen_cvt_csti, IR_OP_CVT_CSTI)
#endif
#ifdef MCC_IR_HAVE_STRUCT_COPY
#if RIR_DBG_STRUCTCPY
void ir_cap_gen_struct_copy(int a0) { MCC_TRACE("enter\n");
	if (rir_dbg_on())
		fprintf(stderr,
						"[scpy] %s phase=%s size=%d dst(r=%x c=%lld t=%x) src(r=%x c=%lld "
						"t=%x) ind=%d loc=%d func_vc=%d ntlv=%d frontier=%d\n",
						funcname,
						rir_c2_active ? "C2" : (ir_cap_replaying ? "CAP" : "PARSE"), a0,
						vtop[-1].r, (long long)vtop[-1].c.i, vtop[-1].type.t, vtop[0].r,
						(long long)vtop[0].c.i, vtop[0].type.t, ind, loc, func_vc,
						nb_temp_local_vars, ast_temp_frontier);
	ir_cap_begin(IR_OP_STRUCTCOPY, NULL);
	if (IR_CAP_REC)
		ir_cap_pending->a0 = a0;
	(gen_struct_copy)(a0);
	ir_cap_end();
}
#else
IR_CAP_W1(gen_struct_copy, IR_OP_STRUCTCOPY)
#endif
#endif
IR_CAP_W0(ggoto, IR_OP_GGOTO)
IR_CAP_W1(gen_fill_nops, IR_OP_FILLNOPS)
IR_CAP_W1(gen_vla_sp_save, IR_OP_VLA_SPSAVE)
IR_CAP_W1(gen_vla_sp_restore, IR_OP_VLA_SPREST)
#ifdef MCC_IR_HAVE_VLA_RESULT
IR_CAP_W1(gen_vla_result, IR_OP_VLA_RESULT)
#endif
#ifdef MCC_IR_HAVE_MULH
IR_CAP_W1(gen_mulh, IR_OP_MULH)
#endif
#if MCC_HAVE_INT128
IR_CAP_W0(gen_mul_widen, IR_OP_MULWIDEN)
#endif
#ifdef MCC_IR_HAVE_FABS_SQRT
IR_CAP_W0(gen_fabs, IR_OP_FABS)
IR_CAP_W0(gen_sqrt, IR_OP_SQRT)
#endif
#ifdef MCC_IR_HAVE_ROUND
IR_CAP_W1(gen_round, IR_OP_ROUND)
#endif
#ifdef MCC_IR_HAVE_COPYSIGN
IR_CAP_W0(gen_copysign, IR_OP_COPYSIGN)
#endif
#ifdef MCC_IR_HAVE_BSWAP
IR_CAP_W1(gen_bswap, IR_OP_BSWAP)
#endif
#ifdef MCC_IR_HAVE_X86_PRIMS
IR_CAP_W1(gen_signbit, IR_OP_SIGNBIT)
IR_CAP_W1(gen_ffs, IR_OP_FFS)
IR_CAP_W2(gen_bitscan, IR_OP_BITSCAN)
#endif
IR_CAP_W2(gen_bit_builtin, IR_OP_BITBUILTIN)
IR_CAP_W0(gen_trap, IR_OP_TRAP)
#ifdef MCC_IR_HAVE_X86_PRIMS
IR_CAP_W1(gen_atomic_cmpxchg, IR_OP_ATOMIC_CMPXCHG)
IR_CAP_W1(gen_atomic_xchg, IR_OP_ATOMIC_XCHG)
IR_CAP_W1(gen_atomic_xadd, IR_OP_ATOMIC_XADD)
#endif
IR_CAP_W1(gen_asan_shadow_check, IR_OP_ASAN_SHADOW)
IR_CAP_W0(gen_asan_mark_write, IR_OP_ASAN_MARK_WRITE)
IR_CAP_W0(gen_ubsan_nullptr, IR_OP_UBSAN_NULLPTR)
#ifdef MCC_IR_HAVE_XFERRET
IR_CAP_W1(arch_transfer_ret_regs, IR_OP_XFERRET)
#endif
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
IR_CAP_W0(gen_x87_pop, IR_OP_X87POP)
#endif

static void ir_cap_vsetc(CType *type, int r, CValue *vc) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	int lit = (r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
	ir_cap_begin(lit ? IR_OP_PUSHLIT : IR_OP_VSETC, NULL);
	if (IR_CAP_REC) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		ir_cap_pending->ctype = *type;
		ir_cap_pending->a0 = r;
		ir_cap_pending->cval = *vc;
	}
	(vsetc)(type, r, vc);
	ir_cap_end();
}

IR_CAP_W0(vswap, IR_OP_VSWAP)
IR_CAP_W0(vpop, IR_OP_VPOP)
IR_CAP_W1(vrotb, IR_OP_VROTB)
IR_CAP_W1(vrott, IR_OP_VROTT)
IR_CAP_W1(vrev, IR_OP_VREV)

void ir_cap_vstore(void) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	int keep = (vtop[-1].type.t & VT_BTYPE) != VT_STRUCT &&
						 (vtop->type.t & VT_BTYPE) != VT_STRUCT &&
						 !((vtop[-1].type.t | vtop->type.t) & VT_ARRAY) &&
						 !(vtop[-1].type.t & VT_BITFIELD);
	if (!keep) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		(vstore)();
		return;
	}
	ir_cap_begin(IR_OP_VSTORE, NULL);
	(vstore)();
	ir_cap_end();
}

IR_CAP_W1(gen_op, IR_OP_GENOP)
IR_CAP_W0(gaddrof, IR_OP_ADDROF)

#ifdef MCC_IR_HAVE_GFUNC_RETURN
void ir_cap_gfunc_return(CType *func_type) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	ir_cap_begin(IR_OP_RETVAL, NULL);
	(gfunc_return)(func_type);
	ir_cap_end();
}
#endif
#ifdef MCC_IR_HAVE_VA_START
IR_CAP_W0(gen_va_start, IR_OP_VA_START)
#endif
void ir_cap_asm_gen_code(ASMOperand *operands, int nb_operands, int nb_outputs,
											int nb_labels, int eff, int is_output,
											uint8_t *clobber_regs, int out_reg) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	ir_cap_begin(IR_OP_ASMGEN, NULL);
	if (IR_CAP_REC) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		int hdr[4];
		int nall = nb_operands + (nb_labels > 0 ? nb_labels : 0);
		int opbytes = (int)((size_t)nall * sizeof *operands);
		hdr[0] = nb_operands;
		hdr[1] = nb_outputs;
		hdr[2] = nb_labels > 0 ? nb_labels : 0;
		hdr[3] = eff;
		ir_cap_pending->raw_off =
				ir_cap_raw_add((const unsigned char *)hdr, (int)sizeof hdr);
		ir_cap_raw_add((const unsigned char *)operands, opbytes);
		ir_cap_raw_add(clobber_regs, MCC_NB_ASM_REGS);
		ir_cap_pending->raw_len = (int)sizeof hdr + opbytes + MCC_NB_ASM_REGS;
		ir_cap_pending->a0 = is_output;
		ir_cap_pending->a1 = out_reg;
	}
	asm_gen_code(operands, nb_operands, nb_outputs, is_output, clobber_regs,
							 out_reg);
	ir_cap_end();
}

void ir_cap_asm(const char *str, int len, int global) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	Section *sec0 = cur_text_section;
	int ind0, rel0;
	ir_cap_begin(IR_OP_ASM, NULL);
	ind0 = ind;
	rel0 = ir_cap_relofs();
	mcc_assemble_inline(mcc_state, str, len, global);
	if (IR_CAP_REC) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		int clen = cur_text_section == sec0 ? ind - ind0 : -1;
		int rlen = cur_text_section == sec0 ? ir_cap_relofs() - rel0 : -1;
		if (clen < 0 || rlen < 0) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
			ir_cap_bad = 1;
			clen = rlen = 0;
		}
		ir_cap_asm_n++;
		ir_cap_pending->a0 = global;
		ir_cap_pending->raw_len = clen;
		if (clen > 0)
			{ MCC_TRACE_WHEN(ir_cap_active, "br\n");
				ir_cap_pending->raw_off = ir_cap_raw_add(sec0->data + ind0, clen); }
		ir_cap_pending->rawrel_len = rlen;
		if (rlen > 0)
			{ MCC_TRACE_WHEN(ir_cap_active, "br\n");
				ir_cap_pending->rawrel_off =
						ir_cap_raw_add(sec0->reloc->data + rel0, rlen); }
	}
	ir_cap_end();
}

#ifdef MCC_IR_HAVE_VA_ARG
void ir_cap_gen_va_arg(CType *t) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	if (t->t & VT_ARRAY) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		(gen_va_arg)(t);
		return;
	}
	ir_cap_begin(IR_OP_VA_ARG, NULL);
	if (IR_CAP_REC)
		ir_cap_pending->ctype = *t;
	(gen_va_arg)(t);
	ir_cap_end();
}
#endif

void ir_cap_mk_pointer(CType *type) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	if (type != &vtop->type) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		(mk_pointer)(type);
		return;
	}
	ir_cap_begin(IR_OP_MKPTR, NULL);
	(mk_pointer)(type);
	if (IR_CAP_REC)
		{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); ir_cap_pending->ctype = *type; }
	ir_cap_end();
}

void ir_cap_vpushv(SValue *v) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	ir_cap_begin(IR_OP_VPUSHV, v);
	(vpushv)(v);
	ir_cap_end();
}

void ir_cap_vpushsym(CType *type, Sym *sym) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	ir_cap_begin(IR_OP_VPUSHSYM, NULL);
	if (IR_CAP_REC) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		ir_cap_pending->ctype = *type;
		ir_cap_pending->sym = sym;
	}
	(vpushsym)(type, sym);
	ir_cap_end();
}

int ir_cap_gv(int rc) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	int rv;
	ir_cap_begin(IR_OP_GV, NULL);
	if (IR_CAP_REC) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		ir_cap_pending->a0 = rc;
		ir_cap_pending->d64 = (int64_t)ast_pinned_regs;
	}
	rv = (gv)(rc);
	if (IR_CAP_REC)
		{ MCC_TRACE_WHEN(ir_cap_active, "br\n"); ir_cap_pending->a1 = rv; }
	ir_cap_end();
	return rv;
}

int ir_cap_gen_cmov(int rt, int rf, int rb, int ll) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	int rv;
	ir_cap_begin(IR_OP_CMOV, NULL);
	if (IR_CAP_REC) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		ir_cap_pending->a0 = rt;
		ir_cap_pending->a1 = rf;
		ir_cap_pending->a2 = rb;
		ir_cap_pending->a3 = ll;
	}
	rv = (gen_cmov)(rt, rf, rb, ll);
	ir_cap_end();
	return rv;
}

#ifdef MCC_IR_HAVE_REGADDI
void ir_cap_gen_reg_addi(int r, int64_t d) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	ir_cap_begin(IR_OP_REGADDI, NULL);
	if (IR_CAP_REC) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		ir_cap_pending->a0 = r;
		ir_cap_pending->d64 = d;
	}
	(gen_reg_addi)(r, d);
	ir_cap_end();
}
#endif

void ir_cap_gen_vla_alloc(CType *type, int align) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	ir_cap_begin(IR_OP_VLA_ALLOC, NULL);
	if (IR_CAP_REC) { MCC_TRACE_WHEN(ir_cap_active, "br\n");
		ir_cap_pending->ctype = *type;
		ir_cap_pending->a0 = align;
	}
	(gen_vla_alloc)(type, align);
	ir_cap_end();
}

void ir_cap_gen_increment_tcov(SValue *sv) { MCC_TRACE_WHEN(ir_cap_active, "enter\n");
	ir_cap_begin(IR_OP_TCOV, sv);
	(gen_increment_tcov)(sv);
	ir_cap_end();
}

static void ir_cap_issue(IrCapOp *o) { MCC_TRACE("enter\n");
	SValue tmp;
	SValue *p;
	tmp = o->svarg;
	p = o->sv_slot >= 0 ? &vstack[o->sv_slot] : &tmp;
	switch (o->kind) { MCC_TRACE("br\n");
	case IR_OP_RAW:
		if (o->raw_len) { MCC_TRACE("br\n");
			if (ind + o->raw_len > cur_text_section->data_allocated)
				{ MCC_TRACE("br\n"); section_realloc(cur_text_section, ind + o->raw_len); }
			memcpy(cur_text_section->data + ind, ir_cap_raw + o->raw_off,
						 (size_t)o->raw_len);
			ind += o->raw_len;
		}
		if (o->rawrel_len && cur_text_section->reloc) { MCC_TRACE("br\n");
			void *rp = section_ptr_add(cur_text_section->reloc, o->rawrel_len);
			memcpy(rp, ir_cap_raw + o->rawrel_off, (size_t)o->rawrel_len);
		}
		break;
	case IR_OP_LOAD: (load)(o->a0, p); break;
	case IR_OP_STORE: (store)(o->a0, p); break;
	case IR_OP_OPI: (gen_opi)(o->a0); break;
	case IR_OP_OPL: (gen_opl)(o->a0); break;
	case IR_OP_OPF: (gen_opf)(o->a0); break;
	case IR_OP_CALL: (gfunc_call)(o->a0); break;
	case IR_OP_JMP: (void)(gjmp)(o->a0); break;
	case IR_OP_JMPADDR: (gjmp_addr)(o->a0); break;
	case IR_OP_JMPCOND: (void)(gjmp_cond)(o->a0, o->a1); break;
	case IR_OP_JMPAPPEND: (void)(gjmp_append)(o->a0, o->a1); break;
	case IR_OP_GSYMADDR: (gsym_addr)(o->a0, o->a1); break;
	case IR_OP_CVT_ITOF: (gen_cvt_itof)(o->a0); break;
	case IR_OP_CVT_FTOF: (gen_cvt_ftof)(o->a0); break;
	case IR_OP_CVT_FTOI: (gen_cvt_ftoi)(o->a0); break;
#ifdef MCC_IR_HAVE_CVT_SXTW
	case IR_OP_CVT_SXTW: (gen_cvt_sxtw)(); break;
#endif
#ifdef MCC_IR_HAVE_CVT_ZXTW
	case IR_OP_CVT_ZXTW: (gen_cvt_zxtw)(); break;
#endif
#ifdef MCC_IR_HAVE_X86_PRIMS
	case IR_OP_CVT_TRUNC32: (gen_cvt_trunc32)(); break;
#endif
#ifdef MCC_IR_HAVE_CVT_CSTI
	case IR_OP_CVT_CSTI: (gen_cvt_csti)(o->a0); break;
#endif
#ifdef MCC_IR_HAVE_STRUCT_COPY
	case IR_OP_STRUCTCOPY: (gen_struct_copy)(o->a0); break;
#endif
	case IR_OP_GGOTO: (ggoto)(); break;
	case IR_OP_CMOV: (void)(gen_cmov)(o->a0, o->a1, o->a2, o->a3); break;
	case IR_OP_FILLNOPS: (gen_fill_nops)(o->a0); break;
	case IR_OP_VLA_SPSAVE: (gen_vla_sp_save)(o->a0); break;
	case IR_OP_VLA_SPREST: (gen_vla_sp_restore)(o->a0); break;
#ifdef MCC_IR_HAVE_VLA_RESULT
	case IR_OP_VLA_RESULT: (gen_vla_result)(o->a0); break;
#endif
	case IR_OP_VLA_ALLOC: (gen_vla_alloc)(&o->ctype, o->a0); break;
#ifdef MCC_IR_HAVE_MULH
	case IR_OP_MULH: (gen_mulh)(o->a0); break;
#endif
#if MCC_HAVE_INT128
	case IR_OP_MULWIDEN: (gen_mul_widen)(); break;
#endif
#ifdef MCC_IR_HAVE_REGADDI
	case IR_OP_REGADDI: (gen_reg_addi)(o->a0, o->d64); break;
#endif
#ifdef MCC_IR_HAVE_FABS_SQRT
	case IR_OP_FABS: (gen_fabs)(); break;
	case IR_OP_SQRT: (gen_sqrt)(); break;
#endif
#ifdef MCC_IR_HAVE_ROUND
	case IR_OP_ROUND: (gen_round)(o->a0); break;
#endif
#ifdef MCC_IR_HAVE_COPYSIGN
	case IR_OP_COPYSIGN: (gen_copysign)(); break;
#endif
#ifdef MCC_IR_HAVE_BSWAP
	case IR_OP_BSWAP: (gen_bswap)(o->a0); break;
#endif
#ifdef MCC_IR_HAVE_X86_PRIMS
	case IR_OP_SIGNBIT: (gen_signbit)(o->a0); break;
	case IR_OP_FFS: (gen_ffs)(o->a0); break;
	case IR_OP_BITSCAN: (gen_bitscan)(o->a0, o->a1); break;
#endif
	case IR_OP_BITBUILTIN: (gen_bit_builtin)(o->a0, o->a1); break;
	case IR_OP_TRAP: (gen_trap)(); break;
	case IR_OP_TCOV: (gen_increment_tcov)(p); break;
#ifdef MCC_IR_HAVE_X86_PRIMS
	case IR_OP_ATOMIC_CMPXCHG: (gen_atomic_cmpxchg)(o->a0); break;
	case IR_OP_ATOMIC_XCHG: (gen_atomic_xchg)(o->a0); break;
	case IR_OP_ATOMIC_XADD: (gen_atomic_xadd)(o->a0); break;
#endif
	case IR_OP_ASAN_SHADOW: (gen_asan_shadow_check)(o->a0); break;
	case IR_OP_ASAN_MARK_WRITE: (gen_asan_mark_write)(); break;
	case IR_OP_UBSAN_NULLPTR: (gen_ubsan_nullptr)(); break;
#ifdef MCC_IR_HAVE_XFERRET
	case IR_OP_XFERRET: (arch_transfer_ret_regs)(o->a0); break;
#endif
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
	case IR_OP_X87POP: (gen_x87_pop)(); break;
#endif
	case IR_OP_VSETC:
	case IR_OP_PUSHLIT: { MCC_TRACE("br\n");
		CValue cv = o->cval;
		(vsetc)(&o->ctype, o->a0, &cv);
		break;
	}
	case IR_OP_VPUSHSYM: (vpushsym)(&o->ctype, o->sym); break;
	case IR_OP_VPUSHV: (vpushv)(p); break;
	case IR_OP_VSWAP: (vswap)(); break;
	case IR_OP_VPOP: (vpop)(); break;
	case IR_OP_VROTB: (vrotb)(o->a0); break;
	case IR_OP_VROTT: (vrott)(o->a0); break;
	case IR_OP_VREV: (vrev)(o->a0); break;
	case IR_OP_VSTORE: (vstore)(); break;
	case IR_OP_GENOP: (gen_op)(o->a0); break;
	case IR_OP_MKPTR:
		if (vtop < vstack) { MCC_TRACE("br\n");
			ir_cap_bad = 1;
			break;
		}
		vtop->type = o->ctype;
		break;
	case IR_OP_ADDROF: (gaddrof)(); break;
#ifdef MCC_IR_HAVE_GFUNC_RETURN
	case IR_OP_RETVAL: (gfunc_return)(&func_vt); break;
#endif
#ifdef MCC_IR_HAVE_VA_START
	case IR_OP_VA_START: (gen_va_start)(); break;
#endif
#ifdef MCC_IR_HAVE_VA_ARG
	case IR_OP_VA_ARG: (gen_va_arg)(&o->ctype); break;
#endif
	case IR_OP_ASMGEN: { MCC_TRACE("br\n");
		ASMOperand ops[MAX_ASM_OPERANDS];
		uint8_t cr[MCC_NB_ASM_REGS];
		const unsigned char *p = ir_cap_raw + o->raw_off;
		int hdr[4], nall;
		memcpy(hdr, p, sizeof hdr);
		p += sizeof hdr;
		nall = hdr[0] + hdr[2];
		if (nall > 0)
			{ MCC_TRACE("br\n"); memcpy(ops, p, (size_t)nall * sizeof *ops); }
		p += (size_t)nall * sizeof *ops;
		memcpy(cr, p, sizeof cr);
		asm_gen_code(ops, hdr[0], hdr[1], o->a0, cr, o->a1);
		break;
	}
	case IR_OP_ASM:
		if (o->raw_len > 0) { MCC_TRACE("br\n");
			if (ind + o->raw_len > cur_text_section->data_allocated)
				{ MCC_TRACE("br\n"); section_realloc(cur_text_section, ind + o->raw_len); }
			memcpy(cur_text_section->data + ind, ir_cap_raw + o->raw_off,
						 (size_t)o->raw_len);
			ind += o->raw_len;
		}
		if (o->rawrel_len > 0 && cur_text_section->reloc) { MCC_TRACE("br\n");
			void *rp = section_ptr_add(cur_text_section->reloc, o->rawrel_len);
			memcpy(rp, ir_cap_raw + o->rawrel_off, (size_t)o->rawrel_len);
		}
		break;
	case IR_OP_GV: { MCC_TRACE("br\n");
		uint64_t pin = ast_pinned_regs;
		ast_pinned_regs = (uint64_t)o->d64;
		(void)(gv)(o->a0);
		ast_pinned_regs = pin;
		break;
	}
	default: break;
	}
}

static void ir_cap_reset(void) { MCC_TRACE("enter\n");
	ir_cap_n = 0;
	ir_cap_vsn = 0;
	ir_cap_rawn = 0;
	ir_cap_fcn = 0;
	ir_cap_bad = 0;
	ir_cap_depth = 0;
	ir_cap_pending = NULL;
	ir_cap_ind_wm = ind;
	ir_cap_rel_wm = ir_cap_relofs();
}

#pragma pop_macro("gjmp")
#pragma pop_macro("gjmp_addr")

#endif
