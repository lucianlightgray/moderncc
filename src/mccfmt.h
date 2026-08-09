#ifndef MCC_FMT_H
#define MCC_FMT_H

#define MCC_FMT_MAXITEM 24
#define MCC_FMT_MAXLIT 192
#define MCC_FMT_MAXARG 8
#define MCC_FMT_NDEC 20
#define MCC_FMT_NHEX 16
#define MCC_FMT_NDEC32 10
#define MCC_FMT_NHEX32 8
#define MCC_FMT_MAXW 32
#define MCC_FMT_MAXSTR 28

#define MCC_FMT_C_BASE 820
#define MCC_FMT_C_BYTE 152
#define MCC_FMT_C_DEC 6900
#define MCC_FMT_C_HEX 4700
#define MCC_FMT_C_DEC32 2400
#define MCC_FMT_C_HEX32 1750
#define MCC_FMT_C_SFIX 130
#define MCC_FMT_C_SBYTE 229
#define MCC_FMT_C_SDYN 14
#define MCC_FMT_MAXCOST 16384

enum {
	MCC_FMT_LIT = 1,
	MCC_FMT_INT = 2,
	MCC_FMT_CHR = 3,
	MCC_FMT_STR = 4,
	MCC_FMT_PREC = 5
};

enum { MCC_FMT_P_NONE = -1, MCC_FMT_P_DYN = -2 };

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
	int prc, left;
} MccFmtItem;

typedef struct MccFmtProg {
	MccFmtItem it[MCC_FMT_MAXITEM];
	int n;
	int narg;
	int refuse;
	int cost;
	char lit[MCC_FMT_MAXLIT];
	int nlit;
} MccFmtProg;

static const char *mcc_fmt_why(int r) {
	switch (r) {
	case MCC_FMT_OK:
		return "ok";
	case MCC_FMT_R_PTR:
		return "%p prints (nil) for NULL on the host and has no device spelling";
	case MCC_FMT_R_FLOAT:
		return "device float formatting is out of scope permanently";
	case MCC_FMT_R_SPEC:
		return "unsupported conversion, flag, precision or signed width";
	default:
		return "the straight-line program does not fit the module budget";
	}
}

static int mcc_fmt_sbytes(const MccFmtItem *it) {
	if (it->prc >= 0 && it->prc < MCC_FMT_MAXSTR)
		return it->prc;
	return MCC_FMT_MAXSTR;
}

static int mcc_fmt_cost(const MccFmtProg *p) {
	int i, c = MCC_FMT_C_BASE;
	for (i = 0; i < p->n; i++) {
		const MccFmtItem *it = &p->it[i];
		switch (it->kind) {
		case MCC_FMT_LIT:
			c += it->llen * MCC_FMT_C_BYTE;
			break;
		case MCC_FMT_CHR:
			c += MCC_FMT_C_BYTE;
			break;
		case MCC_FMT_PREC:
			break;
		case MCC_FMT_STR:
			c += MCC_FMT_C_SFIX +
					 mcc_fmt_sbytes(it) *
							 (MCC_FMT_C_SBYTE +
								(it->prc == MCC_FMT_P_DYN ? MCC_FMT_C_SDYN : 0)) +
					 it->width * MCC_FMT_C_BYTE;
			break;
		default:
			c += (it->base == 10 ? (it->wide ? MCC_FMT_C_DEC : MCC_FMT_C_DEC32)
													: (it->wide ? MCC_FMT_C_HEX : MCC_FMT_C_HEX32)) +
					 it->width * MCC_FMT_C_BYTE;
		}
	}
	return c;
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
		int left = 0, lmod = 0, prc = MCC_FMT_P_NONE;
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
		for (;;) {
			if (*s == '-') {
				left = 1;
				s++;
				continue;
			}
			if (*s == '+' || *s == ' ' || *s == '#') {
				pend = MCC_FMT_R_SPEC;
				s++;
				continue;
			}
			if (*s == '0') {
				zero = 1;
				s++;
				continue;
			}
			break;
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
			s++;
			if (*s == '*') {
				prc = MCC_FMT_P_DYN;
				s++;
			} else {
				prc = 0;
				while (*s >= '0' && *s <= '9') {
					prc = prc * 10 + (*s++ - '0');
					if (prc > MCC_FMT_MAXSTR)
						prc = MCC_FMT_MAXSTR;
				}
			}
		}
		if (s[0] == 'h' && s[1] == 'h')
			lmod = 1, s += 2;
		else if (*s == 'h')
			lmod = 1, s++;
		else if (s[0] == 'l' && s[1] == 'l')
			lmod = wide = 1, s += 2;
		else if (*s == 'l')
			lmod = wide = 1, s++;
		else if (*s == 'z' || *s == 't' || *s == 'j')
			lmod = wide = 1, s++;
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
			kind = MCC_FMT_STR;
			break;
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
		if (kind == MCC_FMT_STR) {
			if (pend || zero || lmod)
				pend = MCC_FMT_R_SPEC;
		} else if (left || prc != MCC_FMT_P_NONE ||
							 (w && (sgn || kind == MCC_FMT_CHR))) {
			pend = MCC_FMT_R_SPEC;
		}
		if (pend) {
			p->refuse = MCC_FMT_R_SPEC;
			return 0;
		}
		if (prc == MCC_FMT_P_DYN) {
			if (p->n >= MCC_FMT_MAXITEM || p->narg >= MCC_FMT_MAXARG) {
				p->refuse = MCC_FMT_R_ROOM;
				return 0;
			}
			o = &p->it[p->n++];
			memset(o, 0, sizeof *o);
			o->kind = MCC_FMT_PREC;
			o->prc = MCC_FMT_P_NONE;
			p->narg++;
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
		o->prc = prc;
		o->left = left;
		p->narg++;
	}
	p->cost = mcc_fmt_cost(p);
	if (p->cost > MCC_FMT_MAXCOST) {
		p->refuse = MCC_FMT_R_ROOM;
		return 0;
	}
	return 1;
}

typedef struct MccFmtSrc {
	const uint32_t *w;
	uint32_t nbyte;
	int64_t base;
} MccFmtSrc;

typedef struct MccFmtDst {
	uint32_t *w;
	uint32_t nbyte;
	uint32_t dst;
	uint32_t size;
	uint32_t pos;
	const uint32_t *sw;
	uint32_t snbyte;
	int64_t sbase;
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
	uint32_t n = (uint32_t)(it->wide
													? (it->base == 10 ? MCC_FMT_NDEC : MCC_FMT_NHEX)
													: (it->base == 10 ? MCC_FMT_NDEC32 : MCC_FMT_NHEX32));
	uint32_t nd = 1, lead, padn = 0, bl, i, j;
	int64_t a = it->wide ? v
											 : (it->sgn ? (int64_t)(int32_t)v : (int64_t)(uint32_t)v);
	int neg = it->sgn && a < 0;
	x[0] = neg ? (uint64_t)0 - (uint64_t)a : (uint64_t)a;
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

static unsigned mcc_fmt_getb(const MccFmtDst *d, uint32_t off) {
	uint32_t u;
	unsigned b;
	int in = d->sw != NULL && off < d->snbyte;
	u = in ? off : 0u;
	b = in ? (unsigned)((d->sw[u >> 2] >> ((u & 3u) * 8u)) & 0xFFu) : 0u;
	return b;
}

static uint32_t mcc_fmt_soff(const MccFmtDst *d, int64_t p) {
	uint64_t x = (uint64_t)p - (uint64_t)d->sbase;
	return (x >> 32) == 0 ? (uint32_t)x : d->snbyte;
}

static void mcc_fmt_str(MccFmtDst *d, const MccFmtItem *it, uint32_t prc,
												int64_t p) {
	unsigned b[MCC_FMT_MAXSTR];
	int g[MCC_FMT_MAXSTR];
	uint32_t soff = mcc_fmt_soff(d, p);
	uint32_t nl = 0, padn = 0, i, j;
	int alive = 1, nb = mcc_fmt_sbytes(it);
	for (i = 0; i < (uint32_t)nb; i++) {
		b[i] = mcc_fmt_getb(d, soff + i);
		alive = alive && b[i] != 0;
		g[i] = alive && i < prc;
		nl += (uint32_t)g[i];
	}
	if (it->width > 0 && (uint32_t)it->width > nl)
		padn = (uint32_t)it->width - nl;
	for (j = 0; j < (uint32_t)it->width; j++) {
		uint32_t q = d->pos + (it->left ? nl + j : j);
		mcc_fmt_putb(d, d->dst + q, ' ', j < padn && q + 1u < d->size);
	}
	for (i = 0; i < (uint32_t)nb; i++) {
		uint32_t q = d->pos + (it->left ? 0u : padn) + i;
		mcc_fmt_putb(d, d->dst + q, b[i], g[i] && q + 1u < d->size);
	}
	d->pos += nl + padn;
}

static uint32_t mcc_fmt_exec(const MccFmtProg *p, uint32_t *w, uint32_t nbyte,
														 uint32_t dst, uint32_t size, const int64_t *arg,
														 int narg, const MccFmtSrc *src) {
	MccFmtDst d;
	uint32_t nul, prc = 0xFFFFFFFFu;
	int i, k, ai = 0;
	d.w = w;
	d.nbyte = nbyte;
	d.dst = dst;
	d.size = size;
	d.pos = 0;
	d.sw = src ? src->w : NULL;
	d.snbyte = src ? src->nbyte : 0u;
	d.sbase = src ? src->base : 0;
	for (i = 0; i < p->n; i++) {
		const MccFmtItem *it = &p->it[i];
		if (it->kind == MCC_FMT_LIT) {
			for (k = 0; k < it->llen; k++)
				mcc_fmt_emit1(&d, (unsigned char)p->lit[it->loff + k]);
			continue;
		}
		if (ai >= narg)
			break;
		if (it->kind == MCC_FMT_PREC) {
			uint32_t v = (uint32_t)(uint64_t)arg[ai++];
			prc = (v & 0x80000000u) ? 0xFFFFFFFFu : v;
		} else if (it->kind == MCC_FMT_STR) {
			mcc_fmt_str(&d, it,
									it->prc == MCC_FMT_P_DYN ? prc
									: it->prc >= 0           ? (uint32_t)it->prc
																						: 0xFFFFFFFFu,
									arg[ai++]);
			prc = 0xFFFFFFFFu;
		} else if (it->kind == MCC_FMT_CHR) {
			mcc_fmt_emit1(&d, (unsigned)((uint64_t)arg[ai++] & 0xFFu));
		} else {
			mcc_fmt_int(&d, it, arg[ai++]);
		}
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

static uint32_t spv_fmt_getb(SpvMod *m, const SpvRegion *s, uint32_t off) {
	uint32_t in = spv_ucmp(m, SpvOpULessThan, off, s->nbyte);
	uint32_t u = spv_usel(m, in, off, spv_uintc(m, 0));
	uint32_t w = spv_emit2(m, SpvOpBitcast, m->id_uint,
												 spv_word_at(m, s->var, spv_region_word(m, s, u, 0)));
	uint32_t sh = spv_uop(m, SpvOpShiftLeftLogical,
												spv_uop(m, SpvOpBitwiseAnd, u, spv_uintc(m, 3)),
												spv_uintc(m, 3));
	uint32_t b = spv_uop(m, SpvOpBitwiseAnd,
											 spv_uop(m, SpvOpShiftRightLogical, w, sh),
											 spv_uintc(m, 0xFF));
	return spv_usel(m, in, b, spv_uintc(m, 0));
}

static uint32_t spv_fmt_soff(SpvMod *m, const SpvRegion *s, SpvV p) {
	SpvV d;
	uint32_t ok;
	spv_widen(m, &p);
	d = spv_sub64(m, p, spv_const64(m, m->mem_base), 1);
	ok = spv_ucmp(m, SpvOpIEqual, spv_hi(m, d.id), spv_uintc(m, 0));
	return spv_usel(m, ok, spv_lo(m, d.id), s->nbyte);
}

static uint32_t spv_fmt_room(SpvMod *m, uint32_t pos, uint32_t size) {
	return spv_ucmp(m, SpvOpULessThan,
									spv_uop(m, SpvOpIAdd, pos, spv_uintc(m, 1)), size);
}

static uint32_t spv_fmt_int(SpvMod *m, const SpvRegion *r, uint32_t dst,
														uint32_t size, uint32_t pos, SpvV v,
														const MccFmtItem *it) {
	SpvV x[MCC_FMT_NDEC + 1], ten;
	uint32_t y[MCC_FMT_NDEC + 1];
	int wide = it->wide;
	int n = wide ? (it->base == 10 ? MCC_FMT_NDEC : MCC_FMT_NHEX)
							 : (it->base == 10 ? MCC_FMT_NDEC32 : MCC_FMT_NHEX32);
	uint32_t nd, lead, padn, bl, neg = 0;
	int i, j;
	ten.id = 0;
	ten.w64 = 0;
	ten.uns = 0;
	lead = spv_uintc(m, 0);
	if (wide) {
		x[0] = v;
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
	} else {
		uint32_t lo = spv_lo(m, spv_pair(m, v));
		y[0] = lo;
		if (it->sgn) {
			neg = spv_ucmp(m, SpvOpUGreaterThanEqual, lo,
										 spv_uintc(m, 0x80000000u));
			y[0] = spv_usel(m, neg,
											spv_uop(m, SpvOpISub, spv_uintc(m, 0), lo), lo);
			lead = spv_usel(m, neg, spv_uintc(m, 1), spv_uintc(m, 0));
		}
		for (i = 1; i <= n; i++)
			y[i] = it->base == 10
								 ? spv_uop(m, SpvOpUDiv, y[i - 1], spv_uintc(m, 10))
								 : spv_uop(m, SpvOpShiftRightLogical, y[i - 1],
													 spv_uintc(m, 4));
	}
	nd = spv_uintc(m, 1);
	for (i = 1; i < n; i++) {
		uint32_t nz = wide ? spv_bool_of_v(m, x[i])
											 : spv_ucmp(m, SpvOpINotEqual, y[i], spv_uintc(m, 0));
		nd = spv_uop(m, SpvOpIAdd, nd,
								 spv_usel(m, nz, spv_uintc(m, 1), spv_uintc(m, 0)));
	}
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
		if (!wide)
			g = it->base == 10
							? spv_uop(m, SpvOpISub, y[i],
												spv_uop(m, SpvOpIMul, y[i + 1], spv_uintc(m, 10)))
							: spv_uop(m, SpvOpBitwiseAnd, y[i], spv_uintc(m, 15));
		else if (it->base == 10)
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

static uint32_t spv_fmt_str(SpvMod *m, const SpvRegion *r, const SpvRegion *s,
														uint32_t dst, uint32_t size, uint32_t pos, SpvV v,
														uint32_t prc, int dyn, const MccFmtItem *it) {
	uint32_t b[MCC_FMT_MAXSTR], g[MCC_FMT_MAXSTR];
	uint32_t soff = spv_fmt_soff(m, s, v);
	uint32_t alive = spv_true(m), nl = spv_uintc(m, 0), padn, q;
	int nb = mcc_fmt_sbytes(it), i, j;
	for (i = 0; i < nb; i++) {
		b[i] = spv_fmt_getb(
				m, s, spv_uop(m, SpvOpIAdd, soff, spv_uintc(m, (uint32_t)i)));
		alive = spv_and(
				m, alive, spv_ucmp(m, SpvOpINotEqual, b[i], spv_uintc(m, 0)));
		g[i] = dyn ? spv_and(m, alive,
												 spv_ucmp(m, SpvOpULessThan, spv_uintc(m, (uint32_t)i),
																	prc))
							 : alive;
		nl = spv_uop(m, SpvOpIAdd, nl,
								 spv_usel(m, g[i], spv_uintc(m, 1), spv_uintc(m, 0)));
	}
	padn = spv_uintc(m, 0);
	if (it->width > 0) {
		uint32_t w = spv_uintc(m, (uint32_t)it->width);
		padn = spv_usel(m, spv_ucmp(m, SpvOpUGreaterThan, w, nl),
										spv_uop(m, SpvOpISub, w, nl), spv_uintc(m, 0));
	}
	for (j = 0; j < it->width; j++) {
		uint32_t cj = spv_uintc(m, (uint32_t)j);
		uint32_t pp = spv_uop(m, SpvOpIAdd, pos,
													it->left ? spv_uop(m, SpvOpIAdd, nl, cj) : cj);
		spv_fmt_putb(m, r, spv_uop(m, SpvOpIAdd, dst, pp), spv_uintc(m, ' '),
								 spv_and(m, spv_ucmp(m, SpvOpULessThan, cj, padn),
												 spv_fmt_room(m, pp, size)));
	}
	q = it->left ? pos : spv_uop(m, SpvOpIAdd, pos, padn);
	for (i = 0; i < nb; i++) {
		uint32_t pp = spv_uop(m, SpvOpIAdd, q, spv_uintc(m, (uint32_t)i));
		spv_fmt_putb(m, r, spv_uop(m, SpvOpIAdd, dst, pp), b[i],
								 spv_and(m, g[i], spv_fmt_room(m, pp, size)));
	}
	return spv_uop(m, SpvOpIAdd, pos, spv_uop(m, SpvOpIAdd, nl, padn));
}

static uint32_t spv_fmt_emit(SpvMod *m, const SpvRegion *r, const SpvRegion *s,
														 const MccFmtProg *p, uint32_t dst, uint32_t size,
														 const SpvV *arg, int narg) {
	uint32_t pos = spv_uintc(m, 0), nul, prc = spv_uintc(m, 0xFFFFFFFFu);
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
		if (it->kind == MCC_FMT_PREC) {
			uint32_t pv = spv_lo(m, spv_pair(m, arg[ai++]));
			prc = spv_usel(m,
											spv_ucmp(m, SpvOpUGreaterThanEqual, pv,
															 spv_uintc(m, 0x80000000u)),
											spv_uintc(m, 0xFFFFFFFFu), pv);
		} else if (it->kind == MCC_FMT_STR) {
			int dyn = it->prc == MCC_FMT_P_DYN;
			pos = spv_fmt_str(m, r, s, dst, size, pos, arg[ai++], prc, dyn, it);
			prc = spv_uintc(m, 0xFFFFFFFFu);
		} else if (it->kind == MCC_FMT_CHR) {
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