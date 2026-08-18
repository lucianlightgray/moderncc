#ifndef MCC_BITINT_SLICE_H
#define MCC_BITINT_SLICE_H

static int bitint_nlimbs_n(int n) { MCC_TRACE("enter\n");
	return n <= 128 ? 2 : n <= 256 ? 4 : 8;
}

static int bitint_arith_nl(int n) { MCC_TRACE("enter\n");
	return n <= 256 ? 4 : 8;
}

static int bitint_nlimbs(CType *type) { MCC_TRACE("enter\n");
	int align;
	return type_size(type, &align) / 8;
}

static int bitint_prec(CType *type) { MCC_TRACE("enter\n");
	int nl = bitint_nlimbs(type);
	return (nl <= 4 && type->bs == 0) ? 256 : type->bs;
}

static void mk_bitint_type(CType *type, int is_unsigned, int n) { MCC_TRACE("enter\n");
	Sym *s, *f, *prev = NULL;
	AttributeDef ad;
	int i, nl = bitint_nlimbs_n(n);
	int idx = (nl == 8 ? 4 : nl == 4 ? 2 : 0) + (is_unsigned ? 1 : 0);
	int lt = VT_LLONG | (is_unsigned ? VT_UNSIGNED : 0);
	unsigned short bs = (unsigned short)(n >= 256 && nl <= 4 ? 0 : n);

	if (mcc_state->gen_bitint128_type_cache[idx].ref) { MCC_TRACE("br\n");
		*type = mcc_state->gen_bitint128_type_cache[idx];
		type->bs = bs;
		return;
	}
	if (!mcc_state->gen_bitint128_limb_tok[0]) { MCC_TRACE("br\n");
		for (i = 0; i < MCC_WIDE512_LIMBS; i++) { MCC_TRACE("br\n");
			char nm[16];
			snprintf(nm, sizeof nm, "__b%d", i);
			mcc_state->gen_bitint128_limb_tok[i] = tok_alloc(nm, (int)strlen(nm))->tok;
		}
	}
	s = sym_push2(&global_stack, anon_sym++ | SYM_STRUCT, VT_STRUCT, -1);
	s->r = 0;
	s->a.is_bitint = 1;
	s->next = NULL;
	for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
		f = sym_push2(&global_stack,
									mcc_state->gen_bitint128_limb_tok[i] | SYM_FIELD, lt, 0);
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
	mcc_state->gen_bitint128_type_cache[idx] = *type;
	type->bs = bs;
}

static void bitint_materialize(CType *vt, SValue *out) { MCC_TRACE("enter\n");
	int i, nl = bitint_nlimbs(vt);

	if (wideint_sv_is_stable_lval(vtop)) { MCC_TRACE("br\n");
		*out = *vtop;
		vpop();
		return;
	}
	if (wideint_sv_is_const(vtop)) { MCC_TRACE("br\n");
		wilimb w[MCC_WIDE512_LIMBS];
		for (i = 0; i < nl; i++)
			{ MCC_TRACE("br\n"); w[i] = (&vtop->c.q.lo)[i]; }
		vpop();
		wideint_local(vt, out);
		for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
			wideint_limb_lval(out, i, 1);
			vpush64(VT_LLONG | VT_UNSIGNED, w[i]);
			vstore();
			vpop();
		}
		return;
	}
	wideint_local(vt, out);
	vpushv(out);
	vswap();
	vstore();
	vpop();
}

static void bitint_to_wide(int uns) { MCC_TRACE("enter\n");
	CType wt, bt = vtop->type;
	SValue a, res;
	int i, nl = bitint_nlimbs(&bt), anl = bitint_arith_nl(bitint_prec(&bt));

	bitint_materialize(&bt, &a);
	mk_wideint_type(&wt, uns, anl);
	wideint_local(&wt, &res);
	for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
		wideint_limb_lval(&res, i, 1);
		wideint_limb_lval(&a, i, 1);
		vstore();
		vpop();
	}
	for (i = nl; i < anl; i++) { MCC_TRACE("br\n");
		wideint_limb_lval(&res, i, 1);
		if (uns) { MCC_TRACE("br\n");
			vpush64(VT_LLONG | VT_UNSIGNED, 0);
		} else { MCC_TRACE("br\n");
			wideint_limb_lval(&a, nl - 1, 0);
			vpushi(63);
			gen_op(TOK_SAR);
		}
		vstore();
		vpop();
	}
	vpushv(&res);
}

static void wide_to_bitint(int uns, int n, SValue *out) { MCC_TRACE("enter\n");
	CType wt = vtop->type, bt;
	SValue r;
	int i, nl = bitint_nlimbs_n(n), anl = bitint_arith_nl(n);

	if (nl >= 4 && n < anl * 64) { MCC_TRACE("br\n");
		int sh = anl * 64 - n;
		vpushi(sh);
		gen_wideint_op(TOK_SHL);
		if (!uns)
			mk_wideint_type(&vtop->type, 0, wideint_nlimbs(&vtop->type));
		vpushi(sh);
		gen_wideint_op(uns ? TOK_SHR : TOK_SAR);
	}
	wideint_materialize(&wt, &r);
	mk_bitint_type(&bt, uns, n);
	wideint_local(&bt, out);
	if (nl == 2) { MCC_TRACE("br\n");
		wideint_limb_lval(out, 0, 1);
		wideint_limb_lval(&r, 0, 1);
		vstore();
		vpop();
		wideint_limb_lval(out, 1, 1);
		if (n == 128) { MCC_TRACE("br\n");
			wideint_limb_lval(&r, 1, 1);
		} else { MCC_TRACE("br\n");
			int k = n - 64, sh = 64 - k;
			if (uns) { MCC_TRACE("br\n");
				wideint_limb_lval(&r, 1, 1);
				vpush64(VT_LLONG | VT_UNSIGNED, ((wilimb)1 << k) - 1);
				gen_op('&');
			} else { MCC_TRACE("br\n");
				wideint_limb_lval(&r, 1, 0);
				vpushi(sh);
				gen_op(TOK_SHL);
				vpushi(sh);
				gen_op(TOK_SAR);
			}
		}
		vstore();
		vpop();
	} else { MCC_TRACE("br\n");
		for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
			wideint_limb_lval(out, i, 1);
			wideint_limb_lval(&r, i, 1);
			vstore();
			vpop();
		}
	}
}

static void bitint_reduce_limbs(wilimb *w, int n, int uns) { MCC_TRACE("enter\n");
	wilimb tmp[MCC_WIDE512_LIMBS];
	int k, sh, anl = bitint_arith_nl(n);
	wilimb mask;

	if (n >= anl * 64)
		{ MCC_TRACE("br\n"); return; }
	if (n <= 64) { MCC_TRACE("br\n");
		if (n < 64) { MCC_TRACE("br\n");
			mask = ((wilimb)1 << n) - 1;
			w[0] &= mask;
			if (!uns && (w[0] & ((wilimb)1 << (n - 1))))
				{ MCC_TRACE("br\n"); w[0] |= ~mask; }
		}
		for (k = 1; k < anl; k++)
			{ MCC_TRACE("br\n"); w[k] = (!uns && (w[0] >> 63)) ? ~(wilimb)0 : 0; }
		return;
	}
	if (n <= 128) { MCC_TRACE("br\n");
		k = n - 64;
		mask = (k == 64) ? ~(wilimb)0 : (((wilimb)1 << k) - 1);
		if (uns) { MCC_TRACE("br\n");
			w[1] &= mask;
		} else { MCC_TRACE("br\n");
			w[1] &= mask;
			if (w[1] & ((wilimb)1 << (k - 1)))
				{ MCC_TRACE("br\n"); w[1] |= ~mask; }
		}
		for (k = 2; k < anl; k++)
			{ MCC_TRACE("br\n"); w[k] = (!uns && (w[1] >> 63)) ? ~(wilimb)0 : 0; }
		return;
	}
	sh = anl * 64 - n;
	wi_shl(anl, tmp, w, (unsigned int)sh);
	if (uns)
		{ MCC_TRACE("br\n"); wi_shr(anl, w, tmp, (unsigned int)sh); }
	else
		{ MCC_TRACE("br\n"); wi_sar(anl, w, tmp, (unsigned int)sh); }
}

static void vpush_bitint_const(CType *vt, const wilimb *w, int anl) { MCC_TRACE("enter\n");
	CValue cv;
	int i;
	memset(&cv, 0, sizeof cv);
	for (i = 0; i < anl; i++)
		{ MCC_TRACE("br\n"); (&cv.q.lo)[i] = w[i]; }
	vsetc(vt, VT_CONST, &cv);
}

static void bitint_sv_limbs(SValue *sv, wilimb *w, int anl) { MCC_TRACE("enter\n");
	int i;
	for (i = 0; i < anl; i++)
		{ MCC_TRACE("br\n"); w[i] = (&sv->c.q.lo)[i]; }
}

static void bitint_sext_limbs(wilimb *w, int from_nl, int to_nl, int uns) { MCC_TRACE("enter\n");
	int j;
	for (j = from_nl; j < to_nl; j++)
		{ MCC_TRACE("br\n"); w[j] = (!uns && (w[from_nl - 1] >> 63)) ? ~(wilimb)0 : 0; }
}

static void bitint_read_operand(SValue *sv, wilimb *w, int anl) { MCC_TRACE("enter\n");
	if (is_bitint_type(&sv->type)) { MCC_TRACE("br\n");
		int sanl = bitint_arith_nl(bitint_prec(&sv->type));
		bitint_sv_limbs(sv, w, sanl);
		bitint_sext_limbs(w, sanl, anl, bitint_is_unsigned(&sv->type));
		return;
	}
	if ((sv->type.t & VT_UNSIGNED) || (sv->type.t & VT_BTYPE) == VT_BOOL)
		{ MCC_TRACE("br\n"); wi_from_u64(anl, w, sv->c.i); }
	else
		{ MCC_TRACE("br\n"); wi_from_i64(anl, w, sv->c.i); }
}

static void gen_bitint_op(int op) { MCC_TRACE("enter\n");
	int shift = (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR);
	int cmp = TOK_ISCOND(op);
	int lb, rb, n, uns, anl;
	CType rt;
	SValue res;

	vcheck_cmp();
	vswap();
	vcheck_cmp();
	vswap();

	lb = is_bitint_type(&vtop[-1].type);
	rb = is_bitint_type(&vtop[0].type);

	if (shift && !lb) { MCC_TRACE("br\n");
		gen_cast_s(VT_INT);
		gen_op(op);
		return;
	}

	if (lb && rb) { MCC_TRACE("br\n");
		int ln = bitint_prec(&vtop[-1].type), rn = bitint_prec(&vtop[0].type);
		int lu = bitint_is_unsigned(&vtop[-1].type);
		int ru = bitint_is_unsigned(&vtop[0].type);
		if (ln == rn) { MCC_TRACE("br\n"); n = ln; uns = lu || ru; }
		else if (ln > rn) { MCC_TRACE("br\n"); n = ln; uns = lu; }
		else { MCC_TRACE("br\n"); n = rn; uns = ru; }
	} else { MCC_TRACE("br\n");
		CType *b = lb ? &vtop[-1].type : &vtop[0].type;
		CType *o = lb ? &vtop[0].type : &vtop[-1].type;
		int obt = o->t & VT_BTYPE;
		if (is_float(o->t) || !is_integer_btype(obt) || obt == VT_PTR)
			{ MCC_TRACE("br\n"); mcc_error("invalid operand types for a '_BitInt' operation"); }
		n = bitint_prec(b);
		uns = bitint_is_unsigned(b);
	}
	if (op == TOK_ULT || op == TOK_UGT || op == TOK_ULE || op == TOK_UGE ||
			op == TOK_UDIV || op == TOK_UMOD)
		{ MCC_TRACE("br\n"); uns = 1; }

	anl = bitint_arith_nl(n);
	mk_bitint_type(&rt, uns, n);

	if (wideint_sv_is_const(vtop - 1) &&
			(shift ? (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
						 : (rb && wideint_sv_is_const(vtop)))) { MCC_TRACE("br\n");
		wilimb wa[MCC_WIDE512_LIMBS], wb[MCC_WIDE512_LIMBS], wr[MCC_WIDE512_LIMBS];
		bitint_read_operand(vtop - 1, wa, anl);
		if (shift) { MCC_TRACE("br\n");
			long long sn = (long long)vtop->c.i;
			vpop();
			vpop();
			if (sn < 0 || sn >= 64 * anl) { MCC_TRACE("br\n");
				if (op == TOK_SAR && !uns)
					{ MCC_TRACE("br\n"); wi_sar(anl, wr, wa, 64 * anl); }
				else
					{ MCC_TRACE("br\n"); wi_zero(anl, wr); }
			} else if (op == TOK_SHL) { MCC_TRACE("br\n");
				wi_shl(anl, wr, wa, (unsigned int)sn);
			} else if (uns || op == TOK_SHR) { MCC_TRACE("br\n");
				wi_shr(anl, wr, wa, (unsigned int)sn);
			} else { MCC_TRACE("br\n");
				wi_sar(anl, wr, wa, (unsigned int)sn);
			}
			bitint_reduce_limbs(wr, n, uns);
			vpush_bitint_const(&rt, wr, anl);
			return;
		}
		bitint_read_operand(vtop, wb, anl);
		vpop();
		vpop();
		if (cmp) { MCC_TRACE("br\n");
			int c = uns ? wi_ucmp(anl, wa, wb) : wi_scmp(anl, wa, wb);
			vpushi(wideint_cmp_result(op, c));
			return;
		}
		wideint_fold_bin(op, uns, anl, wa, wb, wr);
		bitint_reduce_limbs(wr, n, uns);
		vpush_bitint_const(&rt, wr, anl);
		return;
	}

	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load time"); }

	if (shift) { MCC_TRACE("br\n");
		vswap();
		bitint_to_wide(uns);
		vswap();
		gen_wideint_op(op);
		wide_to_bitint(uns, n, &res);
		vpushv(&res);
		return;
	}

	gen_cast(&rt);
	vswap();
	gen_cast(&rt);
	vswap();
	vswap();
	bitint_to_wide(uns);
	vswap();
	bitint_to_wide(uns);
	gen_wideint_op(op);
	if (cmp) { MCC_TRACE("br\n"); return; }
	wide_to_bitint(uns, n, &res);
	vpushv(&res);
}

static void bitint_from_int(CType *dt, int n, int uns) { MCC_TRACE("enter\n");
	CType u64 = wideint_u64_type(), s64 = wideint_s64_type();
	int su = (vtop->type.t & VT_UNSIGNED) != 0 || (vtop->type.t & VT_BTYPE) == VT_BOOL;
	int i, nl = bitint_nlimbs_n(n), anl = bitint_arith_nl(n);
	SValue res;

	if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
		wilimb w[MCC_WIDE512_LIMBS];
		wilimb v = vtop->c.i;
		if (su)
			{ MCC_TRACE("br\n"); wi_from_u64(anl, w, v); }
		else
			{ MCC_TRACE("br\n"); wi_from_i64(anl, w, v); }
		bitint_reduce_limbs(w, n, uns);
		vpop();
		vpush_bitint_const(dt, w, anl);
		return;
	}
	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load time"); }
	gen_cast(su ? &u64 : &s64);
	wideint_local(dt, &res);
	wideint_limb_lval(&res, 0, 1);
	vswap();
	vstore();
	vpop();
	for (i = 1; i < nl; i++) { MCC_TRACE("br\n");
		wideint_limb_lval(&res, i, 1);
		if (su) { MCC_TRACE("br\n");
			vpush64(VT_LLONG | VT_UNSIGNED, 0);
		} else { MCC_TRACE("br\n");
			wideint_limb_lval(&res, 0, 0);
			vpushi(63);
			gen_op(TOK_SAR);
		}
		vstore();
		vpop();
	}
	vpushv(&res);
}

static void gen_bitint_cast(CType *dt) { MCC_TRACE("enter\n");
	int sb = is_bitint_type(&vtop->type), db = is_bitint_type(dt);
	int dbt = dt->t & VT_BTYPE, sbt = vtop->type.t & VT_BTYPE;

	if (sb && db) { MCC_TRACE("br\n");
		int sn = bitint_prec(&vtop->type), dn = bitint_prec(dt);
		int duns = bitint_is_unsigned(dt);
		int suns = bitint_is_unsigned(&vtop->type);
		if (sn == dn && suns == duns) { MCC_TRACE("br\n");
			vtop->type.t = (vtop->type.t & ~VT_BTYPE) | VT_STRUCT;
			vtop->type.ref = dt->ref;
			vtop->type.bs = (unsigned short)(dn >= 256 && bitint_nlimbs_n(dn) <= 4 ? 0 : dn);
			return;
		}
		if (wideint_sv_is_const(vtop)) { MCC_TRACE("br\n");
			wilimb w[MCC_WIDE512_LIMBS];
			int sanl = bitint_arith_nl(sn), danl = bitint_arith_nl(dn);
			bitint_sv_limbs(vtop, w, sanl);
			bitint_sext_limbs(w, sanl, danl, suns);
			bitint_reduce_limbs(w, dn, duns);
			vpop();
			vpush_bitint_const(dt, w, danl);
			return;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load time"); }
		{
			SValue res;
			bitint_to_wide(suns);
			wide_to_bitint(duns, dn, &res);
			vpushv(&res);
		}
		return;
	}

	if (sb) { MCC_TRACE("br\n");
		int suns = bitint_is_unsigned(&vtop->type);
		int i, nl = bitint_nlimbs(&vtop->type);
		int sn = bitint_prec(&vtop->type);
		SValue a;
		if (dbt == VT_VOID) { MCC_TRACE("br\n");
			vtop->type = *dt;
			return;
		}
		if (is_float(dt->t)) { MCC_TRACE("br\n");
			if (nocode_wanted & DATA_ONLY_WANTED)
				{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at "
													"load time"); }
			bitint_to_wide(suns);
			gen_cast(dt);
			return;
		}
		if (!is_integer_btype(dbt) || dbt == VT_PTR)
			{ MCC_TRACE("br\n"); cast_error(&vtop->type, dt); }
		if (wideint_sv_is_const(vtop)) { MCC_TRACE("br\n");
			wilimb w[MCC_WIDE512_LIMBS];
			int nz, j;
			bitint_sv_limbs(vtop, w, nl);
			nz = 0;
			for (j = 0; j < nl; j++)
				{ MCC_TRACE("br\n"); nz |= (w[j] != 0); }
			vpop();
			if (dbt == VT_BOOL)
				{ MCC_TRACE("br\n"); vpushi(nz); }
			else if (dbt == VT_INT128) { MCC_TRACE("br\n");
				CValue cv;
				bitint_reduce_limbs(w, sn, suns);
				memset(&cv, 0, sizeof cv);
				cv.q.lo = w[0];
				cv.q.hi = w[1];
				vsetc(dt, VT_CONST, &cv);
				return;
			} else
				{ MCC_TRACE("br\n"); vpush64(VT_LLONG | VT_UNSIGNED, w[0]); }
			gen_cast(dt);
			return;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load "
												"time"); }
		if (dbt == VT_INT128) { MCC_TRACE("br\n");
			CType u128;
			SValue wa;
			u128.t = VT_INT128 | VT_UNSIGNED;
			u128.bp = 0;
			u128.bs = 0;
			u128.ref = NULL;
			bitint_to_wide(suns);
			wideint_materialize(&vtop->type, &wa);
			wideint_limb_lval(&wa, 1, 1);
			gen_cast(&u128);
			vpushi(64);
			gen_op(TOK_SHL);
			wideint_limb_lval(&wa, 0, 1);
			gen_cast(&u128);
			gen_op('|');
			vtop->type = *dt;
			return;
		}
		bitint_materialize(&vtop->type, &a);
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
		(void)suns;
		return;
	}

	if (is_float(vtop->type.t)) { MCC_TRACE("br\n");
		int duns = bitint_is_unsigned(dt);
		int dn = bitint_prec(dt);
		CType w;
		SValue res;
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load "
												"time"); }
		mk_wideint_type(&w, duns, bitint_arith_nl(dn));
		gen_cast(&w);
		wide_to_bitint(duns, dn, &res);
		vpushv(&res);
		return;
	}
	if (!is_integer_btype(sbt) || sbt == VT_PTR)
		{ MCC_TRACE("br\n"); cast_error(&vtop->type, dt); }
	bitint_from_int(dt, bitint_prec(dt), bitint_is_unsigned(dt));
}

static void bitint_init_putv(void *ptr, SValue *sv) { MCC_TRACE("enter\n");
	int i, nl = bitint_nlimbs(&sv->type);

	for (i = 0; i < nl; i++)
		{ MCC_TRACE("br\n"); write64le((char *)ptr + i * 8, (&sv->c.q.lo)[i]); }
}

static void bitint_deconst(void) { MCC_TRACE("enter\n");
	SValue res;
	CType vt;

	if ((vtop->type.t & VT_BTYPE) != VT_STRUCT || (vtop->r & VT_LVAL))
		{ MCC_TRACE("br\n"); return; }
	if (!is_bitint_type(&vtop->type) || !wideint_sv_is_const(vtop))
		{ MCC_TRACE("br\n"); return; }
	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); return; }
	vt = vtop->type;
	bitint_materialize(&vt, &res);
	vpushv(&res);
}

#endif
