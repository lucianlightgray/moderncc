#ifndef MCC_WIDEINT_SLICE_H
#define MCC_WIDEINT_SLICE_H

#define MCC_W256_LIMBS MCC_WIDE256_LIMBS
#include "wide256_arith.h"
#include "wide512_arith.h"

typedef mcc_w256_limb wilimb;

static void wi_add(int nl, wilimb *r, const wilimb *a, const wilimb *b) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_add(r, a, b); else mcc_w512_add(r, a, b);
}
static void wi_sub(int nl, wilimb *r, const wilimb *a, const wilimb *b) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_sub(r, a, b); else mcc_w512_sub(r, a, b);
}
static void wi_mul(int nl, wilimb *r, const wilimb *a, const wilimb *b) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_mul(r, a, b); else mcc_w512_mul(r, a, b);
}
static void wi_and(int nl, wilimb *r, const wilimb *a, const wilimb *b) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_and(r, a, b); else mcc_w512_and(r, a, b);
}
static void wi_or(int nl, wilimb *r, const wilimb *a, const wilimb *b) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_or(r, a, b); else mcc_w512_or(r, a, b);
}
static void wi_xor(int nl, wilimb *r, const wilimb *a, const wilimb *b) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_xor(r, a, b); else mcc_w512_xor(r, a, b);
}
static void wi_shl(int nl, wilimb *r, const wilimb *a, unsigned int n) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_shl(r, a, n); else mcc_w512_shl(r, a, n);
}
static void wi_shr(int nl, wilimb *r, const wilimb *a, unsigned int n) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_shr(r, a, n); else mcc_w512_shr(r, a, n);
}
static void wi_sar(int nl, wilimb *r, const wilimb *a, unsigned int n) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_sar(r, a, n); else mcc_w512_sar(r, a, n);
}
static int wi_ucmp(int nl, const wilimb *a, const wilimb *b) { MCC_TRACE("enter\n");
	return nl <= 4 ? mcc_w256_ucmp(a, b) : mcc_w512_ucmp(a, b);
}
static int wi_scmp(int nl, const wilimb *a, const wilimb *b) { MCC_TRACE("enter\n");
	return nl <= 4 ? mcc_w256_scmp(a, b) : mcc_w512_scmp(a, b);
}
static int wi_is_zero(int nl, const wilimb *a) { MCC_TRACE("enter\n");
	return nl <= 4 ? mcc_w256_is_zero(a) : mcc_w512_is_zero(a);
}
static void wi_zero(int nl, wilimb *r) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_zero(r); else mcc_w512_zero(r);
}
static void wi_copy(int nl, wilimb *r, const wilimb *a) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_copy(r, a); else mcc_w512_copy(r, a);
}
static void wi_udivmod(int nl, wilimb *q, wilimb *rem, const wilimb *a,
											 const wilimb *b) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_udivmod(q, rem, a, b); else mcc_w512_udivmod(q, rem, a, b);
}
static void wi_sdivmod(int nl, wilimb *q, wilimb *rem, const wilimb *a,
											 const wilimb *b) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_sdivmod(q, rem, a, b); else mcc_w512_sdivmod(q, rem, a, b);
}
static void wi_from_u64(int nl, wilimb *r, uint64_t v) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_from_u64(r, v); else mcc_w512_from_u64(r, v);
}
static void wi_from_i64(int nl, wilimb *r, uint64_t v) { MCC_TRACE("enter\n");
	if (nl <= 4) mcc_w256_from_i64(r, v); else mcc_w512_from_i64(r, v);
}

static int wideint_bin_helper(int op, int uns, int nl) { MCC_TRACE("enter\n");
	if (nl <= 4) { MCC_TRACE("br\n");
		switch (op) { MCC_TRACE("br\n");
		case '+': return TOK___mcc_i256_add;
		case '-': return TOK___mcc_i256_sub;
		case '*': return TOK___mcc_i256_mul;
		case '&': return TOK___mcc_i256_and;
		case '|': return TOK___mcc_i256_or;
		case '^': return TOK___mcc_i256_xor;
		case TOK_UDIV: return TOK___mcc_i256_udiv;
		case TOK_UMOD: return TOK___mcc_i256_umod;
		case '/':
		case TOK_PDIV: return uns ? TOK___mcc_i256_udiv : TOK___mcc_i256_sdiv;
		}
		return uns ? TOK___mcc_i256_umod : TOK___mcc_i256_smod;
	}
	switch (op) { MCC_TRACE("br\n");
	case '+': return TOK___mcc_i512_add;
	case '-': return TOK___mcc_i512_sub;
	case '*': return TOK___mcc_i512_mul;
	case '&': return TOK___mcc_i512_and;
	case '|': return TOK___mcc_i512_or;
	case '^': return TOK___mcc_i512_xor;
	case TOK_UDIV: return TOK___mcc_i512_udiv;
	case TOK_UMOD: return TOK___mcc_i512_umod;
	case '/':
	case TOK_PDIV: return uns ? TOK___mcc_i512_udiv : TOK___mcc_i512_sdiv;
	}
	return uns ? TOK___mcc_i512_umod : TOK___mcc_i512_smod;
}

static int wideint_shift_helper(int op, int uns, int nl) { MCC_TRACE("enter\n");
	if (nl <= 4)
		return op == TOK_SHL ? TOK___mcc_i256_shl
				 : (uns || op == TOK_SHR) ? TOK___mcc_i256_shr : TOK___mcc_i256_sar;
	return op == TOK_SHL ? TOK___mcc_i512_shl
			 : (uns || op == TOK_SHR) ? TOK___mcc_i512_shr : TOK___mcc_i512_sar;
}

static int wideint_cmp_helper(int uns, int nl) { MCC_TRACE("enter\n");
	if (nl <= 4) return uns ? TOK___mcc_i256_ucmp : TOK___mcc_i256_scmp;
	return uns ? TOK___mcc_i512_ucmp : TOK___mcc_i512_scmp;
}

static int wideint_tof_helper(int use_f32, int nl) { MCC_TRACE("enter\n");
	if (nl <= 4) return use_f32 ? TOK___mcc_i256_to_f32 : TOK___mcc_i256_to_f64;
	return use_f32 ? TOK___mcc_i512_to_f32 : TOK___mcc_i512_to_f64;
}

static int wideint_fromf_helper(int use_f32, int nl) { MCC_TRACE("enter\n");
	if (nl <= 4) return use_f32 ? TOK___mcc_i256_from_f32 : TOK___mcc_i256_from_f64;
	return use_f32 ? TOK___mcc_i512_from_f32 : TOK___mcc_i512_from_f64;
}

static CType wideint_u64_type(void) { MCC_TRACE("enter\n");
	CType t;
	t.t = VT_LLONG | VT_UNSIGNED;
	t.bp = t.bs = 0;
	t.ref = NULL;
	return t;
}

static CType wideint_s64_type(void) { MCC_TRACE("enter\n");
	CType t;
	t.t = VT_LLONG;
	t.bp = t.bs = 0;
	t.ref = NULL;
	return t;
}

static int wideint_nlimbs(CType *type) { MCC_TRACE("enter\n");
	int align;
	return type_size(type, &align) / 8;
}

static void mk_wideint_type(CType *type, int is_unsigned, int nl) { MCC_TRACE("enter\n");
	Sym *s, *f, *prev = NULL;
	AttributeDef ad;
	int i, idx = (nl == 8 ? 2 : 0) + (is_unsigned ? 1 : 0);
	int lt = VT_LLONG | (is_unsigned ? VT_UNSIGNED : 0);

	if (mcc_state->gen_wideint_type_cache[idx].ref) { MCC_TRACE("br\n");
		*type = mcc_state->gen_wideint_type_cache[idx];
		return;
	}
	if (!mcc_state->gen_wideint_limb_tok[0]) { MCC_TRACE("br\n");
		for (i = 0; i < MCC_WIDE512_LIMBS; i++) { MCC_TRACE("br\n");
			char nm[16];
			snprintf(nm, sizeof nm, "__w%d", i);
			mcc_state->gen_wideint_limb_tok[i] = tok_alloc(nm, (int)strlen(nm))->tok;
		}
	}
	s = sym_push2(&global_stack, anon_sym++ | SYM_STRUCT, VT_STRUCT, -1);
	s->r = 0;
	s->a.is_wideint = 1;
	s->next = NULL;
	for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
		f = sym_push2(&global_stack,
									mcc_state->gen_wideint_limb_tok[i] | SYM_FIELD, lt, 0);
		f->type.ref = NULL;
		f->next = NULL;
		if (prev)
			{ MCC_TRACE("br\n"); prev->next = f; }
		else
			{ MCC_TRACE("br\n"); s->next = f; }
		prev = f;
	}
	type->t = VT_STRUCT;
	type->bp = type->bs = 0;
	type->ref = s;
	memset(&ad, 0, sizeof ad);
	ad.a.aligned = exact_log2p1(MCC_MAX_ALIGN >= 16 ? 16 : MCC_MAX_ALIGN);
	struct_layout(type, &ad);
	mcc_state->gen_wideint_type_cache[idx] = *type;
	if (mcc_state->gen_wideint_type_cache_n < idx + 1)
		{ MCC_TRACE("br\n"); mcc_state->gen_wideint_type_cache_n = idx + 1; }
}

static int wideint_sv_is_const(SValue *sv) { MCC_TRACE("enter\n");
	return (sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
}

static void wideint_get_const(SValue *sv, wilimb *w, int nl) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < nl; i++)
		{ MCC_TRACE("br\n"); w[i] = (&sv->c.q.lo)[i]; }
}

static void vpush_wideint_const(CType *vt, const wilimb *w, int nl) { MCC_TRACE("enter\n");
	CValue cv;
	int i;
	memset(&cv, 0, sizeof cv);
	for (i = 0; i < nl; i++)
		{ MCC_TRACE("br\n"); (&cv.q.lo)[i] = w[i]; }
	vsetc(vt, VT_CONST, &cv);
}

static int wideint_sv_is_stable_lval(SValue *sv) { MCC_TRACE("enter\n");
	if (!(sv->r & VT_LVAL) || (sv->r & VT_REGDISP))
		{ MCC_TRACE("br\n"); return 0; }
	if ((sv->r & VT_VALMASK) == VT_LOCAL && !sv->sym)
		{ MCC_TRACE("br\n"); return 1; }
	if ((sv->r & VT_VALMASK) == VT_CONST && (sv->r & VT_SYM))
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

static void wideint_local(CType *vt, SValue *out) { MCC_TRACE("enter\n");
	int align, size = type_size(vt, &align);

	loc = ast_alloc_loc(size, align);
	out->type = *vt;
	out->type.t &= ~VT_QUALIFY;
	out->r = VT_LOCAL | VT_LVAL;
	out->r2 = VT_CONST;
	out->c.i = loc;
	out->c.q.hi = out->c.q.w2 = out->c.q.w3 = 0;
	out->sym = NULL;
}

static void wideint_scratch_u64(SValue *out) { MCC_TRACE("enter\n");
	CType u64 = wideint_u64_type();
	int al, sz = type_size(&u64, &al);
	out->type = u64;
	out->r = VT_LOCAL | VT_LVAL;
	out->r2 = VT_CONST;
	out->c.i = ast_alloc_loc(sz, al);
	out->c.q.hi = out->c.q.w2 = out->c.q.w3 = 0;
	out->sym = NULL;
}

static void wideint_limb_lval(SValue *base, int i, int is_unsigned) { MCC_TRACE("enter\n");
	CType lt = is_unsigned ? wideint_u64_type() : wideint_s64_type();

	vpushv(base);
	test_lvalue();
	gaddrof();
	vtop->type = char_pointer_type;
	vpushi(i * 8);
	gen_op('+');
	vtop->type = lt;
	vtop->r |= VT_LVAL;
}

static void wideint_push_ptr(SValue *sv) { MCC_TRACE("enter\n");
	vpushv(sv);
	test_lvalue();
	mk_pointer(&vtop->type);
	gaddrof();
}

static void wideint_store_int(SValue *out, int src_unsigned, int nl) { MCC_TRACE("enter\n");
	int i;

	wideint_limb_lval(out, 0, 1);
	vswap();
	vstore();
	vpop();
	for (i = 1; i < nl; i++) { MCC_TRACE("br\n");
		wideint_limb_lval(out, i, 1);
		if (src_unsigned) { MCC_TRACE("br\n");
			vpush64(VT_LLONG | VT_UNSIGNED, 0);
		} else { MCC_TRACE("br\n");
			wideint_limb_lval(out, 0, 0);
			vpushi(63);
			gen_op(TOK_SAR);
		}
		vstore();
		vpop();
	}
}

static void wideint_materialize(CType *vt, SValue *out) { MCC_TRACE("enter\n");
	CType lt = *vt;
	int i, nl = wideint_nlimbs(vt);

	if (is_wideint_type(&vtop->type) && wideint_sv_is_stable_lval(vtop)) { MCC_TRACE("br\n");
		*out = *vtop;
		vpop();
		return;
	}
	if (is_wideint_type(&vtop->type) && wideint_sv_is_const(vtop)) { MCC_TRACE("br\n");
		wilimb w[MCC_WIDE512_LIMBS];
		wideint_get_const(vtop, w, nl);
		vpop();
		wideint_local(&lt, out);
		for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
			wideint_limb_lval(out, i, 1);
			vpush64(VT_LLONG | VT_UNSIGNED, w[i]);
			vstore();
			vpop();
		}
		return;
	}
	wideint_local(&lt, out);
	vpushv(out);
	vswap();
	vstore();
	vpop();
}

static void wideint_deconst(void) { MCC_TRACE("enter\n");
	SValue res;
	CType vt;

	if ((vtop->type.t & VT_BTYPE) != VT_STRUCT || (vtop->r & VT_LVAL))
		{ MCC_TRACE("br\n"); return; }
	if (!is_wideint_type(&vtop->type) || !wideint_sv_is_const(vtop))
		{ MCC_TRACE("br\n"); return; }
	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); return; }
	vt = vtop->type;
	wideint_materialize(&vt, &res);
	vpushv(&res);
}

static void wideint_addrof_top(CType *vt) { MCC_TRACE("enter\n");
	SValue m;

	wideint_deconst();
	if (!(vtop->r & VT_LVAL)) { MCC_TRACE("br\n");
		wideint_materialize(vt, &m);
		vpushv(&m);
	}
	test_lvalue();
	mk_pointer(&vtop->type);
	gaddrof();
}

static int wideint_op_ok(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
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
	case TOK_PDIV:
	case TOK_UDIV:
	case TOK_UMOD:
	case TOK_EQ:
	case TOK_NE:
	case TOK_LT:
	case TOK_GT:
	case TOK_LE:
	case TOK_GE:
	case TOK_ULT:
	case TOK_UGT:
	case TOK_ULE:
	case TOK_UGE:
		return 1;
	}
	return 0;
}

static int wideint_cmp_result(int op, int c) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case TOK_EQ:
		return c == 0;
	case TOK_NE:
		return c != 0;
	case TOK_LT:
	case TOK_ULT:
		return c < 0;
	case TOK_LE:
	case TOK_ULE:
		return c <= 0;
	case TOK_GT:
	case TOK_UGT:
		return c > 0;
	}
	return c >= 0;
}

static int wideint_cmp_signed_tok(int op) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case TOK_ULT:
		return TOK_LT;
	case TOK_UGT:
		return TOK_GT;
	case TOK_ULE:
		return TOK_LE;
	case TOK_UGE:
		return TOK_GE;
	}
	return op;
}

static void wideint_fold_bin(int op, int uns, int nl, const wilimb *wa,
														 const wilimb *wb, wilimb *wr) { MCC_TRACE("enter\n");
	wilimb q[MCC_WIDE512_LIMBS], rem[MCC_WIDE512_LIMBS];
	int udiv = (op == TOK_UDIV || op == TOK_UMOD) || uns;

	switch (op) { MCC_TRACE("br\n");
	case '+':
		wi_add(nl, wr, wa, wb);
		return;
	case '-':
		wi_sub(nl, wr, wa, wb);
		return;
	case '*':
		wi_mul(nl, wr, wa, wb);
		return;
	case '&':
		wi_and(nl, wr, wa, wb);
		return;
	case '|':
		wi_or(nl, wr, wa, wb);
		return;
	case '^':
		wi_xor(nl, wr, wa, wb);
		return;
	}
	if (wi_is_zero(nl, wb))
		{ MCC_TRACE("br\n"); mcc_error("division by zero in constant"); }
	if (udiv)
		{ MCC_TRACE("br\n"); wi_udivmod(nl, q, rem, wa, wb); }
	else
		{ MCC_TRACE("br\n"); wi_sdivmod(nl, q, rem, wa, wb); }
	if (op == '%' || op == TOK_UMOD)
		{ MCC_TRACE("br\n"); wi_copy(nl, wr, rem); }
	else
		{ MCC_TRACE("br\n"); wi_copy(nl, wr, q); }
}

static void gen_wideint_op(int op) { MCC_TRACE("enter\n");
	CType wt;
	SValue res;
	wilimb wa[MCC_WIDE512_LIMBS], wb[MCC_WIDE512_LIMBS];
	wilimb wr[MCC_WIDE512_LIMBS];
	int uns, cmp, shift, lw, rw, nl;

	vcheck_cmp();
	vswap();
	vcheck_cmp();
	vswap();

	shift = (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR);
	cmp = TOK_ISCOND(op);
	lw = is_wideint_type(&vtop[-1].type);
	rw = is_wideint_type(&vtop[0].type);

	if (shift && !lw) { MCC_TRACE("br\n");
		gen_cast_s(VT_INT);
		gen_op(op);
		return;
	}
	if (!wideint_op_ok(op))
		{ MCC_TRACE("br\n"); mcc_error("operator is not defined on '__int256'"); }

	nl = wideint_nlimbs(lw ? &vtop[-1].type : &vtop[0].type);

	if (shift) { MCC_TRACE("br\n");
		CType ct = wideint_s64_type();
		wt = vtop[-1].type;
		uns = wideint_is_unsigned(&wt);
		gen_cast(&ct);
	} else { MCC_TRACE("br\n");
		if (lw && rw) { MCC_TRACE("br\n");
			uns = wideint_is_unsigned(&vtop[-1].type) ||
						wideint_is_unsigned(&vtop[0].type);
		} else { MCC_TRACE("br\n");
			CType *o = lw ? &vtop[0].type : &vtop[-1].type;
			int obt = o->t & VT_BTYPE;
			if (is_float(o->t) || !is_integer_btype(obt) || obt == VT_PTR)
				{ MCC_TRACE("br\n"); mcc_error("invalid operand types for an '__int256' "
													"operation"); }
			uns = wideint_is_unsigned(lw ? &vtop[-1].type : &vtop[0].type);
		}
		if (op == TOK_ULT || op == TOK_UGT || op == TOK_ULE || op == TOK_UGE ||
				op == TOK_UDIV || op == TOK_UMOD)
			{ MCC_TRACE("br\n"); uns = 1; }
		mk_wideint_type(&wt, uns, nl);
		gen_cast(&wt);
		vswap();
		gen_cast(&wt);
		vswap();
	}

	if (wideint_sv_is_const(vtop - 1) &&
			(shift ? (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
						 : wideint_sv_is_const(vtop))) { MCC_TRACE("br\n");
		wideint_get_const(vtop - 1, wa, nl);
		if (shift) { MCC_TRACE("br\n");
			long long n = (long long)vtop->c.i;
			vpop();
			vpop();
			if (n < 0 || n >= 64 * nl) { MCC_TRACE("br\n");
				if (op == TOK_SAR && !uns)
					{ MCC_TRACE("br\n"); wi_sar(nl, wr, wa, 64 * nl); }
				else
					{ MCC_TRACE("br\n"); wi_zero(nl, wr); }
			} else if (op == TOK_SHL) { MCC_TRACE("br\n");
				wi_shl(nl, wr, wa, (unsigned int)n);
			} else if (uns || op == TOK_SHR) { MCC_TRACE("br\n");
				wi_shr(nl, wr, wa, (unsigned int)n);
			} else { MCC_TRACE("br\n");
				wi_sar(nl, wr, wa, (unsigned int)n);
			}
			vpush_wideint_const(&wt, wr, nl);
			return;
		}
		wideint_get_const(vtop, wb, nl);
		vpop();
		vpop();
		if (cmp) { MCC_TRACE("br\n");
			int c = uns ? wi_ucmp(nl, wa, wb) : wi_scmp(nl, wa, wb);
			vpushi(wideint_cmp_result(op, c));
			return;
		}
		wideint_fold_bin(op, uns, nl, wa, wb, wr);
		vpush_wideint_const(&wt, wr, nl);
		return;
	}

	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load "
											"time"); }

	if (shift) { MCC_TRACE("br\n");
		int fn = wideint_shift_helper(op, uns, nl);
		vswap();
		wideint_addrof_top(&wt);
		wideint_local(&wt, &res);
		vpush_helper_func(fn);
		wideint_push_ptr(&res);
		vrotb(4);
		vrotb(4);
		vswap();
		gfunc_call(3);
		vpushv(&res);
		return;
	}

	if (op == '&' || op == '|' || op == '^') { MCC_TRACE("br\n");
		SValue av, bv;
		int i;
		wideint_materialize(&wt, &bv);
		wideint_materialize(&wt, &av);
		wideint_local(&wt, &res);
		for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
			wideint_limb_lval(&res, i, 1);
			wideint_limb_lval(&av, i, 1);
			wideint_limb_lval(&bv, i, 1);
			gen_op(op);
			vstore();
			vpop();
		}
		vpushv(&res);
		return;
	}

	if (op == '+' || op == '-') { MCC_TRACE("br\n");
		SValue av, bv, tt, cc, carry;
		int i, sub = (op == '-');
		wideint_materialize(&wt, &bv);
		wideint_materialize(&wt, &av);
		wideint_local(&wt, &res);
		wideint_scratch_u64(&tt);
		wideint_scratch_u64(&cc);
		wideint_scratch_u64(&carry);
		vpushv(&carry);
		vpush64(VT_LLONG | VT_UNSIGNED, 0);
		vstore();
		vpop();
		for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
			vpushv(&tt);
			wideint_limb_lval(&av, i, 1);
			wideint_limb_lval(&bv, i, 1);
			gen_op(op);
			vstore();
			vpop();
			vpushv(&cc);
			if (sub) { MCC_TRACE("br\n");
				wideint_limb_lval(&av, i, 1);
				wideint_limb_lval(&bv, i, 1);
			} else { MCC_TRACE("br\n");
				vpushv(&tt);
				wideint_limb_lval(&av, i, 1);
			}
			gen_op(TOK_ULT);
			vstore();
			vpop();
			wideint_limb_lval(&res, i, 1);
			vpushv(&tt);
			vpushv(&carry);
			gen_op(op);
			vstore();
			vpop();
			vpushv(&carry);
			vpushv(&cc);
			if (sub) { MCC_TRACE("br\n");
				vpushv(&tt);
				wideint_limb_lval(&res, i, 1);
			} else { MCC_TRACE("br\n");
				wideint_limb_lval(&res, i, 1);
				vpushv(&tt);
			}
			gen_op(TOK_ULT);
			gen_op('|');
			vstore();
			vpop();
		}
		vpushv(&res);
		return;
	}

	wideint_addrof_top(&wt);
	vswap();
	wideint_addrof_top(&wt);

	if (cmp) { MCC_TRACE("br\n");
		vpush_helper_func(wideint_cmp_helper(uns, nl));
		vrotb(3);
		vrotb(3);
		vswap();
		gfunc_call(2);
		vpushi(0);
		vtop->r = REG_IRET;
		vpushi(0);
		gen_op(wideint_cmp_signed_tok(op));
		vcheck_cmp();
		return;
	}

	wideint_local(&wt, &res);
	vpush_helper_func(wideint_bin_helper(op, uns, nl));
	wideint_push_ptr(&res);
	vrotb(4);
	vrotb(4);
	vswap();
	gfunc_call(3);
	vpushv(&res);
}

static void gen_wideint_cast(CType *dt) { MCC_TRACE("enter\n");
	CType u64 = wideint_u64_type(), s64 = wideint_s64_type();
	int sw = is_wideint_type(&vtop->type), dw = is_wideint_type(dt);
	int dbt = dt->t & VT_BTYPE;
	int sbt = vtop->type.t & VT_BTYPE;
	int nl = sw ? wideint_nlimbs(&vtop->type) : (dw ? wideint_nlimbs(dt) : 4);

	if (sw && dw) { MCC_TRACE("br\n");
		vtop->type.t = (vtop->type.t & ~VT_BTYPE) | VT_STRUCT;
		vtop->type.ref = dt->ref;
		return;
	}
	if (sw) { MCC_TRACE("br\n");
		SValue a;
		CType st = vtop->type;
		int i;
		if (dbt == VT_VOID) { MCC_TRACE("br\n");
			vtop->type = *dt;
			return;
		}
		if (is_float(dt->t)) { MCC_TRACE("br\n");
			int uns = wideint_is_unsigned(&vtop->type);
			int use_f32 = (dbt == VT_FLOAT);
			if (nocode_wanted & DATA_ONLY_WANTED)
				{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable "
													"at load time"); }
			wideint_addrof_top(&st);
			vpush_helper_func(wideint_tof_helper(use_f32, nl));
			vswap();
			vpushi(uns);
			gfunc_call(2);
			vpushi(0);
			vtop->type.t = use_f32 ? VT_FLOAT : VT_DOUBLE;
			PUT_R_RET(vtop, vtop->type.t);
			if ((dt->t & VT_BTYPE) != (vtop->type.t & VT_BTYPE))
				{ MCC_TRACE("br\n"); gen_cast(dt); }
			return;
		}
		if (!is_integer_btype(dbt) || dbt == VT_PTR)
			{ MCC_TRACE("br\n"); cast_error(&vtop->type, dt); }
		if (wideint_sv_is_const(vtop)) { MCC_TRACE("br\n");
			wilimb w[MCC_WIDE512_LIMBS];
			int nz;
			wideint_get_const(vtop, w, nl);
			nz = !wi_is_zero(nl, w);
			vpop();
			if (dbt == VT_BOOL)
				{ MCC_TRACE("br\n"); vpushi(nz); }
			else
				{ MCC_TRACE("br\n"); vpush64(VT_LLONG | VT_UNSIGNED, w[0]); }
			gen_cast(dt);
			return;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at "
												"load time"); }
		wideint_materialize(&st, &a);
		wideint_limb_lval(&a, 0, 1);
		if (dbt == VT_BOOL) { MCC_TRACE("br\n");
			for (i = 1; i < nl; i++) { MCC_TRACE("br\n");
				wideint_limb_lval(&a, i, 1);
				gen_op('|');
			}
			gen_cast(dt);
			vcheck_cmp();
			return;
		}
		gen_cast(dt);
		return;
	}

	if (is_float(vtop->type.t)) { MCC_TRACE("br\n");
		int uns = wideint_is_unsigned(dt);
		int fbt = vtop->type.t & VT_BTYPE;
		SValue res;
		if (fbt != VT_FLOAT && fbt != VT_DOUBLE) { MCC_TRACE("br\n");
			CType dbl;
			dbl.t = VT_DOUBLE;
			dbl.ref = NULL;
			dbl.bp = dbl.bs = 0;
			gen_cast(&dbl);
			fbt = VT_DOUBLE;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at "
												"load time"); }
		wideint_local(dt, &res);
		vpush_helper_func(wideint_fromf_helper(fbt == VT_FLOAT, nl));
		vswap();
		wideint_push_ptr(&res);
		vswap();
		vpushi(uns);
		gfunc_call(3);
		vpushv(&res);
		return;
	}
	if (!is_integer_btype(sbt) || sbt == VT_PTR)
		{ MCC_TRACE("br\n"); cast_error(&vtop->type, dt); }
	{
		int su = (vtop->type.t & VT_UNSIGNED) != 0 || sbt == VT_BOOL;
		SValue res;
		gen_cast(su ? &u64 : &s64);
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			wilimb w[MCC_WIDE512_LIMBS];
			wilimb v = vtop->c.i;
			if (su)
				{ MCC_TRACE("br\n"); wi_from_u64(nl, w, v); }
			else
				{ MCC_TRACE("br\n"); wi_from_i64(nl, w, v); }
			vpop();
			vpush_wideint_const(dt, w, nl);
			return;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at "
												"load time"); }
		wideint_local(dt, &res);
		wideint_store_int(&res, su, nl);
		vpushv(&res);
	}
}

static void wideint_init_putv(void *ptr, SValue *sv) { MCC_TRACE("enter\n");
	wilimb w[MCC_WIDE512_LIMBS];
	int i, nl = wideint_nlimbs(&sv->type);

	wideint_get_const(sv, w, nl);
	for (i = 0; i < nl; i++)
		{ MCC_TRACE("br\n"); write64le((char *)ptr + i * 8, w[i]); }
}

#endif
