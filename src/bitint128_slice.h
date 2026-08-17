#ifndef MCC_BITINT128_SLICE_H
#define MCC_BITINT128_SLICE_H

/* C23 _BitInt(N) slices 2/3: 64 < N <= 256.  A 16-aligned struct of 2 limbs
 * (64 < N <= 128, 16 bytes) or 4 limbs (128 < N <= 256, 32 bytes), tagged
 * a.is_bitint, whose value is reduced to N bits.  The struct layout/ABI is
 * uniform for every N of a given (nlimbs, signedness) pair, so four type
 * instances are cached; the precision N rides the CType .bs channel, with
 * bs == 0 the SENTINEL for N == 256 (the one width that overflows unsigned
 * char).  Arithmetic reuses the tested 4-limb __int256 kernel: extend the
 * stored limbs into an __int256, run gen_wide256_op, then reduce back to N.
 * The 2-limb path keeps its original masked reduce (unchanged, so slice-2
 * codegen is byte-identical); the 4-limb path reduces with the canonical
 * (v << (256-N)) >> (256-N) shift.  The stored form is CANONICAL: bits
 * N..(nlimbs*64-1) = sign(bit N-1) [signed] or 0 [unsigned], matching gcc so
 * casts/reads observe identical bytes. */

static int bitint128_nlimbs_n(int n) { MCC_TRACE("enter\n");
	return n <= 128 ? 2 : 4;
}

static int bitint128_prec(CType *type) { MCC_TRACE("enter\n");
	return type->bs ? type->bs : 256;
}

static int bitint128_nlimbs(CType *type) { MCC_TRACE("enter\n");
	return bitint128_nlimbs_n(bitint128_prec(type));
}

static void mk_bitint128_type(CType *type, int is_unsigned, int n) { MCC_TRACE("enter\n");
	Sym *s, *f, *prev = NULL;
	AttributeDef ad;
	int i, nl = bitint128_nlimbs_n(n);
	int idx = (nl == 4 ? 2 : 0) + (is_unsigned ? 1 : 0);
	int lt = VT_LLONG | (is_unsigned ? VT_UNSIGNED : 0);
	unsigned char bs = (unsigned char)(n >= 256 ? 0 : n);

	if (mcc_state->gen_bitint128_type_cache[idx].ref) { MCC_TRACE("br\n");
		*type = mcc_state->gen_bitint128_type_cache[idx];
		type->bs = bs;
		return;
	}
	if (!mcc_state->gen_bitint128_limb_tok[0]) { MCC_TRACE("br\n");
		for (i = 0; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
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

static void bitint128_materialize(CType *vt, SValue *out) { MCC_TRACE("enter\n");
	int i, nl = bitint128_nlimbs(vt);

	if (wide256_sv_is_stable_lval(vtop)) { MCC_TRACE("br\n");
		*out = *vtop;
		vpop();
		return;
	}
	if (wide256_sv_is_const(vtop)) { MCC_TRACE("br\n");
		mcc_w256_limb w[MCC_WIDE256_LIMBS];
		w[0] = vtop->c.q.lo;
		w[1] = vtop->c.q.hi;
		w[2] = vtop->c.q.w2;
		w[3] = vtop->c.q.w3;
		vpop();
		wide256_local(vt, out);
		for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
			wide256_limb_lval(out, i, 1);
			vpush64(VT_LLONG | VT_UNSIGNED, w[i]);
			vstore();
			vpop();
		}
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
	int i, nl = bitint128_nlimbs(&bt);

	bitint128_materialize(&bt, &a);
	mk_wide256_type(&wt, uns);
	wide256_local(&wt, &res);
	for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
		wide256_limb_lval(&res, i, 1);
		wide256_limb_lval(&a, i, 1);
		vstore();
		vpop();
	}
	for (i = nl; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
		wide256_limb_lval(&res, i, 1);
		if (uns) { MCC_TRACE("br\n");
			vpush64(VT_LLONG | VT_UNSIGNED, 0);
		} else { MCC_TRACE("br\n");
			wide256_limb_lval(&a, nl - 1, 0);
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
	int i, nl = bitint128_nlimbs_n(n);

	if (nl == 4 && n < 256) { MCC_TRACE("br\n");
		int sh = 256 - n;
		vpushi(sh);
		gen_wide256_op(TOK_SHL);
		vpushi(sh);
		gen_wide256_op(uns ? TOK_SHR : TOK_SAR);
	}
	wide256_materialize(&wt, &r);
	mk_bitint128_type(&bt, uns, n);
	wide256_local(&bt, out);
	if (nl == 2) { MCC_TRACE("br\n");
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
	} else { MCC_TRACE("br\n");
		for (i = 0; i < MCC_WIDE256_LIMBS; i++) { MCC_TRACE("br\n");
			wide256_limb_lval(out, i, 1);
			wide256_limb_lval(&r, i, 1);
			vstore();
			vpop();
		}
	}
}

/* Canonicalize a 4-limb value to N bits in place: bits N.. become sign/zero. */
static void bitint128_reduce_limbs(mcc_w256_limb *w, int n, int uns) { MCC_TRACE("enter\n");
	mcc_w256_limb tmp[MCC_WIDE256_LIMBS];
	int k, sh;
	mcc_w256_limb mask;

	if (n >= 256)
		{ MCC_TRACE("br\n"); return; }
	if (n <= 64) { MCC_TRACE("br\n");
		/* not expected for a 2/4-limb bitint, but keep low n bits of limb0 */
		if (n < 64) { MCC_TRACE("br\n");
			mask = ((mcc_w256_limb)1 << n) - 1;
			w[0] &= mask;
			if (!uns && (w[0] & ((mcc_w256_limb)1 << (n - 1))))
				{ MCC_TRACE("br\n"); w[0] |= ~mask; }
		}
		w[1] = w[2] = w[3] = (!uns && (w[0] >> 63)) ? ~(mcc_w256_limb)0 : 0;
		return;
	}
	if (n <= 128) { MCC_TRACE("br\n");
		/* slice-2 shape kept identical: reduce limb1, sign/zero limbs 2..3 */
		k = n - 64;
		mask = (k == 64) ? ~(mcc_w256_limb)0 : (((mcc_w256_limb)1 << k) - 1);
		if (uns) { MCC_TRACE("br\n");
			w[1] &= mask;
		} else { MCC_TRACE("br\n");
			w[1] &= mask;
			if (w[1] & ((mcc_w256_limb)1 << (k - 1)))
				{ MCC_TRACE("br\n"); w[1] |= ~mask; }
		}
		w[2] = w[3] = (!uns && (w[1] >> 63)) ? ~(mcc_w256_limb)0 : 0;
		return;
	}
	/* 128 < n < 256: canonical shift reduce over the full __int256. */
	sh = 256 - n;
	mcc_w256_shl(tmp, w, (unsigned int)sh);
	if (uns)
		{ MCC_TRACE("br\n"); mcc_w256_shr(w, tmp, (unsigned int)sh); }
	else
		{ MCC_TRACE("br\n"); mcc_w256_sar(w, tmp, (unsigned int)sh); }
	(void)k;
}

static void vpush_bitint128_const(CType *vt, const mcc_w256_limb *w) { MCC_TRACE("enter\n");
	CValue cv;
	memset(&cv, 0, sizeof cv);
	cv.q.lo = w[0];
	cv.q.hi = w[1];
	cv.q.w2 = w[2];
	cv.q.w3 = w[3];
	vsetc(vt, VT_CONST, &cv);
}

static void bitint128_sv_limbs(SValue *sv, mcc_w256_limb *w) { MCC_TRACE("enter\n");
	w[0] = sv->c.q.lo;
	w[1] = sv->c.q.hi;
	w[2] = sv->c.q.w2;
	w[3] = sv->c.q.w3;
}

/* Read a CONSTANT operand of a bitint128 op into 4 canonical limbs.  A
 * bitint128 const already carries 4 canonical limbs in c.q; any OTHER const
 * (a scalar int, or a slice-1 _BitInt with only c.i set -- e.g. the int 0 that
 * unary minus feeds as `0 - x`) has garbage in c.q.hi/w2/w3, so its single
 * value limb must be sign/zero-extended instead of read raw. */
static void bitint128_read_operand(SValue *sv, mcc_w256_limb *w) { MCC_TRACE("enter\n");
	if (is_bitint128_type(&sv->type)) { MCC_TRACE("br\n");
		bitint128_sv_limbs(sv, w);
		return;
	}
	if ((sv->type.t & VT_UNSIGNED) || (sv->type.t & VT_BTYPE) == VT_BOOL)
		{ MCC_TRACE("br\n"); mcc_w256_from_u64(w, sv->c.i); }
	else
		{ MCC_TRACE("br\n"); mcc_w256_from_i64(w, sv->c.i); }
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
		bitint128_read_operand(vtop - 1, wa);
		if (shift) { MCC_TRACE("br\n");
			long long sn = (long long)vtop->c.i;
			vpop();
			vpop();
			if (sn < 0 || sn >= 256) { MCC_TRACE("br\n");
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
			vpush_bitint128_const(&rt, wr);
			return;
		}
		bitint128_read_operand(vtop, wb);
		vpop();
		vpop();
		if (cmp) { MCC_TRACE("br\n");
			int c = uns ? mcc_w256_ucmp(wa, wb) : mcc_w256_scmp(wa, wb);
			vpushi(wide256_cmp_result(op, c));
			return;
		}
		wide256_fold_bin(op, uns, wa, wb, wr);
		bitint128_reduce_limbs(wr, n, uns);
		vpush_bitint128_const(&rt, wr);
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
	int i, nl = bitint128_nlimbs_n(n);
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
		vpush_bitint128_const(dt, w);
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
	for (i = 1; i < nl; i++) { MCC_TRACE("br\n");
		wide256_limb_lval(&res, i, 1);
		if (su) { MCC_TRACE("br\n");
			vpush64(VT_LLONG | VT_UNSIGNED, 0);
		} else { MCC_TRACE("br\n");
			wide256_limb_lval(&res, 0, 0);
			vpushi(63);
			gen_op(TOK_SAR);
		}
		vstore();
		vpop();
	}
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
			vtop->type.bs = (unsigned char)(dn >= 256 ? 0 : dn);
			return;
		}
		if (wide256_sv_is_const(vtop)) { MCC_TRACE("br\n");
			mcc_w256_limb w[MCC_WIDE256_LIMBS];
			bitint128_sv_limbs(vtop, w);
			bitint128_reduce_limbs(w, dn, duns);
			vpop();
			vpush_bitint128_const(dt, w);
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
		int i, nl = bitint128_nlimbs(&vtop->type);
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
			mcc_w256_limb w[MCC_WIDE256_LIMBS];
			int nz;
			bitint128_sv_limbs(vtop, w);
			nz = (w[0] != 0 || w[1] != 0 || w[2] != 0 || w[3] != 0);
			vpop();
			if (dbt == VT_BOOL)
				{ MCC_TRACE("br\n"); vpushi(nz); }
			else
				{ MCC_TRACE("br\n"); vpush64(VT_LLONG | VT_UNSIGNED, w[0]); }
			gen_cast(dt);
			return;
		}
		if (nocode_wanted & DATA_ONLY_WANTED)
			{ MCC_TRACE("br\n"); mcc_error("initializer element is not computable at load "
												"time"); }
		bitint128_materialize(&vtop->type, &a);
		wide256_limb_lval(&a, 0, 1);
		if (dbt == VT_BOOL) { MCC_TRACE("br\n");
			for (i = 1; i < nl; i++) { MCC_TRACE("br\n");
				wide256_limb_lval(&a, i, 1);
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
	int i, nl = bitint128_nlimbs(&sv->type);
	mcc_w256_limb w[MCC_WIDE256_LIMBS];

	bitint128_sv_limbs(sv, w);
	for (i = 0; i < nl; i++) { MCC_TRACE("br\n");
		write64le((char *)ptr + i * 8, w[i]);
	}
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
