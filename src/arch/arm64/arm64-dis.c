#include "mcc.h"

#ifdef MCC_TARGET_ARM64

#define D_UNK 0
#define D_CMT 1
#define D_ASM 2

static char *rpool(disasm_ctx *dc) { MCC_TRACE("enter\n");
	dc->dis_namepool_i = (dc->dis_namepool_i + 1) & 7;
	return dc->dis_namepool[dc->dis_namepool_i];
}

static const char *ir(disasm_ctx *dc, int sf, int r) { MCC_TRACE("enter\n");
	char *b;
	if ((r &= 31) == 31)
		{ MCC_TRACE("br\n"); return sf ? "xzr" : "wzr"; }
	b = rpool(dc);
	snprintf(b, 24, "%c%d", sf ? 'x' : 'w', r);
	return b;
}

static const char *irsp(disasm_ctx *dc, int sf, int r) { MCC_TRACE("enter\n");
	if ((r &= 31) == 31)
		{ MCC_TRACE("br\n"); return sf ? "sp" : "wsp"; }
	return ir(dc, sf, r);
}

static const char *fr(disasm_ctx *dc, int kind, int r) { MCC_TRACE("enter\n");
	char *b = rpool(dc);
	snprintf(b, 24, "%c%d", kind, r & 31);
	return b;
}

static const char *cc[16] = {
		"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
		"hi", "ls", "ge", "lt", "gt", "le", "al", "nv"};

static int dis_encode_bimm64(uint64_t x) { MCC_TRACE("enter\n");
	int neg = x & 1;
	int rep, pos, len;

	if (neg)
		{ MCC_TRACE("br\n"); x = ~x; }
	if (!x)
		{ MCC_TRACE("br\n"); return -1; }

	if (x >> 2 == (x & (((uint64_t)1 << 62) - 1)))
		{ MCC_TRACE("br\n"); rep = 2, x &= 3; }
	else if (x >> 4 == (x & (((uint64_t)1 << 60) - 1)))
		{ MCC_TRACE("br\n"); rep = 4, x &= 15; }
	else if (x >> 8 == (x & (((uint64_t)1 << 56) - 1)))
		{ MCC_TRACE("br\n"); rep = 8, x &= 255; }
	else if (x >> 16 == (x & (((uint64_t)1 << 48) - 1)))
		{ MCC_TRACE("br\n"); rep = 16, x &= 0xffff; }
	else if (x >> 32 == (x & (((uint64_t)1 << 32) - 1)))
		{ MCC_TRACE("br\n"); rep = 32, x &= 0xffffffff; }
	else
		{ MCC_TRACE("br\n"); rep = 64; }

	pos = 0;
	if (!(x & 0xffffffff))
		{ MCC_TRACE("br\n"); x >>= 32, pos += 32; }
	if (!(x & 0xffff))
		{ MCC_TRACE("br\n"); x >>= 16, pos += 16; }
	if (!(x & 0xff))
		{ MCC_TRACE("br\n"); x >>= 8, pos += 8; }
	if (!(x & 0xf))
		{ MCC_TRACE("br\n"); x >>= 4, pos += 4; }
	if (!(x & 0x3))
		{ MCC_TRACE("br\n"); x >>= 2, pos += 2; }
	if (!(x & 0x1))
		{ MCC_TRACE("br\n"); x >>= 1, pos += 1; }

	len = 0;
	if (!(~x & 0xffffffff))
		{ MCC_TRACE("br\n"); x >>= 32, len += 32; }
	if (!(~x & 0xffff))
		{ MCC_TRACE("br\n"); x >>= 16, len += 16; }
	if (!(~x & 0xff))
		{ MCC_TRACE("br\n"); x >>= 8, len += 8; }
	if (!(~x & 0xf))
		{ MCC_TRACE("br\n"); x >>= 4, len += 4; }
	if (!(~x & 0x3))
		{ MCC_TRACE("br\n"); x >>= 2, len += 2; }
	if (!(~x & 0x1))
		{ MCC_TRACE("br\n"); x >>= 1, len += 1; }

	if (x)
		{ MCC_TRACE("br\n"); return -1; }
	if (neg) { MCC_TRACE("br\n");
		pos = (pos + len) & (rep - 1);
		len = rep - len;
	}
	return ((0x1000 & rep << 6) | (((rep - 1) ^ 31) << 1 & 63) |
					((rep - pos) & (rep - 1)) << 6 | (len - 1));
}

static uint64_t dis_decode_bimm(int n, int immr, int imms) { MCC_TRACE("enter\n");
	int len, esize, i;
	uint64_t welem, pattern;

	{
		int v = n << 6 | (~imms & 0x3f);
		for (len = 6; len >= 0 && !(v >> len & 1); len--)
			;
	}
	if (len < 1)
		{ MCC_TRACE("br\n"); return 0; }
	esize = 1 << len;
	imms &= esize - 1;
	immr &= esize - 1;
	welem = (imms + 1 == 64) ? ~(uint64_t)0 : (((uint64_t)1 << (imms + 1)) - 1);

	if (immr)
		{ MCC_TRACE("br\n"); welem = (welem >> immr | welem << (esize - immr)) & (esize == 64 ? ~(uint64_t)0 : (((uint64_t)1 << esize) - 1)); }
	pattern = 0;
	for (i = 0; i < 64; i += esize)
		{ MCC_TRACE("br\n"); pattern |= welem << i; }
	return pattern;
}

static int simm(uint32_t w, int lo, int bits) { MCC_TRACE("enter\n");
	return (int)(w << (32 - lo - bits)) >> (32 - bits);
}

static const char *btext(disasm_ctx *dc, addr_t target) { MCC_TRACE("enter\n");
	return disasm_label(dc, target);
}

static int decode(disasm_ctx *dc, uint32_t w, char *out, size_t osz) { MCC_TRACE("enter\n");
	addr_t pc = dc->pc;
	int rd = w & 31, rn = (w >> 5) & 31, rm = (w >> 16) & 31;
	int sf = w >> 31;
	int rtype = 0;
	const char *sym = disasm_reloc(dc, pc, 4, &rtype);

	out[0] = 0;

	if (w == 0xd503201f) { MCC_TRACE("br\n");
		snprintf(out, osz, "nop");
		return D_ASM;
	}
	if ((w & 0xfffffc1f) == 0xd65f0000) { MCC_TRACE("br\n");
		if (rn == 30)
			{ MCC_TRACE("br\n"); snprintf(out, osz, "ret"); }
		else
			{ MCC_TRACE("br\n"); snprintf(out, osz, "ret\t%s", ir(dc, 1, rn)); }
		return D_ASM;
	}
	if ((w & 0xfffffc1f) == 0xd61f0000) { MCC_TRACE("br\n");
		snprintf(out, osz, "br\t%s", ir(dc, 1, rn));
		return D_ASM;
	}
	if ((w & 0xfffffc1f) == 0xd63f0000) { MCC_TRACE("br\n");
		snprintf(out, osz, "blr\t%s", ir(dc, 1, rn));
		return D_ASM;
	}
	if ((w & 0xffffffe0) == 0xd53b4400 || (w & 0xffffffe0) == 0xd53b4420) { MCC_TRACE("br\n");
		snprintf(out, osz, "mrs\t%s, %s", ir(dc, 1, rd),
						 (w & 0x20) ? "fpsr" : "fpcr");
		return D_ASM;
	}
	if ((w & 0xffffffe0) == 0xd51b4400 || (w & 0xffffffe0) == 0xd51b4420) { MCC_TRACE("br\n");
		snprintf(out, osz, "msr\t%s, %s", (w & 0x20) ? "fpsr" : "fpcr",
						 ir(dc, 1, rd));
		return D_ASM;
	}
	if ((w & 0xfffff01f) == 0xd503301f && ((w >> 5) & 7) >= 4) { MCC_TRACE("br\n");
		static const char *opt[16] = {
				0, "oshld", "oshst", "osh", 0, "nshld", "nshst", "nsh",
				0, "ishld", "ishst", "ish", 0, "ld", "st", "sy"};
		int op2 = (w >> 5) & 7, o = (w >> 8) & 15;
		const char *nm = op2 == 4
												 ? "dsb"
										 : op2 == 5
												 ? "dmb"
										 : op2 == 6
												 ? "isb"
												 : 0;
		if (nm) { MCC_TRACE("br\n");
			if (opt[o])
				{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s", nm, opt[o]); }
			else
				{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t#%d", nm, o); }
			return D_ASM;
		}
	}
	if ((w & 0xfff00000) == 0xd5300000) { MCC_TRACE("br\n");
		snprintf(out, osz, "mrs\t%s, s3_%d_c%d_c%d_%d", ir(dc, 1, rd),
						 (int)(w >> 16) & 7, (int)(w >> 12) & 15,
						 (int)(w >> 8) & 15, (int)(w >> 5) & 7);
		return D_CMT;
	}
	if ((w & 0xfff00000) == 0xd5100000) { MCC_TRACE("br\n");
		snprintf(out, osz, "msr\ts3_%d_c%d_c%d_%d, %s",
						 (int)(w >> 16) & 7, (int)(w >> 12) & 15,
						 (int)(w >> 8) & 15, (int)(w >> 5) & 7, ir(dc, 1, rd));
		return D_CMT;
	}
	if ((w & 0xfff80000) == 0xd5080000) { MCC_TRACE("br\n");
		snprintf(out, osz, "sys\t#%d, c%d, c%d, #%d, %s",
						 (int)(w >> 16) & 7, (int)(w >> 12) & 15,
						 (int)(w >> 8) & 15, (int)(w >> 5) & 7, ir(dc, 1, rd));
		return D_CMT;
	}

	if ((w & 0x7c000000) == 0x14000000) { MCC_TRACE("br\n");
		const char *nm = (w >> 31) ? "bl" : "b";
		if (sym && (rtype == R_AARCH64_CALL26 || rtype == R_AARCH64_JUMP26)) { MCC_TRACE("br\n");
			if (!(w & 0x03ffffff)) { MCC_TRACE("br\n");
				snprintf(out, osz, "%s\t%s", nm, sym);
				return D_ASM;
			}
			snprintf(out, osz, "%s\t%s", nm, sym);
			return D_CMT;
		}
		{
			addr_t target = pc + (addr_t)((int64_t)simm(w, 0, 26) << 2);
			const char *l = btext(dc, target);

			snprintf(out, osz, "%s\t0x%llx\t// %s", nm,
							 (unsigned long long)target, l);
			return D_ASM;
		}
	}
	if ((w & 0xff000010) == 0x54000000) { MCC_TRACE("br\n");
		int cond = w & 15;
		if (sym && rtype == R_AARCH64_CONDBR19 && !(w & 0x00ffffe0) && cond < 14) { MCC_TRACE("br\n");
			snprintf(out, osz, "b%s\t%s", cc[cond], sym);
			return D_ASM;
		}
		{
			addr_t target = pc + (addr_t)((int64_t)simm(w, 5, 19) << 2);
			const char *l = btext(dc, target);

			snprintf(out, osz, "b%s\t%s", cc[cond], l);
			return D_CMT;
		}
	}
	if ((w & 0x7e000000) == 0x34000000) { MCC_TRACE("br\n");
		const char *nm = (w & 0x01000000) ? "cbnz" : "cbz";
		if (sym && rtype == R_AARCH64_CONDBR19 && !(w & 0x00ffffe0)) { MCC_TRACE("br\n");
			snprintf(out, osz, "%s\t%s, %s", nm, ir(dc, sf, rd), sym);
			return D_ASM;
		}
		{
			addr_t target = pc + (addr_t)((int64_t)simm(w, 5, 19) << 2);
			const char *l = btext(dc, target);
			snprintf(out, osz, "%s\t%s, %s", nm, ir(dc, sf, rd), l);
			return D_CMT;
		}
	}
	if ((w & 0x7e000000) == 0x36000000) { MCC_TRACE("br\n");
		int bit = ((w >> 31) << 5) | ((w >> 19) & 31);
		addr_t target = pc + (addr_t)((int64_t)simm(w, 5, 14) << 2);
		snprintf(out, osz, "%s\t%s, #%d, %s",
						 (w & 0x01000000) ? "tbnz" : "tbz",
						 ir(dc, w >> 31, rd), bit, btext(dc, target));
		return D_CMT;
	}

	if ((w & 0x1f000000) == 0x10000000) { MCC_TRACE("br\n");
		const char *nm = (w >> 31) ? "adrp" : "adr";
		if (sym) { MCC_TRACE("br\n");
			const char *pfx = rtype == R_AARCH64_ADR_GOT_PAGE ? ":got:" : "";
			snprintf(out, osz, "%s\t%s, %s%s", nm, ir(dc, 1, rd), pfx, sym);

			if ((w >> 31) && !(w & 0x60ffffe0) && (rtype == R_AARCH64_ADR_GOT_PAGE || rtype == R_AARCH64_ADR_PREL_PG_HI21))
				{ MCC_TRACE("br\n"); return D_ASM; }
			return D_CMT;
		}
		snprintf(out, osz, "%s\t%s, 0x%llx", nm, ir(dc, 1, rd),
						 (unsigned long long)(simm(w, 5, 19) << 2 |
																	(int)((w >> 29) & 3)));
		return D_CMT;
	}

	if ((w & 0x1f800000) == 0x12800000) { MCC_TRACE("br\n");
		int opc = (w >> 29) & 3, hw = (w >> 21) & 3;
		unsigned imm = (w >> 5) & 0xffff;
		const char *nm = opc == 0
												 ? "movn"
										 : opc == 2
												 ? "movz"
										 : opc == 3
												 ? "movk"
												 : 0;
		if (nm && (sf || hw <= 1)) { MCC_TRACE("br\n");
			if (hw)
				{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, #0x%x, lsl #%d",
								 nm, ir(dc, sf, rd), imm, hw * 16); }
			else
				{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, #0x%x", nm, ir(dc, sf, rd), imm); }
			return sym ? D_CMT : D_ASM;
		}
		return D_UNK;
	}

	if ((w & 0x1f800000) == 0x11000000) { MCC_TRACE("br\n");
		int op = (w >> 30) & 1, S = (w >> 29) & 1, sh = (w >> 22) & 1;
		unsigned long long val = (unsigned long long)((w >> 10) & 0xfff)
														 << (sh ? 12 : 0);
		const char *nm = op ? (S ? "subs" : "sub") : (S ? "adds" : "add");
		const char *d = S ? ir(dc, sf, rd) : irsp(dc, sf, rd);
		if (sym) { MCC_TRACE("br\n");
			const char *pfx =
					rtype == R_AARCH64_ADD_ABS_LO12_NC
							? ":lo12:"
					: rtype == R_AARCH64_TLSLE_ADD_TPREL_HI12
							? ":tprel_hi12:"
					: rtype == R_AARCH64_TLSLE_ADD_TPREL_LO12
							? ":tprel_lo12:"
							: "";
			snprintf(out, osz, "%s\t%s, %s, #%s%s", nm, d, irsp(dc, sf, rn),
							 pfx, sym);

			if (*pfx && !op && !S && !((w >> 10) & 0xfff) && sh == (rtype == R_AARCH64_TLSLE_ADD_TPREL_HI12) && (sf || (rd != 31 && rn != 31)))
				{ MCC_TRACE("br\n"); return D_ASM; }
			return D_CMT;
		}
		if (!sf && (rd == 31 || rn == 31) && !S)
			{ MCC_TRACE("br\n"); return D_UNK; }
		snprintf(out, osz, "%s\t%s, %s, #%llu", nm, d, irsp(dc, sf, rn), val);
		return D_ASM;
	}

	if ((w & 0x1f200000) == 0x0b000000) { MCC_TRACE("br\n");
		int op = (w >> 30) & 1, S = (w >> 29) & 1;
		int shift = (w >> 22) & 3, imm6 = (w >> 10) & 0x3f;
		const char *nm = op ? (S ? "subs" : "sub") : (S ? "adds" : "add");
		if (!shift && !imm6) { MCC_TRACE("br\n");
			snprintf(out, osz, "%s\t%s, %s, %s", nm,
							 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm));
			return D_ASM;
		}
		snprintf(out, osz, "%s\t%s, %s, %s, %s #%d", nm,
						 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm),
						 shift == 0
								 ? "lsl"
						 : shift == 1
								 ? "lsr"
								 : "asr",
						 imm6);
		return D_CMT;
	}
	if ((w & 0x1f200000) == 0x0b200000) { MCC_TRACE("br\n");
		int op = (w >> 30) & 1, S = (w >> 29) & 1;
		static const char *ext[8] = {
				"uxtb", "uxth", "uxtw", "uxtx", "sxtb", "sxth", "sxtw", "sxtx"};
		int option = (w >> 13) & 7, imm3 = (w >> 10) & 7;
		const char *nm = op ? (S ? "subs" : "sub") : (S ? "adds" : "add");
		if (imm3)
			{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, %s, %s, %s #%d", nm,
							 S ? ir(dc, sf, rd) : irsp(dc, sf, rd), irsp(dc, sf, rn),
							 ir(dc, sf && (option & 3) == 3, rm), ext[option], imm3); }
		else
			{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, %s, %s, %s", nm,
							 S ? ir(dc, sf, rd) : irsp(dc, sf, rd), irsp(dc, sf, rn),
							 ir(dc, sf && (option & 3) == 3, rm), ext[option]); }
		return D_CMT;
	}

	if ((w & 0x1f000000) == 0x0a000000) { MCC_TRACE("br\n");
		static const char *nm4[4] = {"and", "orr", "eor", "ands"};
		static const char *nm4n[4] = {"bic", "orn", "eon", "bics"};
		int opc = (w >> 29) & 3, N = (w >> 21) & 1;
		int shift = (w >> 22) & 3, imm6 = (w >> 10) & 0x3f;
		static const char *shnm[4] = {"lsl", "lsr", "asr", "ror"};
		if (N) { MCC_TRACE("br\n");
			if (opc == 1 && rn == 31 && !shift && !imm6)
				{ MCC_TRACE("br\n"); snprintf(out, osz, "mvn\t%s, %s", ir(dc, sf, rd), ir(dc, sf, rm)); }
			else if (!shift && !imm6)
				{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, %s, %s", nm4n[opc],
								 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm)); }
			else
				{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, %s, %s, %s #%d", nm4n[opc],
								 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm),
								 shnm[shift], imm6); }
			return D_CMT;
		}
		if (!shift && !imm6) { MCC_TRACE("br\n");
			if (opc == 1 && rn == 31) { MCC_TRACE("br\n");
				snprintf(out, osz, "mov\t%s, %s", ir(dc, sf, rd), ir(dc, sf, rm));
				return D_ASM;
			}
			snprintf(out, osz, "%s\t%s, %s, %s", nm4[opc],
							 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm));
			return D_ASM;
		}
		snprintf(out, osz, "%s\t%s, %s, %s, %s #%d", nm4[opc],
						 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm), shnm[shift], imm6);
		return D_CMT;
	}

	if ((w & 0x1f800000) == 0x12000000) { MCC_TRACE("br\n");
		static const char *nm4[4] = {"and", "orr", "eor", "ands"};
		int opc = (w >> 29) & 3;
		int N = (w >> 22) & 1, immr = (w >> 16) & 0x3f, imms = (w >> 10) & 0x3f;
		uint64_t val;
		if (!sf && N)
			{ MCC_TRACE("br\n"); return D_UNK; }
		val = dis_decode_bimm(N, immr, imms);
		if (!sf)
			{ MCC_TRACE("br\n"); val &= 0xffffffff; }

		{
			int e = dis_encode_bimm64(sf ? val : val | val << 32);
			if (e != (N << 12 | immr << 6 | imms))
				{ MCC_TRACE("br\n"); return D_UNK; }
		}
		snprintf(out, osz, "%s\t%s, %s, #0x%llx", nm4[opc],
						 opc == 3 ? ir(dc, sf, rd) : irsp(dc, sf, rd), ir(dc, sf, rn),
						 (unsigned long long)val);
		return D_ASM;
	}

	if ((w & 0x1f800000) == 0x13000000) { MCC_TRACE("br\n");
		int opc = (w >> 29) & 3;
		int N = (w >> 22) & 1, immr = (w >> 16) & 0x3f, imms = (w >> 10) & 0x3f;
		int nbits = sf ? 64 : 32;
		if (N != sf)
			{ MCC_TRACE("br\n"); return D_UNK; }
		if (opc == 2) { MCC_TRACE("br\n");
			if (imms == nbits - 1) { MCC_TRACE("br\n");
				snprintf(out, osz, "lsr\t%s, %s, #%d",
								 ir(dc, sf, rd), ir(dc, sf, rn), immr);
				return D_ASM;
			}
			if (imms + 1 == immr) { MCC_TRACE("br\n");
				snprintf(out, osz, "lsl\t%s, %s, #%d",
								 ir(dc, sf, rd), ir(dc, sf, rn), nbits - 1 - imms);
				return D_ASM;
			}
			if (!immr && imms == 7) { MCC_TRACE("br\n");
				snprintf(out, osz, "uxtb\t%s, %s", ir(dc, sf, rd), ir(dc, sf, rn));
				return D_CMT;
			}
			if (!immr && imms == 15) { MCC_TRACE("br\n");
				snprintf(out, osz, "uxth\t%s, %s", ir(dc, sf, rd), ir(dc, sf, rn));
				return D_CMT;
			}
			snprintf(out, osz, "ubfm\t%s, %s, #%d, #%d",
							 ir(dc, sf, rd), ir(dc, sf, rn), immr, imms);
			return D_CMT;
		}
		if (opc == 0) { MCC_TRACE("br\n");
			if (imms == nbits - 1) { MCC_TRACE("br\n");
				snprintf(out, osz, "asr\t%s, %s, #%d",
								 ir(dc, sf, rd), ir(dc, sf, rn), immr);

				return sf ? D_ASM : D_CMT;
			}
			if (!immr && imms == 7) { MCC_TRACE("br\n");
				snprintf(out, osz, "sxtb\t%s, %s", ir(dc, sf, rd), ir(dc, 0, rn));
				return D_CMT;
			}
			if (!immr && imms == 15) { MCC_TRACE("br\n");
				snprintf(out, osz, "sxth\t%s, %s", ir(dc, sf, rd), ir(dc, 0, rn));
				return D_CMT;
			}
			if (sf && !immr && imms == 31) { MCC_TRACE("br\n");
				snprintf(out, osz, "sxtw\t%s, %s", ir(dc, 1, rd), ir(dc, 0, rn));
				return D_CMT;
			}
			snprintf(out, osz, "sbfm\t%s, %s, #%d, #%d",
							 ir(dc, sf, rd), ir(dc, sf, rn), immr, imms);
			return D_CMT;
		}
		return D_UNK;
	}

	if ((w & 0x1fa00000) == 0x13800000 && ((w >> 22) & 1) == sf) { MCC_TRACE("br\n");
		int imms = (w >> 10) & 0x3f;
		if (rn == rm && (sf || imms < 32)) { MCC_TRACE("br\n");
			snprintf(out, osz, "ror\t%s, %s, #%d", ir(dc, sf, rd), ir(dc, sf, rn),
							 imms);
			return D_ASM;
		}
		snprintf(out, osz, "extr\t%s, %s, %s, #%d",
						 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm), imms);
		return D_CMT;
	}

	if ((w & 0x7fe0f000) == 0x1ac00000 || (w & 0x7fe0f000) == 0x1ac02000) { MCC_TRACE("br\n");
		int op = (w >> 10) & 0x3f;
		static const char *shnm[4] = {"lsl", "lsr", "asr", "ror"};
		if (op >= 8 && op <= 11) { MCC_TRACE("br\n");
			snprintf(out, osz, "%s\t%s, %s, %s", shnm[op - 8],
							 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm));
			return D_ASM;
		}
		if (op == 2 || op == 3) { MCC_TRACE("br\n");
			snprintf(out, osz, "%s\t%s, %s, %s", op == 2 ? "udiv" : "sdiv",
							 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm));
			return D_CMT;
		}
		return D_UNK;
	}

	if ((w & 0x7fe08000) == 0x1b000000 || (w & 0x7fe08000) == 0x1b008000) { MCC_TRACE("br\n");
		int o0 = (w >> 15) & 1, ra = (w >> 10) & 31;
		if (!o0 && ra == 31)

			{ MCC_TRACE("br\n"); snprintf(out, osz, "mul\t%s, %s, %s",
							 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm)); }
		else
			{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, %s, %s, %s", o0 ? "msub" : "madd",
							 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm), ir(dc, sf, ra)); }
		return D_CMT;
	}

	if ((w & 0x1fe00800) == 0x1a800000 || (w & 0x1fe00800) == 0x1a800800) { MCC_TRACE("br\n");
		int op = (w >> 30) & 1, o2 = (w >> 10) & 1, cond = (w >> 12) & 15;
		if (!op && o2 && rn == 31 && rm == 31 && cond < 14)
			{ MCC_TRACE("br\n"); snprintf(out, osz, "cset\t%s, %s", ir(dc, sf, rd), cc[cond ^ 1]); }
		else { MCC_TRACE("br\n");
			static const char *nm[4] = {"csel", "csinc", "csinv", "csneg"};
			snprintf(out, osz, "%s\t%s, %s, %s, %s", nm[op << 1 | o2],
							 ir(dc, sf, rd), ir(dc, sf, rn), ir(dc, sf, rm), cc[cond]);
		}
		return D_CMT;
	}

	if ((w & 0x3a000000) == 0x28000000) { MCC_TRACE("br\n");
		int opcv = w >> 30, V = (w >> 26) & 1, enc = (w >> 23) & 3;
		int L = (w >> 22) & 1, rt2 = (w >> 10) & 31;
		int scale, kind = 0, psf = 1, off;
		char addr[64];
		const char *nm = L ? "ldp" : "stp";
		if (enc == 0)
			{ MCC_TRACE("br\n"); return D_UNK; }
		if (V) { MCC_TRACE("br\n");
			kind = opcv == 0
								 ? 's'
						 : opcv == 1
								 ? 'd'
						 : opcv == 2
								 ? 'q'
								 : 0;
			scale = opcv == 0
									? 2
							: opcv == 1
									? 3
									: 4;
			if (!kind)
				{ MCC_TRACE("br\n"); return D_UNK; }
		} else { MCC_TRACE("br\n");
			if (opcv == 0)
				{ MCC_TRACE("br\n"); psf = 0, scale = 2; }
			else if (opcv == 2)
				{ MCC_TRACE("br\n"); scale = 3; }
			else
				{ MCC_TRACE("br\n"); return D_UNK; }
		}
		off = simm(w, 15, 7) << scale;
		if (enc == 2) { MCC_TRACE("br\n");
			if (off)
				{ MCC_TRACE("br\n"); snprintf(addr, sizeof addr, "[%s, #%d]", irsp(dc, 1, rn), off); }
			else
				{ MCC_TRACE("br\n"); snprintf(addr, sizeof addr, "[%s]", irsp(dc, 1, rn)); }
		} else if (enc == 3) { MCC_TRACE("br\n");
			snprintf(addr, sizeof addr, "[%s, #%d]!", irsp(dc, 1, rn), off);
		} else { MCC_TRACE("br\n");
			snprintf(addr, sizeof addr, "[%s], #%d", irsp(dc, 1, rn), off);
		}
		if (V)
			{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, %s, %s", nm,
							 fr(dc, kind, rd), fr(dc, kind, rt2), addr); }
		else
			{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, %s, %s", nm,
							 ir(dc, psf, rd), ir(dc, psf, rt2), addr); }

		return (V ? kind == 'd' : psf) ? D_ASM : D_CMT;
	}

	if ((w & 0x3b000000) == 0x39000000) { MCC_TRACE("br\n");
		int size = w >> 30, V = (w >> 26) & 1, opc = (w >> 22) & 3;
		unsigned long long off;
		int scale;
		char addr[300];
		if (V) { MCC_TRACE("br\n");
			int kind, load;
			if (opc & 2) { MCC_TRACE("br\n");
				if (size)
					{ MCC_TRACE("br\n"); return D_UNK; }
				kind = 'q', scale = 4, load = opc & 1;
			} else { MCC_TRACE("br\n");
				kind = size == 0
									 ? 'b'
							 : size == 1
									 ? 'h'
							 : size == 2
									 ? 's'
									 : 'd';
				scale = size;
				load = opc & 1;
			}
			off = (unsigned long long)((w >> 10) & 0xfff) << scale;
			if (sym) { MCC_TRACE("br\n");
				snprintf(out, osz, "%s\t%s, [%s, #:lo12:%s]",
								 load ? "ldr" : "str", fr(dc, kind, rd), irsp(dc, 1, rn), sym);
				return D_CMT;
			}
			if (off)
				{ MCC_TRACE("br\n"); snprintf(addr, sizeof addr, "[%s, #%llu]", irsp(dc, 1, rn), off); }
			else
				{ MCC_TRACE("br\n"); snprintf(addr, sizeof addr, "[%s]", irsp(dc, 1, rn)); }
			snprintf(out, osz, "%s\t%s, %s", load ? "ldr" : "str",
							 fr(dc, kind, rd), addr);
			return kind == 'd' ? D_ASM : D_CMT;
		}
		scale = size;
		off = (unsigned long long)((w >> 10) & 0xfff) << scale;
		{
			static const char *ldnm[4] = {"ldrb", "ldrh", "ldr", "ldr"};
			static const char *stnm[4] = {"strb", "strh", "str", "str"};
			int tsf = (size == 3);
			const char *nm;
			if (opc == 0)
				{ MCC_TRACE("br\n"); nm = stnm[size]; }
			else if (opc == 1)
				{ MCC_TRACE("br\n"); nm = ldnm[size]; }
			else { MCC_TRACE("br\n");
				const char *snm = size == 0
															? "ldrsb"
													: size == 1
															? "ldrsh"
													: size == 2
															? "ldrsw"
															: 0;
				if (!snm)
					{ MCC_TRACE("br\n"); return D_UNK; }
				tsf = (opc == 2);
				if (sym)
					{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, [%s, #:lo12:%s]", snm,
									 ir(dc, tsf, rd), irsp(dc, 1, rn), sym); }
				else if (off)
					{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, [%s, #%llu]", snm,
									 ir(dc, tsf, rd), irsp(dc, 1, rn), off); }
				else
					{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, [%s]", snm,
									 ir(dc, tsf, rd), irsp(dc, 1, rn)); }
				return D_CMT;
			}
			if (sym) { MCC_TRACE("br\n");
				static const int lo12_type[4] = {
						R_AARCH64_LDST8_ABS_LO12_NC, R_AARCH64_LDST16_ABS_LO12_NC,
						R_AARCH64_LDST32_ABS_LO12_NC, R_AARCH64_LDST64_ABS_LO12_NC};
				const char *pfx =
						rtype == R_AARCH64_LD64_GOT_LO12_NC
								? ":got_lo12:"
								: ":lo12:";
				snprintf(out, osz, "%s\t%s, [%s, #%s%s]", nm,
								 ir(dc, tsf, rd), irsp(dc, 1, rn), pfx, sym);

				if (!((w >> 10) & 0xfff) && (rtype == lo12_type[size] || (rtype == R_AARCH64_LD64_GOT_LO12_NC && size == 3 && opc == 1)))
					{ MCC_TRACE("br\n"); return D_ASM; }
				return D_CMT;
			}
			if (off)
				{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, [%s, #%llu]", nm,
								 ir(dc, tsf, rd), irsp(dc, 1, rn), off); }
			else
				{ MCC_TRACE("br\n"); snprintf(out, osz, "%s\t%s, [%s]", nm,
								 ir(dc, tsf, rd), irsp(dc, 1, rn)); }
			return D_ASM;
		}
	}

	if ((w & 0x3b200000) == 0x38000000 && ((w >> 10) & 3) != 2) { MCC_TRACE("br\n");
		int size = w >> 30, V = (w >> 26) & 1, opc = (w >> 22) & 3;
		int idx = (w >> 10) & 3;
		int off = simm(w, 12, 9);
		char addr[64];
		if (idx == 0)
			{ MCC_TRACE("br\n"); snprintf(addr, sizeof addr, "[%s, #%d]", irsp(dc, 1, rn), off); }
		else if (idx == 3)
			{ MCC_TRACE("br\n"); snprintf(addr, sizeof addr, "[%s, #%d]!", irsp(dc, 1, rn), off); }
		else
			{ MCC_TRACE("br\n"); snprintf(addr, sizeof addr, "[%s], #%d", irsp(dc, 1, rn), off); }
		if (V) { MCC_TRACE("br\n");
			int kind, load;
			if (opc & 2) { MCC_TRACE("br\n");
				if (size)
					{ MCC_TRACE("br\n"); return D_UNK; }
				kind = 'q', load = opc & 1;
			} else { MCC_TRACE("br\n");
				kind = size == 0
									 ? 'b'
							 : size == 1
									 ? 'h'
							 : size == 2
									 ? 's'
									 : 'd';
				load = opc & 1;
			}
			if (idx == 0) { MCC_TRACE("br\n");
				if (kind == 'd' && (off < 0 || (off & 7))) { MCC_TRACE("br\n");
					snprintf(out, osz, "%s\t%s, %s", load ? "ldr" : "str",
									 fr(dc, kind, rd), addr);
					return D_ASM;
				}
				snprintf(out, osz, "%s\t%s, %s", load ? "ldur" : "stur",
								 fr(dc, kind, rd), addr);
				return D_CMT;
			}
			snprintf(out, osz, "%s\t%s, %s", load ? "ldr" : "str",
							 fr(dc, kind, rd), addr);
			return D_CMT;
		}
		{
			static const char *ldnm[4] = {"ldrb", "ldrh", "ldr", "ldr"};
			static const char *stnm[4] = {"strb", "strh", "str", "str"};
			static const char *lunm[4] = {"ldurb", "ldurh", "ldur", "ldur"};
			static const char *sunm[4] = {"sturb", "sturh", "stur", "stur"};
			int tsf = (size == 3);
			if (opc > 1) { MCC_TRACE("br\n");
				const char *snm = size == 0
															? "ldursb"
													: size == 1
															? "ldursh"
													: size == 2
															? "ldursw"
															: 0;
				if (!snm)
					{ MCC_TRACE("br\n"); return D_UNK; }
				snprintf(out, osz, "%s\t%s, %s", snm,
								 ir(dc, opc == 2, rd), addr);
				return D_CMT;
			}
			if (idx == 0) { MCC_TRACE("br\n");
				if (off < 0 || (off & ((1 << size) - 1))) { MCC_TRACE("br\n");
					snprintf(out, osz, "%s\t%s, %s",
									 opc ? ldnm[size] : stnm[size], ir(dc, tsf, rd), addr);
					return D_ASM;
				}
				snprintf(out, osz, "%s\t%s, %s",
								 opc ? lunm[size] : sunm[size], ir(dc, tsf, rd), addr);
				return D_CMT;
			}
			snprintf(out, osz, "%s\t%s, %s",
							 opc ? ldnm[size] : stnm[size], ir(dc, tsf, rd), addr);
			return D_CMT;
		}
	}

	if ((w & 0x3b200c00) == 0x38200800) { MCC_TRACE("br\n");
		int size = w >> 30, V = (w >> 26) & 1, opc = (w >> 22) & 3;
		int load = opc & 1;
		if (V) { MCC_TRACE("br\n");
			int kind = (opc & 2)
										 ? 'q'
								 : size == 0
										 ? 'b'
								 : size == 1
										 ? 'h'
								 : size == 2
										 ? 's'
										 : 'd';
			snprintf(out, osz, "%s\t%s, [%s, %s]", load ? "ldr" : "str",
							 fr(dc, kind, rd), irsp(dc, 1, rn), ir(dc, 1, rm));
			return D_CMT;
		}
		{
			static const char *ldnm[4] = {"ldrb", "ldrh", "ldr", "ldr"};
			static const char *stnm[4] = {"strb", "strh", "str", "str"};
			const char *nm = opc > 1
													 ? (size == 0
																	? "ldrsb"
															: size == 1
																	? "ldrsh"
																	: "ldrsw")
													 : (load ? ldnm[size] : stnm[size]);
			snprintf(out, osz, "%s\t%s, [%s, %s]", nm,
							 ir(dc, opc > 1 ? opc == 2 : size == 3, rd),
							 irsp(dc, 1, rn), ir(dc, 1, rm));
			return D_CMT;
		}
	}

	if ((w & 0xff200c00) == 0x1e200800) { MCC_TRACE("br\n");
		static const char *nm[16] = {
				"fmul", "fdiv", "fadd", "fsub", "fmax", "fmin", "fmaxnm", "fminnm",
				"fnmul", 0, 0, 0, 0, 0, 0, 0};
		int type = (w >> 22) & 1, op = (w >> 12) & 15;
		int kind = type ? 'd' : 's';
		if (!nm[op])
			{ MCC_TRACE("br\n"); return D_UNK; }
		snprintf(out, osz, "%s\t%s, %s, %s", nm[op],
						 fr(dc, kind, rd), fr(dc, kind, rn), fr(dc, kind, rm));
		return D_CMT;
	}
	if ((w & 0xffbffc00) == 0x1e204000) { MCC_TRACE("br\n");
		int kind = (w & 0x400000) ? 'd' : 's';
		snprintf(out, osz, "fmov\t%s, %s", fr(dc, kind, rd), fr(dc, kind, rn));
		return D_CMT;
	}
	if ((w & 0xffbffc00) == 0x1e20c000) { MCC_TRACE("br\n");
		int kind = (w & 0x400000) ? 'd' : 's';
		snprintf(out, osz, "fabs\t%s, %s", fr(dc, kind, rd), fr(dc, kind, rn));
		return D_CMT;
	}
	if ((w & 0xffbffc00) == 0x1e214000) { MCC_TRACE("br\n");
		int kind = (w & 0x400000) ? 'd' : 's';
		snprintf(out, osz, "fneg\t%s, %s", fr(dc, kind, rd), fr(dc, kind, rn));
		return D_CMT;
	}
	if ((w & 0xffbffc00) == 0x1e21c000) { MCC_TRACE("br\n");
		int kind = (w & 0x400000) ? 'd' : 's';
		snprintf(out, osz, "fsqrt\t%s, %s", fr(dc, kind, rd), fr(dc, kind, rn));
		return D_CMT;
	}
	if ((w & 0xfffffc00) == 0x1e22c000) { MCC_TRACE("br\n");
		snprintf(out, osz, "fcvt\t%s, %s", fr(dc, 'd', rd), fr(dc, 's', rn));
		return D_CMT;
	}
	if ((w & 0xfffffc00) == 0x1e624000) { MCC_TRACE("br\n");
		snprintf(out, osz, "fcvt\t%s, %s", fr(dc, 's', rd), fr(dc, 'd', rn));
		return D_CMT;
	}
	if ((w & 0xffa0fc17) == 0x1e202000) { MCC_TRACE("br\n");
		int kind = (w & 0x400000) ? 'd' : 's';
		if (w & 8)
			{ MCC_TRACE("br\n"); snprintf(out, osz, "fcmp\t%s, #0.0", fr(dc, kind, rn)); }
		else
			{ MCC_TRACE("br\n"); snprintf(out, osz, "fcmp\t%s, %s", fr(dc, kind, rn), fr(dc, kind, rm)); }
		return D_CMT;
	}
	if ((w & 0x7fbefc00) == 0x1e220000) { MCC_TRACE("br\n");
		int kind = (w & 0x400000) ? 'd' : 's';
		snprintf(out, osz, "%s\t%s, %s", (w & 0x10000) ? "ucvtf" : "scvtf",
						 fr(dc, kind, rd), ir(dc, sf, rn));
		return D_CMT;
	}
	if ((w & 0x7fbefc00) == 0x1e380000) { MCC_TRACE("br\n");
		int kind = (w & 0x400000) ? 'd' : 's';
		snprintf(out, osz, "%s\t%s, %s", (w & 0x10000) ? "fcvtzu" : "fcvtzs",
						 ir(dc, sf, rd), fr(dc, kind, rn));
		return D_CMT;
	}
	if ((w & 0xfffffc00) == 0x9e660000) { MCC_TRACE("br\n");
		snprintf(out, osz, "fmov\t%s, %s", ir(dc, 1, rd), fr(dc, 'd', rn));
		return D_CMT;
	}
	if ((w & 0xfffffc00) == 0x9e670000) { MCC_TRACE("br\n");
		snprintf(out, osz, "fmov\t%s, %s", fr(dc, 'd', rd), ir(dc, 1, rn));
		return D_CMT;
	}
	if ((w & 0xfffffc00) == 0x1e260000) { MCC_TRACE("br\n");
		snprintf(out, osz, "fmov\t%s, %s", ir(dc, 0, rd), fr(dc, 's', rn));
		return D_CMT;
	}
	if ((w & 0xfffffc00) == 0x1e270000) { MCC_TRACE("br\n");
		snprintf(out, osz, "fmov\t%s, %s", fr(dc, 's', rd), ir(dc, 0, rn));
		return D_CMT;
	}
	if ((w & 0xffeffc00) == 0x4e083c00) { MCC_TRACE("br\n");
		snprintf(out, osz, "mov\t%s, %s.d[%d]", ir(dc, 1, rd), fr(dc, 'v', rn),
						 (int)(w >> 20) & 1);
		return D_CMT;
	}
	if ((w & 0xffe0fc00) == 0x4ea01c00) { MCC_TRACE("br\n");
		if (rn == rm)
			{ MCC_TRACE("br\n"); snprintf(out, osz, "mov\t%s.16b, %s.16b", fr(dc, 'v', rd),
							 fr(dc, 'v', rn)); }
		else
			{ MCC_TRACE("br\n"); snprintf(out, osz, "orr\t%s.16b, %s.16b, %s.16b", fr(dc, 'v', rd),
							 fr(dc, 'v', rn), fr(dc, 'v', rm)); }
		return D_CMT;
	}
	if ((w & 0xbfbf0000) == 0x0c000000 || (w & 0xbf9f0000) == 0x0d000000) { MCC_TRACE("br\n");
		snprintf(out, osz, "%s\t{...}, [%s]",
						 (w & 0x400000) ? "ld1" : "st1", irsp(dc, 1, rn));
		return D_CMT;
	}

	return D_UNK;
}

ST_FUNC int mcc_disasm_insn(disasm_ctx *dc) { MCC_TRACE("enter\n");
	char text[400];
	uint32_t w;
	int r;

	if (dc->pc + 4 > dc->size) { MCC_TRACE("br\n");
		if (!dc->collect)
			{ MCC_TRACE("br\n"); fprintf(dc->out, ".byte\t0x%02x", dc->data[dc->pc]); }
		return 1;
	}
	w = read32le(dc->data + dc->pc);
	r = decode(dc, w, text, sizeof text);
	if (!dc->collect) { MCC_TRACE("br\n");
		if (r == D_ASM)
			{ MCC_TRACE("br\n"); fputs(text, dc->out); }
		else if (r == D_CMT)
			{ MCC_TRACE("br\n"); fprintf(dc->out, ".long\t0x%08x\t// %s", w, text); }
		else
			{ MCC_TRACE("br\n"); fprintf(dc->out, ".long\t0x%08x", w); }
	}
	return 4;
}

ST_FUNC int mcc_disasm_reloc_size(int type) { MCC_TRACE("enter\n");
	switch (type) { MCC_TRACE("br\n");
	case R_AARCH64_ABS64:
		return 8;
	case R_AARCH64_ABS32:
	case R_AARCH64_PREL32:
		return 4;

	case R_AARCH64_CALL26:
	case R_AARCH64_JUMP26:
	case R_AARCH64_CONDBR19:
	case R_AARCH64_TSTBR14:
	case R_AARCH64_ADR_PREL_PG_HI21:
	case R_AARCH64_ADR_GOT_PAGE:
	case R_AARCH64_ADD_ABS_LO12_NC:
	case R_AARCH64_LD64_GOT_LO12_NC:
	case R_AARCH64_LDST8_ABS_LO12_NC:
	case R_AARCH64_LDST16_ABS_LO12_NC:
	case R_AARCH64_LDST32_ABS_LO12_NC:
	case R_AARCH64_LDST64_ABS_LO12_NC:
	case R_AARCH64_LDST128_ABS_LO12_NC:
	case R_AARCH64_MOVW_UABS_G0_NC:
	case R_AARCH64_MOVW_UABS_G1_NC:
	case R_AARCH64_MOVW_UABS_G2_NC:
	case R_AARCH64_MOVW_UABS_G3:
	case R_AARCH64_TLSLE_ADD_TPREL_HI12:
	case R_AARCH64_TLSLE_ADD_TPREL_LO12:
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
