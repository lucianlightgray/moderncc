#ifndef MCC_BITINT128_SLICE_H
#define MCC_BITINT128_SLICE_H

/* C23 _BitInt(N) slice 2: 64 < N <= 128.  A 16-byte, 16-aligned 2-limb struct
 * (tagged a.is_bitint) whose value is reduced to N bits.  The struct layout/ABI
 * is identical for every N of a given signedness (so only two type instances are
 * cached); the precision N rides on the CType .bs channel.  Arithmetic reuses the
 * tested 4-limb __int256 kernel: extend the 2 stored limbs into an __int256,
 * run gen_wide256_op, then reduce the low 128 bits back to N.  The stored form is
 * kept CANONICAL: bits N..127 = sign(bit N-1) [signed] or 0 [unsigned], matching
 * gcc-16 so casts/reads observe identical bytes. */

static void mk_bitint128_type(CType *type, int is_unsigned, int n) { MCC_TRACE("enter\n");
	Sym *s, *f, *prev = NULL;
	AttributeDef ad;
	int i, idx = is_unsigned ? 1 : 0;
	int lt = VT_LLONG | (is_unsigned ? VT_UNSIGNED : 0);

	if (mcc_state->gen_bitint128_type_cache[idx].ref) { MCC_TRACE("br\n");
		*type = mcc_state->gen_bitint128_type_cache[idx];
		type->bs = (unsigned char)n;
		return;
	}
	if (!mcc_state->gen_bitint128_limb_tok[0]) { MCC_TRACE("br\n");
		for (i = 0; i < 2; i++) { MCC_TRACE("br\n");
			char nm[16];
			snprintf(nm, sizeof nm, "__b%d", i);
			mcc_state->gen_bitint128_limb_tok[i] = tok_alloc(nm, (int)strlen(nm))->tok;
		}
	}
	s = sym_push2(&global_stack, anon_sym++ | SYM_STRUCT, VT_STRUCT, -1);
	s->r = 0;
	s->a.is_bitint = 1;
	s->next = NULL;
	for (i = 0; i < 2; i++) { MCC_TRACE("br\n");
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
	type->bs = (unsigned char)n;
}

static int bitint128_prec(CType *type) { MCC_TRACE("enter\n");
	return type->bs;
}

static void bitint128_materialize(CType *vt, SValue *out) { MCC_TRACE("enter\n");
	int i;

	if (wide256_sv_is_stable_lval(vtop)) { MCC_TRACE("br\n");
		*out = *vtop;
		vpop();
		return;
	}
	if (wide256_sv_is_const(vtop)) { MCC_TRACE("br\n");
		mcc_w256_limb w0 = vtop->c.q.lo, w1 = vtop->c.q.hi;
		vpop();
		wide256_local(vt, out);
		wide256_limb_lval(out, 0, 1);
		vpush64(VT_LLONG | VT_UNSIGNED, w0);
		vstore();
		vpop();
		wide256_limb_lval(out, 1, 1);
		vpush64(VT_LLONG | VT_UNSIGNED, w1);
		vstore();
		vpop();
		(void)i;
		return;
	}
	wide256_local(vt, out);
	vpushv(out);
	vswap();
	vstore();
	vpop();
}

/* vtop (a bitint128 value) -> vtop (an __int256 value), canonically extended. */
static void bitint128_to_wide256(int uns) { MCC_TRACE("enter\n");
	CType wt, bt = vtop->type;
	SValue a, res;
	int i;

	bitint128_materialize(&bt, &a);
	mk_wide256_type(&wt, uns);
	wide256_local(&wt, &res);
	for (i = 0; i < 2; i++) { MCC_TRACE("br\n");
		wide256_limb_lval(&res, i, 1);
		wide256_limb_lval(&a, i, 1);
		vstore();
		vpop();
	}
	for (i = 2; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
		wide256_limb_lval(&res, i, 1);
		if (uns) { MCC_TRACE("br\n");
			vpush64(VT_LLONG | VT_UNSIGNED, 0);
		} else { MCC_TRACE("br\n");
			wide256_limb_lval(&a, 1, 0);
			vpushi(63);
			gen_op(TOK_SAR);
		}
		vstore();
		vpop();
	}
	vpushv(&res);
}

/* vtop (an __int256 value) -> *out (a stored bitint128(n) local, reduced). */
static void wide256_to_bitint128(int uns, int n, SValue *out) { MCC_TRACE("enter\n");
	CType wt = vtop->type, bt;
	SValue r;

	wide256_materialize(&wt, &r);
	mk_bitint128_type(&bt, uns, n);
	wide256_local(&bt, out);
	wide256_limb_lval(out, 0, 1);
	wide256_limb_lval(&r, 0, 1);
	vstore();
	vpop();
	wide256_limb_lval(out, 1, 1);
	if (n == 128) { MCC_TRACE("br\n");
		wide256_limb_lval(&r, 1, 1);
	} else { MCC_TRACE("br\n");
		int k = n - 64, sh = 64 - k;
		if (uns) { MCC_TRACE("br\n");
			wide256_limb_lval(&r, 1, 1);
			vpush64(VT_LLONG | VT_UNSIGNED, ((mcc_w256_limb)1 << k) - 1);
			gen_op('&');
		} else { MCC_TRACE("br\n");
			wide256_limb_lval(&r, 1, 0);
			vpushi(sh);
			gen_op(TOK_SHL);
			vpushi(sh);
			gen_op(TOK_SAR);
		}
	}
	vstore();
	vpop();
}

static void bitint128_reduce_limbs(mcc_w256_limb *w, int n, int uns) { MCC_TRACE("enter\n");
	int k, sh;
	mcc_w256_limb mask;

	if (n >= 128)
		{ MCC_TRACE("br\n"); return; }
	if (n <= 64) { MCC_TRACE("br\n");
		/* not expected for slice 2, but keep low n bits of limb0 */
		if (n < 64) { MCC_TRACE("br\n");
			mask = ((mcc_w256_limb)1 << n) - 1;
			w[0] &= mask;
			if (!uns && (w[0] & ((mcc_w256_limb)1 << (n - 1))))
				{ MCC_TRACE("br\n"); w[0] |= ~mask; }
		}
		w[1] = (!uns && (w[0] >> 63)) ? ~(mcc_w256_limb)0 : 0;
		return;
	}
	k = n - 64;
	sh = 64 - k;
	mask = ((mcc_w256_limb)1 << k) - 1;
	if (uns) { MCC_TRACE("br\n");
		w[1] &= mask;
	} else { MCC_TRACE("br\n");
		w[1] &= mask;
		if (w[1] & ((mcc_w256_limb)1 << (k - 1)))
			{ MCC_TRACE("br\n"); w[1] |= ~mask; }
	}
	(void)sh;
}

static void vpush_bitint128_const(CType *vt, mcc_w256_limb w0, mcc_w256_limb w1) { MCC_TRACE("enter\n");
	CValue cv;
	memset(&cv, 0, sizeof cv);
	cv.q.lo = w0;
	cv.q.hi = w1;
	vsetc(vt, VT_CONST, &cv);
}

static void gen_bitint128_op(int op) { MCC_TRACE("enter\n");
	int shift = (op == TOK_SHL || op == TOK_SHR || op == TOK_SAR);
	int cmp = TOK_ISCOND(op);
	int lb, rb, n, uns;
	CType rt;
	SValue res;

	vcheck_cmp();
	vswap();
	vcheck_cmp();
	vswap();

	lb = is_bitint128_type(&vtop[-1].type);
	rb = is_bitint128_type(&vtop[0].type);

	/* A shift whose value operand is not a bitint128 (int << bitint128 count):
	 * demote the count to int and take the ordinary scalar path. */
	if (shift && !lb) { MCC_TRACE("br\n");
		gen_cast_s(VT_INT);
		gen_op(op);
		return;
	}

	if (lb && rb) { MCC_TRACE("br\n");
		int ln = bitint128_prec(&vtop[-1].type), rn = bitint128_prec(&vtop[0].type);
		int lu = bitint128_is_unsigned(&vtop[-1].type);
		int ru = bitint128_is_unsigned(&vtop[0].type);
		/* Usual arithmetic conversions: equal width -> unsigned wins; unequal ->
		 * the wider _BitInt's type (higher rank) wins entirely, signedness incl. */
		if (ln == rn) { MCC_TRACE("br\n"); n = ln; uns = lu || ru; }
		else if (ln > rn) { MCC_TRACE("br\n"); n = ln; uns = lu; }
		else { MCC_TRACE("br\n"); n = rn; uns = ru; }
	} else { MCC_TRACE("br\n");
		CType *b = lb ? &vtop[-1].type : &vtop[0].type;
		CType *o = lb ? &vtop[0].type : &vtop[-1].type;
		int obt = o->t & VT_BTYPE;
		if (is_float(o->t) || !is_integer_btype(obt) || obt == VT_PTR)
			{ MCC_TRACE("br\n"); mcc_error("invalid operand types for a '_BitInt' operation"); }
		n = bitint128_prec(b);
		uns = bitint128_is_unsigned(b);
	}
	if (op == TOK_ULT || op == TOK_UGT || op == TOK_ULE || op == TOK_UGE ||
			op == TOK_UDIV || op == TOK_UMOD)
		{ MCC_TRACE("br\n"); uns = 1; }

	mk_bitint128_type(&rt, uns, n);

	/* Constant fold: both operands bitint128 constants (arith/cmp), or a
	 * bitint128 constant shifted by a constant count. */
	if (wide256_sv_is_const(vtop - 1) &&
			(shift ? (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST
						 : (rb && wide256_sv_is_const(vtop)))) { MCC_TRACE("br\n");
		mcc_w256_limb wa[MCC_WIDE256_LIMBS], wb[MCC_WIDE256_LIMBS], wr[MCC_WIDE256_LIMBS];
		wa[0] = vtop[-1].c.q.lo;
		wa[1] = vtop[-1].c.q.hi;
		wa[2] = wa[3] = (!uns && (wa[1] >> 63)) ? ~(mcc_w256_limb)0 : 0;
		if (shift) { MCC_TRACE("br\n");
			long long sn = (long long)vtop->c.i;
			vpop();
			vpop();
			if (sn < 0 || sn >= 128) { MCC_TRACE("br\n");
				if (op == TOK_SAR && !uns)
					{ MCC_TRACE("br\n"); mcc_w256_sar(wr, wa, 64 * MCC_WIDE256_LIMBS); }
				else
					{ MCC_TRACE("br\n"); mcc_w256_zero(wr); }
			} else if (op == TOK_SHL) { MCC_TRACE("br\n");
				mcc_w256_shl(wr, wa, (unsigned int)sn);
			} else if (uns || op == TOK_SHR) { MCC_TRACE("br\n");
				mcc_w256_shr(wr, wa, (unsigned int)sn);
			} else { MCC_TRACE("br\n");
				mcc_w256_sar(wr, wa, (unsigned int)sn);
			}
			bitint128_reduce_limbs(wr, n, uns);
			vpush_bitint128_const(&rt, wr[0], wr[1]);
			return;
		}
		wb[0] = vtop->c.q.lo;
		wb[1] = vtop->c.q.hi;
		wb[2] = wb[3] = (!uns && (wb[1] >> 63)) ? ~(mcc_w256_limb)0 : 0;
		vpop();
		vpop();
		if (cmp) { MCC_TRACE("br\n");
			int c = uns ? mcc_w256_ucmp(wa, wb) : mcc_w256_scmp(wa, wb);
			vpushi(wide256_cmp_result(op, c));
			return;
		}
		wide256_fold_bin(op, uns, wa, wb, wr);
		bitint128_reduce_limbs(wr, n, uns);
		vpush_bitint128_const(&rt, wr[0], wr[1]);
		return;
	}

	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load time"); }

	if (shift) { MCC_TRACE("br\n");
		vswap();
		bitint128_to_wide256(uns);
		vswap();
		gen_wide256_op(op);
		wide256_to_bitint128(uns, n, &res);
		vpushv(&res);
		return;
	}

	gen_cast(&rt);
	vswap();
	gen_cast(&rt);
	vswap();
	vswap();
	bitint128_to_wide256(uns);
	vswap();
	bitint128_to_wide256(uns);
	gen_wide256_op(op);
	if (cmp) { MCC_TRACE("br\n"); return; }
	wide256_to_bitint128(uns, n, &res);
	vpushv(&res);
}

static void bitint128_from_int(CType *dt, int n, int uns) { MCC_TRACE("enter\n");
	CType u64 = wide256_u64_type(), s64 = wide256_s64_type();
	int su = (vtop->type.t & VT_UNSIGNED) != 0 || (vtop->type.t & VT_BTYPE) == VT_BOOL;
	SValue res;

	if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
		mcc_w256_limb w[MCC_WIDE256_LIMBS];
		mcc_w256_limb v = vtop->c.i;
		if (su)
			{ MCC_TRACE("br\n"); mcc_w256_from_u64(w, v); }
		else
			{ MCC_TRACE("br\n"); mcc_w256_from_i64(w, v); }
		bitint128_reduce_limbs(w, n, uns);
		vpop();
		vpush_bitint128_const(dt, w[0], w[1]);
		return;
	}
	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load time"); }
	gen_cast(su ? &u64 : &s64);
	wide256_local(dt, &res);
	wide256_limb_lval(&res, 0, 1);
	vswap();
	vstore();
	vpop();
	wide256_limb_lval(&res, 1, 1);
	if (su) { MCC_TRACE("br\n");
		vpush64(VT_LLONG | VT_UNSIGNED, 0);
	} else { MCC_TRACE("br\n");
		wide256_limb_lval(&res, 0, 0);
		vpushi(63);
		gen_op(TOK_SAR);
	}
	vstore();
	vpop();
	vpushv(&res);
}

static void gen_bitint128_cast(CType *dt) { MCC_TRACE("enter\n");
	int sb = is_bitint128_type(&vtop->type), db = is_bitint128_type(dt);
	int dbt = dt->t & VT_BTYPE, sbt = vtop->type.t & VT_BTYPE;

	if (sb && db) { MCC_TRACE("br\n");
		int sn = bitint128_prec(&vtop->type), dn = bitint128_prec(dt);
		int duns = bitint128_is_unsigned(dt);
		int suns = bitint128_is_unsigned(&vtop->type);
		if (sn == dn && suns == duns) { MCC_TRACE("br\n");
			vtop->type.t = (vtop->type.t & ~VT_BTYPE) | VT_STRUCT;
			vtop->type.ref = dt->ref;
			vtop->type.bs = (unsigned char)dn;
			return;
		}
		if (wide256_sv_is_const(vtop)) { MCC_TRACE("br\n");
			mcc_w256_limb w[MCC_WIDE256_LIMBS];
			w[0] = vtop->c.q.lo;
			w[1] = vtop->c.q.hi;
			bitint128_reduce_limbs(w, dn, duns);
			vpop();
			vpush_bitint128_const(dt, w[0], w[1]);
			return;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load time"); }
		{
			SValue res;
			bitint128_to_wide256(suns);
			wide256_to_bitint128(duns, dn, &res);
			vpushv(&res);
		}
		return;
	}

	if (sb) { MCC_TRACE("br\n");
		int suns = bitint128_is_unsigned(&vtop->type);
		SValue a;
		if (dbt == VT_VOID) { MCC_TRACE("br\n");
			vtop->type = *dt;
			return;
		}
		if (is_float(dt->t)) { MCC_TRACE("br\n");
			/* extend to __int256 and reuse its correctly-rounded ->float path */
			if (nocode_wanted & DATA_ONLY_WANTED)
				{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at "
													"load time"); }
			bitint128_to_wide256(suns);
			gen_cast(dt);
			return;
		}
		if (!is_integer_btype(dbt) || dbt == VT_PTR)
			{ MCC_TRACE("br\n"); cast_error(&vtop->type, dt); }
		if (wide256_sv_is_const(vtop)) { MCC_TRACE("br\n");
			mcc_w256_limb lo = vtop->c.q.lo, hi = vtop->c.q.hi;
			int nz = (lo != 0 || hi != 0);
			vpop();
			if (dbt == VT_BOOL)
				{ MCC_TRACE("br\n"); vpushi(nz); }
			else
				{ MCC_TRACE("br\n"); vpush64(VT_LLONG | VT_UNSIGNED, lo); }
			gen_cast(dt);
			return;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load "
												"time"); }
		bitint128_materialize(&vtop->type, &a);
		wide256_limb_lval(&a, 0, 1);
		if (dbt == VT_BOOL) { MCC_TRACE("br\n");
			wide256_limb_lval(&a, 1, 1);
			gen_op('|');
			gen_cast(dt);
			vcheck_cmp();
			return;
		}
		gen_cast(dt);
		(void)suns;
		return;
	}

	/* float -> bitint128: truncate to __int256, then reduce to N bits */
	if (is_float(vtop->type.t)) { MCC_TRACE("br\n");
		int duns = bitint128_is_unsigned(dt);
		int dn = bitint128_prec(dt);
		CType w;
		SValue res;
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load "
												"time"); }
		mk_wide256_type(&w, duns);
		gen_cast(&w);
		wide256_to_bitint128(duns, dn, &res);
		vpushv(&res);
		return;
	}
	/* integer -> bitint128 */
	if (!is_integer_btype(sbt) || sbt == VT_PTR)
		{ MCC_TRACE("br\n"); cast_error(&vtop->type, dt); }
	bitint128_from_int(dt, bitint128_prec(dt), bitint128_is_unsigned(dt));
}

static void bitint128_init_putv(void *ptr, SValue *sv) { MCC_TRACE("enter\n");
	write64le((char *)ptr, sv->c.q.lo);
	write64le((char *)ptr + 8, sv->c.q.hi);
}

/* Materialize a non-lvalue bitint128 constant into a stored local before it is
 * copied/loaded, so the store/gv paths see limb bytes (mirrors wide256_deconst). */
static void bitint128_deconst(void) { MCC_TRACE("enter\n");
	SValue res;
	CType vt;

	if ((vtop->type.t & VT_BTYPE) != VT_STRUCT || (vtop->r & VT_LVAL))
		{ MCC_TRACE("br\n"); return; }
	if (!is_bitint128_type(&vtop->type) || !wide256_sv_is_const(vtop))
		{ MCC_TRACE("br\n"); return; }
	if (nocode_wanted & DATA_ONLY_WANTED)
		{ MCC_TRACE("br\n"); return; }
	vt = vtop->type;
	bitint128_materialize(&vt, &res);
	vpushv(&res);
}

#endif
