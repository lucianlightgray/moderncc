#include "mcc.h"

#ifdef MCC_TARGET_ARM

static const char *const cc_sfx[16] = {
		"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
		"hi", "ls", "ge", "lt", "gt", "le", "", ""};

static const char *const regnm[16] = {
		"r0", "r1", "r2", "r3", "r4", "r5", "r6", "r7",
		"r8", "r9", "r10", "r11", "r12", "sp", "lr", "pc"};

static const char *const dpnm[16] = {
		"and", "eor", "sub", "rsb", "add", "adc", "sbc", "rsc",
		"tst", "teq", "cmp", "cmn", "orr", "mov", "bic", "mvn"};

static const char *const shnm[4] = {"lsl", "lsr", "asr", "ror"};

typedef struct
{
	char *p;
	char *end;
} SB;

static void sb_init(SB *b, char *buf, int size) { MCC_TRACE("enter\n");
	b->p = buf;
	b->end = buf + size;
	buf[0] = 0;
}

static void sb(SB *b, const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	int n;
	if (b->p >= b->end)
		{ MCC_TRACE("br\n"); return; }
	va_start(ap, fmt);
	n = vsnprintf(b->p, b->end - b->p, fmt, ap);
	va_end(ap);
	if (n > 0)
		{ MCC_TRACE("br\n"); b->p += (n < b->end - b->p) ? n : (int)(b->end - b->p); }
}

static uint32_t ror32(uint32_t v, unsigned r) { MCC_TRACE("enter\n");
	r &= 31;
	return r ? (v >> r) | (v << (32 - r)) : v;
}

static uint32_t rol32(uint32_t v, unsigned r) { MCC_TRACE("enter\n");
	r &= 31;
	return r ? (v << r) | (v >> (32 - r)) : v;
}

static int imm_canonical(uint32_t rot, uint32_t imm8) { MCC_TRACE("enter\n");
	uint32_t v = ror32(imm8, 2 * rot);
	uint32_t r;
	for (r = 0; r < 16; r++)
		{ MCC_TRACE("br\n"); if (rol32(v, 2 * r) < 0x100)
			{ MCC_TRACE("br\n"); return r == rot; } }
	return 0;
}

static void sb_imm(SB *b, uint32_t v) { MCC_TRACE("enter\n");
	if (v < 1024)
		{ MCC_TRACE("br\n"); sb(b, "#%u", v); }
	else
		{ MCC_TRACE("br\n"); sb(b, "#0x%x", v); }
}

static int sb_shift_op(SB *b, uint32_t w, int allow_regshift) { MCC_TRACE("enter\n");
	int rm = w & 15, ty = (w >> 5) & 3;
	if (w & 0x10) { MCC_TRACE("br\n");
		if (!allow_regshift || (w & 0x80))
			{ MCC_TRACE("br\n"); return 0; }
		sb(b, "%s, %s %s", regnm[rm], shnm[ty], regnm[(w >> 8) & 15]);
	} else { MCC_TRACE("br\n");
		int amt = (w >> 7) & 31;
		if (amt == 0) { MCC_TRACE("br\n");
			if (ty == 0)
				{ MCC_TRACE("br\n"); sb(b, "%s", regnm[rm]); }
			else if (ty == 3)
				{ MCC_TRACE("br\n"); sb(b, "%s, rrx", regnm[rm]); }
			else
				{ MCC_TRACE("br\n"); return 0; }
		} else { MCC_TRACE("br\n");
			sb(b, "%s, %s #%d", regnm[rm], shnm[ty], amt);
		}
	}
	return 1;
}

static void sb_reglist(SB *b, uint32_t mask) { MCC_TRACE("enter\n");
	int i, first = 1;
	sb(b, "{");
	for (i = 0; i < 16; i++)
		{ MCC_TRACE("br\n"); if (mask & (1u << i)) { MCC_TRACE("br\n");
			sb(b, "%s%s", first ? "" : ", ", regnm[i]);
			first = 0;
		} }
	sb(b, "}");
}

static int dis_branch(disasm_ctx *dc, uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	const char *nm = (w & 0x01000000) ? "bl" : "b";
	int32_t off = w & 0xffffff;
	int rtype = 0;
	const char *rel;
	if (off & 0x800000)
		{ MCC_TRACE("br\n"); off -= 0x1000000; }
	rel = disasm_reloc(dc, dc->pc, 4, &rtype);
	if (rel && (rtype == R_ARM_PC24 || rtype == R_ARM_CALL || rtype == R_ARM_JUMP24 || rtype == R_ARM_PLT32)) { MCC_TRACE("br\n");
		long v = (long)off * 4 + 8;
		if (v == 0)
			{ MCC_TRACE("br\n"); sb(b, "%s%s\t%s", nm, cc, rel); }
		else if (v > 0)
			{ MCC_TRACE("br\n"); sb(b, "%s%s\t%s+%ld", nm, cc, rel, v); }
		else
			{ MCC_TRACE("br\n"); sb(b, "%s%s\t%s-%ld", nm, cc, rel, -v); }
		return 1;
	}
	if (rel)
		{ MCC_TRACE("br\n"); return 0; }
	{
		addr_t target = dc->pc + 8 + (addr_t)((long)off * 4);
		if (target >= dc->size || (target & 3))
			{ MCC_TRACE("br\n"); return 0; }
		sb(b, "%s%s\t%s", nm, cc, disasm_label(dc, target));
	}
	return 1;
}

static int dis_dp(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int I = (w >> 25) & 1, opc = (w >> 21) & 15, S = (w >> 20) & 1;
	int rn = (w >> 16) & 15, rd = (w >> 12) & 15;
	char o2buf[64];
	SB o2;

	if ((w & 0x0FF00000) == 0x03000000 || (w & 0x0FF00000) == 0x03400000) { MCC_TRACE("br\n");
		uint32_t imm16 = ((w >> 4) & 0xf000) | (w & 0xfff);
		sb(b, "%s%s\t%s, ", (w & 0x00400000) ? "movt" : "movw", cc, regnm[rd]);
		sb_imm(b, imm16);
		return 1;
	}

	if (!S && opc >= 8 && opc <= 11)
		{ MCC_TRACE("br\n"); return 0; }

	sb_init(&o2, o2buf, sizeof o2buf);
	if (I) { MCC_TRACE("br\n");
		uint32_t rot = (w >> 8) & 15, imm8 = w & 0xff;
		if (!imm_canonical(rot, imm8))
			{ MCC_TRACE("br\n"); return 0; }
		sb_imm(&o2, ror32(imm8, 2 * rot));
	} else { MCC_TRACE("br\n");
		if (!sb_shift_op(&o2, w, 1))
			{ MCC_TRACE("br\n"); return 0; }
	}

	if (!I && (w & 0x10)) { MCC_TRACE("br\n");
		int bad = ((w >> 8) & 15) == 15;
		if (opc == 13)
			{ MCC_TRACE("br\n"); bad |= rd == 15 || (w & 15) == 15; }
		else if (opc == 15)
			{ MCC_TRACE("br\n"); bad |= rd == 15; }
		else if (opc >= 8 && opc <= 11)
			{ MCC_TRACE("br\n"); bad |= rn == 15; }
		else
			{ MCC_TRACE("br\n"); bad |= rd == 15 || rn == 15; }
		if (bad)
			{ MCC_TRACE("br\n"); return 0; }
	}

	if (opc == 13 || opc == 15) { MCC_TRACE("br\n");
		if (rn)
			{ MCC_TRACE("br\n"); return 0; }
		if (opc == 13 && !I) { MCC_TRACE("br\n");
			int rm = w & 15, ty = (w >> 5) & 3;
			if (w & 0x10) { MCC_TRACE("br\n");
				if (w & 0x80)
					{ MCC_TRACE("br\n"); return 0; }
				sb(b, "%s%s%s\t%s, %s, %s", shnm[ty], S ? "s" : "", cc,
					 regnm[rd], regnm[rm], regnm[(w >> 8) & 15]);
				return 1;
			} else { MCC_TRACE("br\n");
				int amt = (w >> 7) & 31;
				if (amt == 0 && ty == 3) { MCC_TRACE("br\n");
					sb(b, "rrx%s%s\t%s, %s", S ? "s" : "", cc,
						 regnm[rd], regnm[rm]);
					return 1;
				}
				if (amt) { MCC_TRACE("br\n");
					sb(b, "%s%s%s\t%s, %s, #%d", shnm[ty], S ? "s" : "", cc,
						 regnm[rd], regnm[rm], amt);
					return 1;
				}
				if (ty)
					{ MCC_TRACE("br\n"); return 0; }
			}
		}
		sb(b, "%s%s%s\t%s, %s", dpnm[opc], S ? "s" : "", cc, regnm[rd], o2buf);
		return 1;
	}
	if (opc >= 8 && opc <= 11) { MCC_TRACE("br\n");
		if (rd)
			{ MCC_TRACE("br\n"); return 0; }
		sb(b, "%s%s\t%s, %s", dpnm[opc], cc, regnm[rn], o2buf);
		return 1;
	}
	sb(b, "%s%s%s\t%s, %s, %s", dpnm[opc], S ? "s" : "", cc,
		 regnm[rd], regnm[rn], o2buf);
	return 1;
}

static int dis_mul(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int A = (w >> 21) & 1, S = (w >> 20) & 1;
	int rd = (w >> 16) & 15, ra = (w >> 12) & 15;
	int rs = (w >> 8) & 15, rm = w & 15;
	if (A)
		{ MCC_TRACE("br\n"); sb(b, "mla%s%s\t%s, %s, %s, %s", S ? "s" : "", cc,
			 regnm[rd], regnm[rm], regnm[rs], regnm[ra]); }
	else { MCC_TRACE("br\n");
		if (ra)
			{ MCC_TRACE("br\n"); return 0; }
		sb(b, "mul%s%s\t%s, %s, %s", S ? "s" : "", cc,
			 regnm[rd], regnm[rm], regnm[rs]);
	}
	return 1;
}

static int dis_mull(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int sg = (w >> 22) & 1, A = (w >> 21) & 1, S = (w >> 20) & 1;
	int rdhi = (w >> 16) & 15, rdlo = (w >> 12) & 15;
	int rs = (w >> 8) & 15, rm = w & 15;
	sb(b, "%s%s%s%s\t%s, %s, %s, %s",
		 sg ? "s" : "u", A ? "mlal" : "mull", S ? "s" : "", cc,
		 regnm[rdlo], regnm[rdhi], regnm[rm], regnm[rs]);
	return 1;
}

static int sb_addr(SB *b, int rn, const char *off, int Pb, int W) { MCC_TRACE("enter\n");
	if (Pb) { MCC_TRACE("br\n");
		if (off[0])
			{ MCC_TRACE("br\n"); sb(b, "[%s, %s]%s", regnm[rn], off, W ? "!" : ""); }
		else if (W)
			{ MCC_TRACE("br\n"); sb(b, "[%s, #0]!", regnm[rn]); }
		else
			{ MCC_TRACE("br\n"); sb(b, "[%s]", regnm[rn]); }
	} else { MCC_TRACE("br\n");
		if (W)
			{ MCC_TRACE("br\n"); return 0; }
		sb(b, "[%s], %s", regnm[rn], off[0] ? off : "#0");
	}
	return 1;
}

static int dis_mem(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int I = (w >> 25) & 1, Pb = (w >> 24) & 1, U = (w >> 23) & 1;
	int B = (w >> 22) & 1, W = (w >> 21) & 1, L = (w >> 20) & 1;
	int rn = (w >> 16) & 15, rd = (w >> 12) & 15;
	char offbuf[64];
	SB off;

	sb_init(&off, offbuf, sizeof offbuf);
	if (I) { MCC_TRACE("br\n");
		if (w & 0x10)
			{ MCC_TRACE("br\n"); return 0; }
		if (!U)
			{ MCC_TRACE("br\n"); sb(&off, "-"); }
		if (!sb_shift_op(&off, w, 0))
			{ MCC_TRACE("br\n"); return 0; }
	} else { MCC_TRACE("br\n");
		uint32_t imm = w & 0xfff;
		if (imm)
			{ MCC_TRACE("br\n"); sb(&off, "#%s%u", U ? "" : "-", imm); }
		else if (!U)
			{ MCC_TRACE("br\n"); sb(&off, "#-0"); }
	}
	sb(b, "%s%s%s\t%s, ", L ? "ldr" : "str", B ? "b" : "", cc, regnm[rd]);
	return sb_addr(b, rn, offbuf, Pb, W);
}

static int dis_hmem(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int Pb = (w >> 24) & 1, U = (w >> 23) & 1, I = (w >> 22) & 1;
	int W = (w >> 21) & 1, L = (w >> 20) & 1, SH = (w >> 5) & 3;
	int rn = (w >> 16) & 15, rd = (w >> 12) & 15;
	const char *nm;
	char offbuf[32];
	SB off;

	if (L)
		{ MCC_TRACE("br\n"); nm = SH == 1
						 ? "ldrh"
				 : SH == 2
						 ? "ldrsb"
						 : "ldrsh"; }
	else if (SH == 1)
		{ MCC_TRACE("br\n"); nm = "strh"; }
	else
		{ MCC_TRACE("br\n"); return 0; }

	sb_init(&off, offbuf, sizeof offbuf);
	if (I) { MCC_TRACE("br\n");
		uint32_t imm = ((w >> 4) & 0xf0) | (w & 15);
		if (imm)
			{ MCC_TRACE("br\n"); sb(&off, "#%s%u", U ? "" : "-", imm); }
		else if (!U)
			{ MCC_TRACE("br\n"); sb(&off, "#-0"); }
	} else { MCC_TRACE("br\n");
		if (w & 0xf00)
			{ MCC_TRACE("br\n"); return 0; }
		sb(&off, "%s%s", U ? "" : "-", regnm[w & 15]);
	}
	sb(b, "%s%s\t%s, ", nm, cc, regnm[rd]);
	return sb_addr(b, rn, offbuf, Pb, W);
}

static int dis_ldm(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int Pb = (w >> 24) & 1, U = (w >> 23) & 1, S = (w >> 22) & 1;
	int W = (w >> 21) & 1, L = (w >> 20) & 1;
	int rn = (w >> 16) & 15;
	uint32_t list = w & 0xffff;
	static const char *const sfx[4] = {"da", "ia", "db", "ib"};

	if (S || !list)
		{ MCC_TRACE("br\n"); return 0; }
	if (rn == 13 && W && L && !Pb && U)
		{ MCC_TRACE("br\n"); sb(b, "pop%s\t", cc); }
	else if (rn == 13 && W && !L && Pb && !U)
		{ MCC_TRACE("br\n"); sb(b, "push%s\t", cc); }
	else
		{ MCC_TRACE("br\n"); sb(b, "%s%s%s\t%s%s, ", L ? "ldm" : "stm", sfx[(Pb << 1) | U], cc,
			 regnm[rn], W ? "!" : ""); }
	sb_reglist(b, list);
	return 1;
}

static void sb_sreg(SB *b, int base4, int lowbit) { MCC_TRACE("enter\n");
	sb(b, "s%d", (base4 << 1) | lowbit);
}

static void sb_vlist(SB *b, int dbl, int first, int count) { MCC_TRACE("enter\n");
	if (count == 1)
		{ MCC_TRACE("br\n"); sb(b, "{%c%d}", dbl ? 'd' : 's', first); }
	else
		{ MCC_TRACE("br\n"); sb(b, "{%c%d-%c%d}", dbl ? 'd' : 's', first,
			 dbl ? 'd' : 's', first + count - 1); }
}

static int dis_cpmem(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int Pb = (w >> 24) & 1, U = (w >> 23) & 1, D = (w >> 22) & 1;
	int W = (w >> 21) & 1, L = (w >> 20) & 1;
	int rn = (w >> 16) & 15, crd = (w >> 12) & 15, cp = (w >> 8) & 15;
	uint32_t imm8 = w & 0xff;
	int off = imm8 * 4;
	int vfp = (cp == 10 || cp == 11), dbl = (cp == 11);

	if (vfp && Pb && !W && !(dbl && D)) { MCC_TRACE("br\n");
		sb(b, "%s%s\t", L ? "vldr" : "vstr", cc);
		if (dbl)
			{ MCC_TRACE("br\n"); sb(b, "d%d", crd); }
		else
			{ MCC_TRACE("br\n"); sb_sreg(b, crd, D); }
		if (imm8)
			{ MCC_TRACE("br\n"); sb(b, ", [%s, #%s%d]", regnm[rn], U ? "" : "-", off); }
		else if (!U)
			{ MCC_TRACE("br\n"); sb(b, ", [%s, #-0]", regnm[rn]); }
		else
			{ MCC_TRACE("br\n"); sb(b, ", [%s]", regnm[rn]); }
		return 1;
	} else if (vfp && imm8 && !(Pb && !W) && !(dbl && ((imm8 & 1) || D))) { MCC_TRACE("br\n");
		int count = dbl ? imm8 >> 1 : imm8;
		int first = dbl ? crd : (crd << 1) | D;
		if (Pb && !U && W) { MCC_TRACE("br\n");
			if (rn == 13 && !L)
				{ MCC_TRACE("br\n"); sb(b, "vpush%s\t", cc); }
			else
				{ MCC_TRACE("br\n"); sb(b, "%s%s\t%s!, ", L ? "vldmdb" : "vstmdb", cc, regnm[rn]); }
			sb_vlist(b, dbl, first, count);
			return 1;
		}
		if (!Pb && U) { MCC_TRACE("br\n");
			if (rn == 13 && W && L)
				{ MCC_TRACE("br\n"); sb(b, "vpop%s\t", cc); }
			else
				{ MCC_TRACE("br\n"); sb(b, "%s%s\t%s%s, ", L ? "vldmia" : "vstmia", cc,
					 regnm[rn], W ? "!" : ""); }
			sb_vlist(b, dbl, first, count);
			return 1;
		}
	}

	{
		char offbuf[24];
		SB ob;
		sb_init(&ob, offbuf, sizeof offbuf);
		if (imm8)
			{ MCC_TRACE("br\n"); sb(&ob, "#%s%d", U ? "" : "-", off); }
		else if (!U)
			{ MCC_TRACE("br\n"); sb(&ob, "#-0"); }
		sb(b, "%s%s%s\tp%d, c%d, ", L ? "ldc" : "stc", D ? "l" : "", cc,
			 cp, crd);
		return sb_addr(b, rn, offbuf, Pb, W);
	}
}

static int dis_vfp_dp(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int dbl = ((w >> 8) & 15) == 11;
	const char *pr = dbl ? "f64" : "f32";
	int D = (w >> 22) & 1, N = (w >> 7) & 1, M = (w >> 5) & 1;
	int Fd = (w >> 12) & 15, Fn = (w >> 16) & 15, Fm = w & 15;
	int op6 = (w >> 6) & 1;
	int sel = (w >> 20) & 0xb;
	char d[8], m[8];

	if (dbl) { MCC_TRACE("br\n");
		if (D)
			{ MCC_TRACE("br\n"); return 0; }
		snprintf(d, sizeof d, "d%d", Fd);
		snprintf(m, sizeof m, "d%d", Fm);
	} else { MCC_TRACE("br\n");
		snprintf(d, sizeof d, "s%d", (Fd << 1) | D);
		snprintf(m, sizeof m, "s%d", (Fm << 1) | M);
	}

	if (sel == 0xb) { MCC_TRACE("br\n");
		if (!op6)
			{ MCC_TRACE("br\n"); return 0; }
		switch (Fn) { MCC_TRACE("br\n");
		case 0:
		case 1:
		case 4: {
			static const char *const enm[3][2] = {
					{"vmov", "vabs"}, {"vneg", "vsqrt"}, {"vcmp", "vcmpe"}};
			if (dbl && M)
				{ MCC_TRACE("br\n"); return 0; }
			sb(b, "%s%s.%s\t%s, %s",
				 enm[Fn == 4 ? 2 : Fn][N], cc, pr, d, m);
			return 1;
		}
		case 5:
			if (Fm || M)
				{ MCC_TRACE("br\n"); return 0; }
			sb(b, "%s%s.%s\t%s, #0", N ? "vcmpe" : "vcmp", cc, pr, d);
			return 1;
		case 7:
			if (!N)
				{ MCC_TRACE("br\n"); return 0; }
			if (dbl) { MCC_TRACE("br\n");
				if (M)
					{ MCC_TRACE("br\n"); return 0; }
				sb(b, "vcvt%s.f32.f64\ts%d, d%d", cc, (Fd << 1) | D, Fm);
			} else { MCC_TRACE("br\n");
				if (D)
					{ MCC_TRACE("br\n"); return 0; }
				sb(b, "vcvt%s.f64.f32\td%d, s%d", cc, Fd, (Fm << 1) | M);
			}
			return 1;
		case 8:
			sb(b, "vcvt%s.%s.%s\t%s, s%d", cc, pr, N ? "s32" : "u32", d,
				 (Fm << 1) | M);
			return 1;
		case 0xc:
		case 0xd:

			if (dbl && M)
				{ MCC_TRACE("br\n"); return 0; }
			sb(b, "%s%s.%s.%s\ts%d, %s", N ? "vcvt" : "vcvtr", cc,
				 (Fn & 1) ? "s32" : "u32", pr, (Fd << 1) | D, m);
			return 1;
		}
		return 0;
	}

	{
		char n[8];
		if (dbl) { MCC_TRACE("br\n");
			if (N || M)
				{ MCC_TRACE("br\n"); return 0; }
			snprintf(n, sizeof n, "d%d", Fn);
		} else { MCC_TRACE("br\n");
			snprintf(n, sizeof n, "s%d", (Fn << 1) | N);
		}
		switch (sel) { MCC_TRACE("br\n");
		case 0x2:
			sb(b, "%s%s.%s\t%s, %s, %s", op6 ? "vnmul" : "vmul",
				 cc, pr, d, n, m);
			return 1;
		case 0x3:
			sb(b, "%s%s.%s\t%s, %s, %s", op6 ? "vsub" : "vadd",
				 cc, pr, d, n, m);
			return 1;
		case 0x8:
			if (op6)
				{ MCC_TRACE("br\n"); return 0; }
			sb(b, "vdiv%s.%s\t%s, %s, %s", cc, pr, d, n, m);
			return 1;
		}
	}
	return 0;
}

static int dis_cdp(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int cp = (w >> 8) & 15;
	if ((cp == 10 || cp == 11) && dis_vfp_dp(w, b))
		{ MCC_TRACE("br\n"); return 1; }

	sb(b, "cdp%s\tp%d, %d, c%d, c%d, c%d, %d", cc, cp,
		 (w >> 20) & 15, (w >> 12) & 15, (w >> 16) & 15, w & 15, (w >> 5) & 7);
	return 1;
}

static int dis_mrc(uint32_t w, SB *b) { MCC_TRACE("enter\n");
	const char *cc = cc_sfx[w >> 28];
	int cp = (w >> 8) & 15, L = (w >> 20) & 1;
	int rt = (w >> 12) & 15;

	if (cp == 10) { MCC_TRACE("br\n");
		if ((w & 0x0FFF0FFF) == 0x0EF10A10) { MCC_TRACE("br\n");
			sb(b, "vmrs%s\t%s, fpscr", cc, rt == 15 ? "apsr_nzcv" : regnm[rt]);
			return 1;
		}
		if ((w & 0x0FFF0FFF) == 0x0EE10A10 && rt != 15) { MCC_TRACE("br\n");
			sb(b, "vmsr%s\tfpscr, %s", cc, regnm[rt]);
			return 1;
		}
		if ((w & 0x0FE00F7F) == 0x0E000A10) { MCC_TRACE("br\n");
			int sn = (((w >> 16) & 15) << 1) | ((w >> 7) & 1);
			if (rt != 15) { MCC_TRACE("br\n");
				if (L)
					{ MCC_TRACE("br\n"); sb(b, "vmov%s.f32\t%s, s%d", cc, regnm[rt], sn); }
				else
					{ MCC_TRACE("br\n"); sb(b, "vmov%s.f32\ts%d, %s", cc, sn, regnm[rt]); }
				return 1;
			}
		}
	}

	sb(b, "%s%s\tp%d, %d, %s, c%d, c%d, %d", L ? "mrc" : "mcr", cc, cp,
		 (w >> 21) & 7, regnm[rt], (w >> 16) & 15, w & 15, (w >> 5) & 7);
	return 1;
}

static int decode(disasm_ctx *dc, uint32_t w, SB *b) { MCC_TRACE("enter\n");
	uint32_t cond = w >> 28;

	if (cond == 15)
		{ MCC_TRACE("br\n"); return 0; }

	switch ((w >> 25) & 7) { MCC_TRACE("br\n");
	case 0:
		if ((w & 0x0FFFFFF0) == 0x012FFF10) { MCC_TRACE("br\n");
			sb(b, "bx%s\t%s", cc_sfx[cond], regnm[w & 15]);
			return 1;
		}
		if ((w & 0x0FFFFFF0) == 0x012FFF30) { MCC_TRACE("br\n");
			sb(b, "blx%s\t%s", cc_sfx[cond], regnm[w & 15]);
			return 1;
		}
		if ((w & 0x0FFF0FF0) == 0x016F0F10) { MCC_TRACE("br\n");
			sb(b, "clz%s\t%s, %s", cc_sfx[cond],
				 regnm[(w >> 12) & 15], regnm[w & 15]);
			return 1;
		}
		if ((w & 0x0FC000F0) == 0x00000090)
			{ MCC_TRACE("br\n"); return dis_mul(w, b); }
		if ((w & 0x0F8000F0) == 0x00800090)
			{ MCC_TRACE("br\n"); return dis_mull(w, b); }
		if ((w & 0x00000090) == 0x00000090 && (w & 0x60))
			{ MCC_TRACE("br\n"); return dis_hmem(w, b); }
		return dis_dp(w, b);
	case 1:
		return dis_dp(w, b);
	case 2:
	case 3:
		return dis_mem(w, b);
	case 4:
		return dis_ldm(w, b);
	case 5:
		return dis_branch(dc, w, b);
	case 6:
		return dis_cpmem(w, b);
	case 7:
		if (w & 0x01000000) { MCC_TRACE("br\n");
			uint32_t imm = w & 0xffffff;
			if (imm > 255)
				{ MCC_TRACE("br\n"); return 0; }
			sb(b, "svc%s\t#%u", cc_sfx[cond], imm);
			return 1;
		}
		if (w & 0x10)
			{ MCC_TRACE("br\n"); return dis_mrc(w, b); }
		return dis_cdp(w, b);
	}
	return 0;
}

ST_FUNC int mcc_disasm_insn(disasm_ctx *dc) { MCC_TRACE("enter\n");
	char line[352];
	SB b;
	uint32_t w;
	int rtype = 0;
	const char *rel;

	if (dc->pc + 4 > dc->size) { MCC_TRACE("br\n");
		if (!dc->collect)
			{ MCC_TRACE("br\n"); fprintf(dc->out, ".byte\t0x%02x", dc->data[dc->pc]); }
		return 1;
	}
	w = read32le(dc->data + dc->pc);

	rel = disasm_reloc(dc, dc->pc, 4, &rtype);
	if (rel && mcc_disasm_reloc_size(rtype) == 4) { MCC_TRACE("br\n");
		if (!dc->collect)
			{ MCC_TRACE("br\n"); fprintf(dc->out, ".long\t%s", rel); }
		return 4;
	}

	sb_init(&b, line, sizeof line);
	if (decode(dc, w, &b)) { MCC_TRACE("br\n");
		if (!dc->collect)
			{ MCC_TRACE("br\n"); fprintf(dc->out, "%s", line); }
	} else { MCC_TRACE("br\n");
		if (!dc->collect)
			{ MCC_TRACE("br\n"); fprintf(dc->out, ".long\t0x%08x", w); }
	}
	return 4;
}

ST_FUNC int mcc_disasm_reloc_size(int type) { MCC_TRACE("enter\n");
	switch (type) { MCC_TRACE("br\n");
	case R_ARM_ABS32:
	case R_ARM_REL32:
	case R_ARM_GOT_PREL:
	case R_ARM_GOT32:
	case R_ARM_GOTOFF:
	case R_ARM_TLS_IE32:
	case R_ARM_TLS_LE32:
		return 4;
	}
	return 0;
}

ST_FUNC int mcc_disasm_reloc_addend_bias(int type, int size) { MCC_TRACE("enter\n");
	(void)type;
	(void)size;
	return 0;
}

#endif
