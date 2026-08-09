#ifndef MCC_FMT_H
#define MCC_FMT_H

#define MCC_FMT_MAXITEM 24
#define MCC_FMT_MAXLIT 192
#define MCC_FMT_MAXARG 8
#define MCC_FMT_NDEC 20
#define MCC_FMT_NHEX 16
#define MCC_FMT_MAXW 32

enum { MCC_FMT_LIT = 1, MCC_FMT_INT = 2, MCC_FMT_CHR = 3 };

enum {
	MCC_FMT_OK = 0,
	MCC_FMT_R_PTR = 1,
	MCC_FMT_R_FLOAT = 2,
	MCC_FMT_R_SPEC = 3,
	MCC_FMT_R_ROOM = 4
};

typedef struct MccFmtItem {
	int kind;
	int loff, llen;
	int base, ucase, sgn, wide, width, pad;
} MccFmtItem;

typedef struct MccFmtProg {
	MccFmtItem it[MCC_FMT_MAXITEM];
	int n;
	int narg;
	int refuse;
	char lit[MCC_FMT_MAXLIT];
	int nlit;
} MccFmtProg;

static const char *mcc_fmt_why(int r) {
	switch (r) {
	case MCC_FMT_OK:
		return "ok";
	case MCC_FMT_R_PTR:
		return "%s/%p needs a device pointer value, which is board item 1";
	case MCC_FMT_R_FLOAT:
		return "device float formatting is out of scope permanently";
	case MCC_FMT_R_SPEC:
		return "unsupported conversion, flag, precision or signed width";
	default:
		return "format program does not fit";
	}
}

static int mcc_fmt_lit(MccFmtProg *p, int c) {
	MccFmtItem *o;
	if (p->nlit >= MCC_FMT_MAXLIT)
		return 0;
	o = p->n > 0 ? &p->it[p->n - 1] : NULL;
	if (o && o->kind == MCC_FMT_LIT && o->loff + o->llen == p->nlit) {
		p->lit[p->nlit++] = (char)c;
		o->llen++;
		return 1;
	}
	if (p->n >= MCC_FMT_MAXITEM)
		return 0;
	o = &p->it[p->n++];
	o->kind = MCC_FMT_LIT;
	o->loff = p->nlit;
	o->llen = 1;
	p->lit[p->nlit++] = (char)c;
	return 1;
}

static int mcc_fmt_compile(const char *f, MccFmtProg *p) {
	const char *s = f;
	memset(p, 0, sizeof *p);
	if (!f) {
		p->refuse = MCC_FMT_R_SPEC;
		return 0;
	}
	while (*s) {
		MccFmtItem *o;
		int w = 0, zero = 0, wide = 0, base = 0, sgn = 0, ucase = 0;
		int kind = MCC_FMT_INT, pend = MCC_FMT_OK;
		if (*s != '%') {
			if (!mcc_fmt_lit(p, (unsigned char)*s++)) {
				p->refuse = MCC_FMT_R_ROOM;
				return 0;
			}
			continue;
		}
		s++;
		if (*s == '%') {
			s++;
			if (!mcc_fmt_lit(p, '%')) {
				p->refuse = MCC_FMT_R_ROOM;
				return 0;
			}
			continue;
		}
		while (*s == '-' || *s == '+' || *s == ' ' || *s == '#') {
			pend = MCC_FMT_R_SPEC;
			s++;
		}
		while (*s == '0') {
			zero = 1;
			s++;
		}
		if (*s == '*') {
			pend = MCC_FMT_R_SPEC;
			s++;
		} else {
			while (*s >= '0' && *s <= '9') {
				w = w * 10 + (*s++ - '0');
				if (w > MCC_FMT_MAXW)
					pend = MCC_FMT_R_SPEC;
			}
		}
		if (*s == '.') {
			pend = MCC_FMT_R_SPEC;
			s++;
			if (*s == '*')
				s++;
			while (*s >= '0' && *s <= '9')
				s++;
		}
		if (s[0] == 'h' && s[1] == 'h')
			s += 2;
		else if (*s == 'h')
			s++;
		else if (s[0] == 'l' && s[1] == 'l')
			wide = 1, s += 2;
		else if (*s == 'l')
			wide = 1, s++;
		else if (*s == 'z' || *s == 't' || *s == 'j')
			wide = 1, s++;
		else if (*s == 'L') {
			p->refuse = MCC_FMT_R_FLOAT;
			return 0;
		}
		switch (*s) {
		case 'd':
		case 'i':
			base = 10;
			sgn = 1;
			break;
		case 'u':
			base = 10;
			break;
		case 'x':
			base = 16;
			break;
		case 'X':
			base = 16;
			ucase = 1;
			break;
		case 'c':
			kind = MCC_FMT_CHR;
			break;
		case 's':
		case 'p':
			p->refuse = MCC_FMT_R_PTR;
			return 0;
		case 'e':
		case 'E':
		case 'f':
		case 'F':
		case 'g':
		case 'G':
		case 'a':
		case 'A':
			p->refuse = MCC_FMT_R_FLOAT;
			return 0;
		default:
			p->refuse = MCC_FMT_R_SPEC;
			return 0;
		}
		s++;
		if (pend || (w && (sgn || kind == MCC_FMT_CHR))) {
			p->refuse = MCC_FMT_R_SPEC;
			return 0;
		}
		if (p->n >= MCC_FMT_MAXITEM || p->narg >= MCC_FMT_MAXARG) {
			p->refuse = MCC_FMT_R_ROOM;
			return 0;
		}
		o = &p->it[p->n++];
		o->kind = kind;
		o->loff = o->llen = 0;
		o->base = base;
		o->ucase = ucase;
		o->sgn = sgn;
		o->wide = wide;
		o->width = w;
		o->pad = zero ? '0' : ' ';
		p->narg++;
	}
	return 1;
}

typedef struct MccFmtDst {
	uint32_t *w;
	uint32_t nbyte;
	uint32_t dst;
	uint32_t size;
	uint32_t pos;
} MccFmtDst;

static void mcc_fmt_putb(MccFmtDst *d, uint32_t off, unsigned b, int wr) {
	uint32_t u, sh, keep;
	int go = wr && off < d->nbyte;
	u = go ? off : 0u;
	sh = (u & 3u) * 8u;
	keep = go ? (0xFFu << sh) : 0u;
	d->w[u >> 2] = (d->w[u >> 2] & ~keep) | (((b & 0xFFu) << sh) & keep);
}

static void mcc_fmt_emit1(MccFmtDst *d, unsigned b) {
	mcc_fmt_putb(d, d->dst + d->pos, b, d->pos + 1u < d->size);
	d->pos++;
}

static void mcc_fmt_int(MccFmtDst *d, const MccFmtItem *it, int64_t v) {
	uint64_t x[MCC_FMT_NDEC + 1];
	uint32_t n = (uint32_t)(it->base == 10 ? MCC_FMT_NDEC : MCC_FMT_NHEX);
	uint32_t nd = 1, lead, padn = 0, bl, i, j;
	int neg = it->sgn && v < 0;
	x[0] = neg ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
	for (i = 1; i <= n; i++)
		x[i] = it->base == 10 ? x[i - 1] / 10u : x[i - 1] >> 4;
	for (i = 1; i < n; i++)
		if (x[i])
			nd++;
	lead = neg ? 1u : 0u;
	bl = lead + nd;
	if (it->width > 0 && (uint32_t)it->width > bl)
		padn = (uint32_t)it->width - bl;
	if (it->sgn)
		mcc_fmt_putb(d, d->dst + d->pos, '-', neg && d->pos + 1u < d->size);
	for (j = 0; j < (uint32_t)it->width; j++) {
		uint32_t p = d->pos + lead + j;
		mcc_fmt_putb(d, d->dst + p, (unsigned)it->pad,
								 j < padn && p + 1u < d->size);
	}
	for (i = 0; i < n; i++) {
		uint32_t g = (uint32_t)(it->base == 10 ? x[i] - 10u * x[i + 1]
																					: (x[i] & 15u));
		uint32_t p = d->pos + lead + padn + (nd - 1u - i);
		unsigned c = g < 10u ? (unsigned)'0' + g
												 : g + (unsigned)(it->ucase ? 'A' - 10 : 'a' - 10);
		mcc_fmt_putb(d, d->dst + p, c, i < nd && p + 1u < d->size);
	}
	d->pos += lead + padn + nd;
}

static uint32_t mcc_fmt_exec(const MccFmtProg *p, uint32_t *w, uint32_t nbyte,
														 uint32_t dst, uint32_t size, const int64_t *arg,
														 int narg) {
	MccFmtDst d;
	uint32_t nul;
	int i, k, ai = 0;
	d.w = w;
	d.nbyte = nbyte;
	d.dst = dst;
	d.size = size;
	d.pos = 0;
	for (i = 0; i < p->n; i++) {
		const MccFmtItem *it = &p->it[i];
		if (it->kind == MCC_FMT_LIT) {
			for (k = 0; k < it->llen; k++)
				mcc_fmt_emit1(&d, (unsigned char)p->lit[it->loff + k]);
			continue;
		}
		if (ai >= narg)
			break;
		if (it->kind == MCC_FMT_CHR)
			mcc_fmt_emit1(&d, (unsigned)((uint64_t)arg[ai++] & 0xFFu));
		else
			mcc_fmt_int(&d, it, arg[ai++]);
	}
	nul = d.pos + 1u < size ? d.pos : size - 1u;
	mcc_fmt_putb(&d, dst + nul, 0, size != 0u);
	return d.pos;
}

#if defined(MCC_GPU_EMITTER) && !MCC_GPU_LANG_MSL

static void spv_fmt_putb(SpvMod *m, const SpvRegion *r, uint32_t off,
												 uint32_t byte, uint32_t wr) {
	uint32_t go, u, wi, sh, keep, msk, cur, nw;
	if (r->shared) {
		m->failed = 1;
		return;
	}
	go = spv_and(m, wr, spv_ucmp(m, SpvOpULessThan, off, r->nbyte));
	u = spv_usel(m, go, off, spv_uintc(m, 0));
	wi = spv_region_word(m, r, u, 0);
	sh = spv_uop(m, SpvOpShiftLeftLogical,
							 spv_uop(m, SpvOpBitwiseAnd, u, spv_uintc(m, 3)),
							 spv_uintc(m, 3));
	keep = spv_uop(m, SpvOpShiftLeftLogical, spv_uintc(m, 0xFF), sh);
	msk = spv_usel(m, go, keep, spv_uintc(m, 0));
	cur = spv_emit2(m, SpvOpBitcast, m->id_uint, spv_word_at(m, r->var, wi));
	nw = spv_uop(
			m, SpvOpBitwiseOr,
			spv_uop(m, SpvOpBitwiseAnd, cur,
							spv_emit2(m, SpvOpNot, m->id_uint, msk)),
			spv_uop(m, SpvOpBitwiseAnd,
							spv_uop(m, SpvOpShiftLeftLogical,
											spv_uop(m, SpvOpBitwiseAnd, byte, spv_uintc(m, 0xFF)),
											sh),
							msk));
	spv_word_set(m, r->var, wi, spv_emit2(m, SpvOpBitcast, m->id_int, nw));
}

static uint32_t spv_fmt_room(SpvMod *m, uint32_t pos, uint32_t size) {
	return spv_ucmp(m, SpvOpULessThan,
									spv_uop(m, SpvOpIAdd, pos, spv_uintc(m, 1)), size);
}

static uint32_t spv_fmt_int(SpvMod *m, const SpvRegion *r, uint32_t dst,
														uint32_t size, uint32_t pos, SpvV v,
														const MccFmtItem *it) {
	SpvV x[MCC_FMT_NDEC + 1], ten;
	int n = it->base == 10 ? MCC_FMT_NDEC : MCC_FMT_NHEX;
	uint32_t nd, lead, padn, bl, neg = 0;
	int i, j;
	x[0] = v;
	lead = spv_uintc(m, 0);
	if (it->sgn) {
		neg = spv_sign64(m, v);
		x[0] = spv_sel64(m, neg, spv_neg64(m, v), v);
		lead = spv_usel(m, neg, spv_uintc(m, 1), spv_uintc(m, 0));
	}
	ten = spv_const64(m, 10);
	for (i = 1; i <= n; i++)
		x[i] = it->base == 10
							 ? spv_udiv64(m, x[i - 1], ten, 1)
							 : spv_shift64(m, SpvOpShiftRightLogical, x[i - 1],
														 spv_uintc(m, 4), 1);
	nd = spv_uintc(m, 1);
	for (i = 1; i < n; i++)
		nd = spv_uop(m, SpvOpIAdd, nd,
								 spv_usel(m, spv_bool_of_v(m, x[i]), spv_uintc(m, 1),
													spv_uintc(m, 0)));
	bl = spv_uop(m, SpvOpIAdd, lead, nd);
	padn = spv_uintc(m, 0);
	if (it->width > 0) {
		uint32_t w = spv_uintc(m, (uint32_t)it->width);
		padn = spv_usel(m, spv_ucmp(m, SpvOpUGreaterThan, w, bl),
										spv_uop(m, SpvOpISub, w, bl), spv_uintc(m, 0));
	}
	if (it->sgn)
		spv_fmt_putb(m, r, spv_uop(m, SpvOpIAdd, dst, pos), spv_uintc(m, '-'),
								 spv_and(m, neg, spv_fmt_room(m, pos, size)));
	for (j = 0; j < it->width; j++) {
		uint32_t p = spv_uop(m, SpvOpIAdd, spv_uop(m, SpvOpIAdd, pos, lead),
												 spv_uintc(m, (uint32_t)j));
		spv_fmt_putb(m, r, spv_uop(m, SpvOpIAdd, dst, p),
								 spv_uintc(m, (uint32_t)it->pad),
								 spv_and(m,
												 spv_ucmp(m, SpvOpULessThan, spv_uintc(m, (uint32_t)j),
																	padn),
												 spv_fmt_room(m, p, size)));
	}
	for (i = 0; i < n; i++) {
		uint32_t g, c, p, lt;
		if (it->base == 10)
			g = spv_lo(m, spv_sub64(m, x[i], spv_mul64(m, x[i + 1], ten, 1), 1).id);
		else
			g = spv_uop(m, SpvOpBitwiseAnd, spv_lo(m, x[i].id), spv_uintc(m, 15));
		lt = spv_ucmp(m, SpvOpULessThan, g, spv_uintc(m, 10));
		c = spv_uop(m, SpvOpIAdd, g,
								spv_usel(m, lt, spv_uintc(m, '0'),
												 spv_uintc(m, (uint32_t)(it->ucase ? 'A' - 10
																													: 'a' - 10))));
		p = spv_uop(m, SpvOpIAdd,
								spv_uop(m, SpvOpIAdd, pos, spv_uop(m, SpvOpIAdd, lead, padn)),
								spv_uop(m, SpvOpISub, spv_uop(m, SpvOpISub, nd, spv_uintc(m, 1)),
												spv_uintc(m, (uint32_t)i)));
		spv_fmt_putb(m, r, spv_uop(m, SpvOpIAdd, dst, p), c,
								 spv_and(m,
												 spv_ucmp(m, SpvOpULessThan, spv_uintc(m, (uint32_t)i),
																	nd),
												 spv_fmt_room(m, p, size)));
	}
	return spv_uop(m, SpvOpIAdd, pos,
								 spv_uop(m, SpvOpIAdd, bl, padn));
}

static uint32_t spv_fmt_emit(SpvMod *m, const SpvRegion *r, const MccFmtProg *p,
														 uint32_t dst, uint32_t size, const SpvV *arg,
														 int narg) {
	uint32_t pos = spv_uintc(m, 0), nul;
	int i, k, ai = 0;
	for (i = 0; i < p->n && !m->failed; i++) {
		const MccFmtItem *it = &p->it[i];
		if (it->kind == MCC_FMT_LIT) {
			for (k = 0; k < it->llen; k++) {
				uint32_t q = spv_uop(m, SpvOpIAdd, pos, spv_uintc(m, (uint32_t)k));
				spv_fmt_putb(m, r, spv_uop(m, SpvOpIAdd, dst, q),
										 spv_uintc(m, (unsigned char)p->lit[it->loff + k]),
										 spv_fmt_room(m, q, size));
			}
			pos = spv_uop(m, SpvOpIAdd, pos, spv_uintc(m, (uint32_t)it->llen));
			continue;
		}
		if (ai >= narg)
			break;
		if (it->kind == MCC_FMT_CHR) {
			spv_fmt_putb(m, r, spv_uop(m, SpvOpIAdd, dst, pos),
									 spv_lo(m, spv_pair(m, arg[ai++])),
									 spv_fmt_room(m, pos, size));
			pos = spv_uop(m, SpvOpIAdd, pos, spv_uintc(m, 1));
		} else {
			pos = spv_fmt_int(m, r, dst, size, pos, arg[ai++], it);
		}
	}
	nul = spv_usel(m, spv_fmt_room(m, pos, size), pos,
								 spv_uop(m, SpvOpISub, size, spv_uintc(m, 1)));
	spv_fmt_putb(m, r, spv_uop(m, SpvOpIAdd, dst, nul), spv_uintc(m, 0),
							 spv_ucmp(m, SpvOpINotEqual, size, spv_uintc(m, 0)));
	return pos;
}

#endif 
#endif 