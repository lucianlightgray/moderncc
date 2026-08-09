#ifndef MCC_WIDE256_SLICE_H
#define MCC_WIDE256_SLICE_H

#define MCC_W256_LIMBS MCC_WIDE256_LIMBS
#include "wide256_arith.h"

static CType wide256_u64_type(void) { MCC_TRACE("enter\n");
	CType t;
	t.t = VT_LLONG | VT_UNSIGNED;
	t.bp = t.bs = 0;
	t.ref = NULL;
	return t;
}

static CType wide256_s64_type(void) { MCC_TRACE("enter\n");
	CType t;
	t.t = VT_LLONG;
	t.bp = t.bs = 0;
	t.ref = NULL;
	return t;
}

static void mk_wide256_type(CType *type, int is_unsigned) { MCC_TRACE("enter\n");
	Sym *s, *f, *prev = NULL;
	AttributeDef ad;
	int i, idx = is_unsigned ? 1 : 0;
	int lt = VT_LLONG | (is_unsigned ? VT_UNSIGNED : 0);

	if (mcc_state->gen_wide256_type_cache[idx].ref) { MCC_TRACE("br\n");
		*type = mcc_state->gen_wide256_type_cache[idx];
		return;
	}
	if (!mcc_state->gen_wide256_limb_tok[0]) { MCC_TRACE("br\n");
		for (i = 0; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
			char nm[16];
			snprintf(nm, sizeof nm, "__w%d", i);
			mcc_state->gen_wide256_limb_tok[i] = tok_alloc(nm, (int)strlen(nm))->tok;
		}
	}
	s = sym_push2(&global_stack, anon_sym++ | SYM_STRUCT, VT_STRUCT, -1);
	s->r = 0;
	s->a.is_wideint = 1;
	s->next = NULL;
	for (i = 0; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
		f = sym_push2(&global_stack,
									mcc_state->gen_wide256_limb_tok[i] | SYM_FIELD, lt, 0);
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
	mcc_state->gen_wide256_type_cache[idx] = *type;
	if (mcc_state->gen_wide256_type_cache_n < idx + 1)
		{ MCC_TRACE("br\n"); mcc_state->gen_wide256_type_cache_n = idx + 1; }
}

static int wide256_sv_is_const(SValue *sv) { MCC_TRACE("enter\n");
	return (sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;
}

static void wide256_get_const(SValue *sv, mcc_w256_limb *w) { MCC_TRACE("enter\n");
	w[0] = sv->c.q.lo;
	w[1] = sv->c.q.hi;
	w[2] = sv->c.q.w2;
	w[3] = sv->c.q.w3;
}

static void vpush_wide256_const(CType *vt, const mcc_w256_limb *w) { MCC_TRACE("enter\n");
	CValue cv;
	memset(&cv, 0, sizeof cv);
	cv.q.lo = w[0];
	cv.q.hi = w[1];
	cv.q.w2 = w[2];
	cv.q.w3 = w[3];
	vsetc(vt, VT_CONST, &cv);
}

static int wide256_sv_is_stable_lval(SValue *sv) { MCC_TRACE("enter\n");
	if (!(sv->r & VT_LVAL) || (sv->r & VT_REGDISP))
		{ MCC_TRACE("br\n"); return 0; }
	if ((sv->r & VT_VALMASK) == VT_LOCAL && !sv->sym)
		{ MCC_TRACE("br\n"); return 1; }
	if ((sv->r & VT_VALMASK) == VT_CONST && (sv->r & VT_SYM))
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

static void wide256_local(CType *vt, SValue *out) { MCC_TRACE("enter\n");
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

static void wide256_limb_lval(SValue *base, int i, int is_unsigned) { MCC_TRACE("enter\n");
	CType lt = is_unsigned ? wide256_u64_type() : wide256_s64_type();

	vpushv(base);
	test_lvalue();
	gaddrof();
	vtop->type = char_pointer_type;
	vpushi(i * 8);
	gen_op('+');
	vtop->type = lt;
	vtop->r |= VT_LVAL;
}

static void wide256_push_ptr(SValue *sv) { MCC_TRACE("enter\n");
	vpushv(sv);
	test_lvalue();
	mk_pointer(&vtop->type);
	gaddrof();
}

static void wide256_store_int(SValue *out, int src_unsigned) { MCC_TRACE("enter\n");
	int i;

	wide256_limb_lval(out, 0, 1);
	vswap();
	vstore();
	vpop();
	for (i = 1; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
		wide256_limb_lval(out, i, 1);
		if (src_unsigned) { MCC_TRACE("br\n");
			vpush64(VT_LLONG | VT_UNSIGNED, 0);
		} else { MCC_TRACE("br\n");
			wide256_limb_lval(out, 0, 0);
			vpushi(63);
			gen_op(TOK_SAR);
		}
		vstore();
		vpop();
	}
}

static void wide256_materialize(CType *vt, SValue *out) { MCC_TRACE("enter\n");
	CType lt = *vt;
	int i;

	if (is_wide256_type(&vtop->type) && wide256_sv_is_stable_lval(vtop)) { MCC_TRACE("br\n");
		*out = *vtop;
		vpop();
		return;
	}
	if (is_wide256_type(&vtop->type) && wide256_sv_is_const(vtop)) { MCC_TRACE("br\n");
		mcc_w256_limb w[MCC_WIDE256_LIMBS];
		wide256_get_const(vtop, w);
		vpop();
		wide256_local(&lt, out);
		for (i = 0; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
			wide256_limb_lval(out, i, 1);
			vpush64(VT_LLONG | VT_UNSIGNED, w[i]);
			vstore();
			vpop();
		}
		return;
	}
	wide256_local(&lt, out);
	vpushv(out);
	vswap();
	vstore();
	vpop();
}

static void wide256_deconst(void) { MCC_TRACE("enter\n");
	SValue res;
	CType vt;

	if ((vtop->type.t & VT_BTYPE) != VT_STRUCT || (vtop->r & VT_LVAL))
		{ MCC_TRACE("br\n"); return; }
	if (!is_wide256_type(&vtop->type) || !wide256_sv_is_const(vtop))
		{ MCC_TRACE("br\n"); return; }
	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); return; }
	vt = vtop->type;
	wide256_materialize(&vt, &res);
	vpushv(&res);
}

static void wide256_settle(void) { MCC_TRACE("enter\n");
	vcheck_cmp();
	if (vtop->r != VT_CMP && !(vtop->r & VT_SYM))
		{ MCC_TRACE("br\n"); vtop->sym = NULL; }
}

static int wide256_op_ok(int op) { MCC_TRACE("enter\n");
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

static int wide256_cmp_result(int op, int c) { MCC_TRACE("enter\n");
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

static int wide256_cmp_signed_tok(int op) { MCC_TRACE("enter\n");
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

static int wide256_bin_helper(int op, int uns) { MCC_TRACE("enter\n");
	switch (op) { MCC_TRACE("br\n");
	case '+':
		return TOK___mcc_i256_add;
	case '-':
		return TOK___mcc_i256_sub;
	case '*':
		return TOK___mcc_i256_mul;
	case '&':
		return TOK___mcc_i256_and;
	case '|':
		return TOK___mcc_i256_or;
	case '^':
		return TOK___mcc_i256_xor;
	case TOK_UDIV:
		return TOK___mcc_i256_udiv;
	case TOK_UMOD:
		return TOK___mcc_i256_umod;
	case '/':
	case TOK_PDIV:
		return uns ? TOK___mcc_i256_udiv : TOK___mcc_i256_sdiv;
	}
	return uns ? TOK___mcc_i256_umod : TOK___mcc_i256_smod;
}

static void wide256_fold_bin(int op, int uns, const mcc_w256_limb *wa,
														 const mcc_w256_limb *wb, mcc_w256_limb *wr) { MCC_TRACE("enter\n");
	mcc_w256_limb q[MCC_WIDE256_LIMBS], rem[MCC_WIDE256_LIMBS];
	int udiv = (op == TOK_UDIV || op == TOK_UMOD) || uns;

	switch (op) { MCC_TRACE("br\n");
	case '+':
		mcc_w256_add(wr, wa, wb);
		return;
	case '-':
		mcc_w256_sub(wr, wa, wb);
		return;
	case '*':
		mcc_w256_mul(wr, wa, wb);
		return;
	case '&':
		mcc_w256_and(wr, wa, wb);
		return;
	case '|':
		mcc_w256_or(wr, wa, wb);
		return;
	case '^':
		mcc_w256_xor(wr, wa, wb);
		return;
	}
	if (mcc_w256_is_zero(wb))
		{ MCC_TRACE("br\n"); mcc_error("division by zero in constant"); }
	if (udiv)
		{ MCC_TRACE("br\n"); mcc_w256_udivmod(q, rem, wa, wb); }
	else
		{ MCC_TRACE("br\n"); mcc_w256_sdivmod(q, rem, wa, wb); }
	if (op == '%' || op == TOK_UMOD)
		{ MCC_TRACE("br\n"); mcc_w256_copy(wr, rem); }
	else
		{ MCC_TRACE("br\n"); mcc_w256_copy(wr, q); }
}

static void wide256_call3(int fn, SValue *res, SValue *a, SValue *b) { MCC_TRACE("enter\n");
	vpush_helper_func(fn);
	wide256_push_ptr(res);
	wide256_push_ptr(a);
	wide256_push_ptr(b);
	gfunc_call(3);
}

static void gen_wide256_op(int op) { MCC_TRACE("enter\n");
	CType wt;
	SValue a, b, res;
	mcc_w256_limb wa[MCC_WIDE256_LIMBS], wb[MCC_WIDE256_LIMBS];
	mcc_w256_limb wr[MCC_WIDE256_LIMBS];
	int uns, cmp, shift, lw, rw;

	vcheck_cmp();
	vswap();
	vcheck_cmp();
	vswap();

	shift = (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR);
	cmp = TOK_ISCOND(op);
	lw = is_wide256_type(&vtop[-1].type);
	rw = is_wide256_type(&vtop[0].type);

	if (shift && !lw) { MCC_TRACE("br\n");
		gen_cast_s(VT_INT);
		gen_op(op);
		return;
	}
	if (!wide256_op_ok(op))
		{ MCC_TRACE("br\n"); mcc_error("operator is not defined on '__int256'"); }

	if (shift) { MCC_TRACE("br\n");
		CType ct = wide256_s64_type();
		wt = vtop[-1].type;
		uns = wide256_is_unsigned(&wt);
		gen_cast(&ct);
	} else { MCC_TRACE("br\n");
		if (lw && rw) { MCC_TRACE("br\n");
			uns = wide256_is_unsigned(&vtop[-1].type) ||
						wide256_is_unsigned(&vtop[0].type);
		} else { MCC_TRACE("br\n");
			CType *o = lw ? &vtop[0].type : &vtop[-1].type;
			int obt = o->t & VT_BTYPE;
			if (is_float(o->t) || !is_integer_btype(obt) || obt == VT_PTR)
				{ MCC_TRACE("br\n"); mcc_error("invalid operand types for an '__int256' "
													"operation"); }
			uns = wide256_is_unsigned(lw ? &vtop[-1].type : &vtop[0].type);
		}
		if (op == TOK_ULT || op == TOK_UGT || op == TOK_ULE || op == TOK_UGE ||
				op == TOK_UDIV || op == TOK_UMOD)
			{ MCC_TRACE("br\n"); uns = 1; }
		mk_wide256_type(&wt, uns);
		gen_cast(&wt);
		vswap();
		gen_cast(&wt);
		vswap();
	}

	if (wide256_sv_is_const(vtop - 1) &&
			(shift ? (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
						 : wide256_sv_is_const(vtop))) { MCC_TRACE("br\n");
		wide256_get_const(vtop - 1, wa);
		if (shift) { MCC_TRACE("br\n");
			long long n = (long long)vtop->c.i;
			vpop();
			vpop();
			if (n < 0 || n >= 64 * MCC_WIDE256_LIMBS) { MCC_TRACE("br\n");
				if (op == TOK_SAR && !uns)
					{ MCC_TRACE("br\n"); mcc_w256_sar(wr, wa, 64 * MCC_WIDE256_LIMBS); }
				else
					{ MCC_TRACE("br\n"); mcc_w256_zero(wr); }
			} else if (op == TOK_SHL) { MCC_TRACE("br\n");
				mcc_w256_shl(wr, wa, (unsigned int)n);
			} else if (uns || op == TOK_SHR) { MCC_TRACE("br\n");
				mcc_w256_shr(wr, wa, (unsigned int)n);
			} else { MCC_TRACE("br\n");
				mcc_w256_sar(wr, wa, (unsigned int)n);
			}
			vpush_wide256_const(&wt, wr);
			return;
		}
		wide256_get_const(vtop, wb);
		vpop();
		vpop();
		if (cmp) { MCC_TRACE("br\n");
			int c = uns ? mcc_w256_ucmp(wa, wb) : mcc_w256_scmp(wa, wb);
			vpushi(wide256_cmp_result(op, c));
			return;
		}
		wide256_fold_bin(op, uns, wa, wb, wr);
		vpush_wide256_const(&wt, wr);
		return;
	}

	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load "
											"time"); }

	if (shift) { MCC_TRACE("br\n");
		int fn = op == TOK_SHL		 ? TOK___mcc_i256_shl
						 : (uns || op == TOK_SHR) ? TOK___mcc_i256_shr
																		: TOK___mcc_i256_sar;
		vswap();
		wide256_materialize(&wt, &a);
		wide256_local(&wt, &res);
		vpush_helper_func(fn);
		wide256_push_ptr(&res);
		wide256_push_ptr(&a);
		vrotb(4);
		gfunc_call(3);
		vpushv(&res);
		return;
	}

	wide256_materialize(&wt, &b);
	wide256_materialize(&wt, &a);

	if (cmp) { MCC_TRACE("br\n");
		vpush_helper_func(uns ? TOK___mcc_i256_ucmp : TOK___mcc_i256_scmp);
		wide256_push_ptr(&a);
		wide256_push_ptr(&b);
		gfunc_call(2);
		vpushi(0);
		vtop->r = REG_IRET;
		vpushi(0);
		gen_op(wide256_cmp_signed_tok(op));
		wide256_settle();
		return;
	}

	wide256_local(&wt, &res);
	wide256_call3(wide256_bin_helper(op, uns), &res, &a, &b);
	vpushv(&res);
}

static void gen_wide256_cast(CType *dt) { MCC_TRACE("enter\n");
	CType u64 = wide256_u64_type(), s64 = wide256_s64_type();
	int sw = is_wide256_type(&vtop->type), dw = is_wide256_type(dt);
	int dbt = dt->t & VT_BTYPE;
	int sbt = vtop->type.t & VT_BTYPE;

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
		if (is_float(dt->t))
			{ MCC_TRACE("br\n"); mcc_error("conversion between '__int256' and "
												"floating-point types is not supported"); }
		if (!is_integer_btype(dbt) || dbt == VT_PTR)
			{ MCC_TRACE("br\n"); cast_error(&vtop->type, dt); }
		if (wide256_sv_is_const(vtop)) { MCC_TRACE("br\n");
			mcc_w256_limb w[MCC_WIDE256_LIMBS];
			int nz;
			wide256_get_const(vtop, w);
			nz = !mcc_w256_is_zero(w);
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
		wide256_materialize(&st, &a);
		wide256_limb_lval(&a, 0, 1);
		if (dbt == VT_BOOL) { MCC_TRACE("br\n");
			for (i = 1; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
				wide256_limb_lval(&a, i, 1);
				gen_op('|');
			}
			gen_cast(dt);
			wide256_settle();
			return;
		}
		gen_cast(dt);
		return;
	}

	if (is_float(vtop->type.t))
		{ MCC_TRACE("br\n"); mcc_error("conversion between '__int256' and "
											"floating-point types is not supported"); }
	if (!is_integer_btype(sbt) || sbt == VT_PTR)
		{ MCC_TRACE("br\n"); cast_error(&vtop->type, dt); }
	{
		int su = (vtop->type.t & VT_UNSIGNED) != 0 || sbt == VT_BOOL;
		SValue res;
		gen_cast(su ? &u64 : &s64);
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			mcc_w256_limb w[MCC_WIDE256_LIMBS];
			mcc_w256_limb v = vtop->c.i;
			if (su)
				{ MCC_TRACE("br\n"); mcc_w256_from_u64(w, v); }
			else
				{ MCC_TRACE("br\n"); mcc_w256_from_i64(w, v); }
			vpop();
			vpush_wide256_const(dt, w);
			return;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at "
												"load time"); }
		wide256_local(dt, &res);
		wide256_store_int(&res, su);
		vpushv(&res);
	}
}

static void wide256_init_putv(void *ptr, SValue *sv) { MCC_TRACE("enter\n");
	mcc_w256_limb w[MCC_WIDE256_LIMBS];
	int i;

	wide256_get_const(sv, w);
	for (i = 0; i < MCC_WIDE256_LIMBS; i++)
		{ MCC_TRACE("br\n"); write64le((char *)ptr + i * 8, w[i]); }
}

#endif
