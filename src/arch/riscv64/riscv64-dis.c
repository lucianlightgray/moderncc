#include "mcc.h"

#ifdef MCC_TARGET_RISCV64

static const char *const xrn[32] = {
		"zero", "ra", "sp", "gp", "tp", "t0", "t1", "t2",
		"s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
		"a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7",
		"s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"};
static const char *const frn[32] = {
		"ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7",
		"fs0", "fs1", "fa0", "fa1", "fa2", "fa3", "fa4", "fa5",
		"fa6", "fa7", "fs2", "fs3", "fs4", "fs5", "fs6", "fs7",
		"fs8", "fs9", "fs10", "fs11", "ft8", "ft9", "ft10", "ft11"};

static void P(disasm_ctx *dc, const char *fmt, ...) { MCC_TRACE("enter\n");
	va_list ap;
	if (dc->collect)
		{ MCC_TRACE("br\n"); return; }
	va_start(ap, fmt);
	vfprintf(dc->out, fmt, ap);
	va_end(ap);
}

static int raw32(disasm_ctx *dc, uint32_t w) { MCC_TRACE("enter\n");
	P(dc, ".long\t0x%08x", w);
	return 4;
}

static int32_t imm_i(uint32_t w) { MCC_TRACE("enter\n");
	return (int32_t)w >> 20;
}
static int32_t imm_s(uint32_t w) { MCC_TRACE("enter\n");
	return (((int32_t)w >> 25) << 5) | ((w >> 7) & 0x1f);
}
static int32_t imm_b(uint32_t w) { MCC_TRACE("enter\n");
	int32_t v = (((w >> 31) & 1) << 12) | (((w >> 7) & 1) << 11) | (((w >> 25) & 0x3f) << 5) | (((w >> 8) & 0xf) << 1);
	return (v << 19) >> 19;
}
static int32_t imm_j(uint32_t w) { MCC_TRACE("enter\n");
	int32_t v = (((w >> 31) & 1) << 20) | (((w >> 12) & 0xff) << 12) | (((w >> 20) & 1) << 11) | (((w >> 21) & 0x3ff) << 1);
	return (v << 11) >> 11;
}

static int plain_sym(const char *s) { MCC_TRACE("enter\n");
	return s && !strchr(s, '+') && !strchr(s, '-');
}

static const char *sym_esc(const char *s) { MCC_TRACE("enter\n");
	static const char *const csr[] = {
			"cycle", "fcsr", "fflags", "frm", "instret", "time",
			"cycleh", "instreth", "timeh", "pc", NULL};
	int i;
	if ((s[0] == 'x' || s[0] == 'f') && s[1] >= '0' && s[1] <= '9')
		{ MCC_TRACE("br\n"); return "0+"; }
	for (i = 0; i < 32; i++)
		if (!strcmp(s, xrn[i]) || !strcmp(s, frn[i]))
			{ MCC_TRACE("br\n"); return "0+"; }
	for (i = 0; csr[i]; i++)
		if (!strcmp(s, csr[i]))
			{ MCC_TRACE("br\n"); return "0+"; }
	return "";
}

static ElfW(Sym) * reloc_sym_at(disasm_ctx *dc, addr_t off, int type) { MCC_TRACE("enter\n");
	Section *sr = dc->sec->reloc;
	ElfW_Rel *rel;
	if (!sr)
		{ MCC_TRACE("br\n"); return NULL; }
	for_each_elem(sr, 0, rel, ElfW_Rel) {
		if (rel->r_offset == off && (int)ELFW(R_TYPE)(rel->r_info) == type)
			{ MCC_TRACE("br\n"); return &((ElfW(Sym) *)dc->symtab->data)[ELFW(R_SYM)(rel->r_info)]; }
	}
	return NULL;
}

static int defined_before(disasm_ctx *dc, ElfW(Sym) * s, addr_t pc) { MCC_TRACE("enter\n");
	return s && s->st_shndx == dc->sec->sh_num && s->st_value < pc;
}

static addr_t pcrel_lo_target(disasm_ctx *dc, addr_t off, int type) { MCC_TRACE("enter\n");
	ElfW(Sym) *s = reloc_sym_at(dc, off, type);
	if (!s || s->st_shndx != dc->sec->sh_num)
		{ MCC_TRACE("br\n"); return (addr_t)-1; }
	return s->st_value;
}

static int auipc_spelling(disasm_ctx *dc, addr_t A) { MCC_TRACE("enter\n");
	int rtype = 0, lotype = 0;
	const char *rel, *lo;
	char relbuf[256];

	if (A == (addr_t)-1 || A + 4 > dc->size || (read32le(dc->data + A) & 0x7f) != 0x17)
		{ MCC_TRACE("br\n"); return 0; }
	rel = disasm_reloc(dc, A, 4, &rtype);
	if (!rel)
		{ MCC_TRACE("br\n"); return 0; }
	snprintf(relbuf, sizeof relbuf, "%s", rel);
	lo = A + 4 < dc->size ? disasm_reloc(dc, A + 4, 4, &lotype) : NULL;
	if (lo && lotype == R_RISCV_PCREL_LO12_I && plain_sym(relbuf) && (rtype == R_RISCV_PCREL_HI20 || (rtype == R_RISCV_GOT_HI20 && !defined_before(dc, reloc_sym_at(dc, A, rtype), A))))
		{ MCC_TRACE("br\n"); return 1; }
	if (rtype == R_RISCV_PCREL_HI20 || rtype == R_RISCV_GOT_HI20)
		{ MCC_TRACE("br\n"); return 2; }
	return 0;
}

static int dis_auipc(disasm_ctx *dc, uint32_t w) { MCC_TRACE("enter\n");
	int rd = (w >> 7) & 0x1f;
	int rtype = 0, lotype = 0;
	char relbuf[256];
	const char *rel = disasm_reloc(dc, dc->pc, 4, &rtype);
	const char *lo;

	if (rel) { MCC_TRACE("br\n");
		snprintf(relbuf, sizeof relbuf, "%s", rel);
		rel = relbuf;
	}
	lo = dc->pc + 4 < dc->size
					 ? disasm_reloc(dc, dc->pc + 4, 4, &lotype)
					 : NULL;

	if (!rel) { MCC_TRACE("br\n");
		P(dc, "auipc\t%s, 0x%x", xrn[rd], (w >> 12) & 0xfffff);
		return 4;
	}
	if (rtype == R_RISCV_CALL || rtype == R_RISCV_CALL_PLT) { MCC_TRACE("br\n");
		ElfW(Sym) *s = reloc_sym_at(dc, dc->pc, rtype);
		if (plain_sym(rel)) { MCC_TRACE("br\n");
			if (defined_before(dc, s, dc->pc) && ELFW(ST_BIND)(s->st_info) != STB_LOCAL) { MCC_TRACE("br\n");
				P(dc, ".set\t.Lcs%llx, %s%s\n\tauipc\t%s, .Lcs%llx",
					(unsigned long long)dc->pc, sym_esc(rel), rel, xrn[rd],
					(unsigned long long)dc->pc);
			} else { MCC_TRACE("br\n");
				P(dc, "auipc\t%s, %s%s", xrn[rd], sym_esc(rel), rel);
			}
			return 4;
		}
		return raw32(dc, w);
	}
	if (rtype == R_RISCV_GOT_HI20 && plain_sym(rel) && lo && lotype == R_RISCV_PCREL_LO12_I && !defined_before(dc, reloc_sym_at(dc, dc->pc, rtype), dc->pc)) { MCC_TRACE("br\n");
		P(dc, ".globl\t%s\n\tauipc\t%s, %s%s", rel, xrn[rd], sym_esc(rel), rel);
		return 4;
	}
	if (rtype == R_RISCV_PCREL_HI20 && plain_sym(rel) && lo && lotype == R_RISCV_PCREL_LO12_I) { MCC_TRACE("br\n");
		P(dc, "auipc\t%s, %s%s", xrn[rd], sym_esc(rel), rel);
		return 4;
	}
	if (rtype == R_RISCV_PCREL_HI20 || rtype == R_RISCV_GOT_HI20) { MCC_TRACE("br\n");
		P(dc, ".Lpc%llx:\n\tauipc\t%s, %s(%s%s)",
			(unsigned long long)dc->pc, xrn[rd],
			rtype == R_RISCV_GOT_HI20 ? "%got_pcrel_hi" : "%pcrel_hi",
			sym_esc(rel), rel);
		return 4;
	}

	return raw32(dc, w);
}

static int dis_op_fp(disasm_ctx *dc, uint32_t w) { MCC_TRACE("enter\n");
	int rd = (w >> 7) & 0x1f, rm = (w >> 12) & 7;
	int rs1 = (w >> 15) & 0x1f, rs2 = (w >> 20) & 0x1f;
	int f7 = w >> 25;
	char fc = (f7 & 1) ? 'd' : 's';
	static const char *const cvt[4] = {"w", "wu", "l", "lu"};

	switch (f7 & ~1) { MCC_TRACE("br\n");
	case 0x00:
	case 0x04:
	case 0x08:
	case 0x0c: {
		static const char *const nm[4] = {"fadd", "fsub", "fmul", "fdiv"};
		if (rm != 7)
			{ MCC_TRACE("br\n"); break; }
		P(dc, "%s.%c\t%s, %s, %s", nm[(f7 >> 2) & 3], fc,
			frn[rd], frn[rs1], frn[rs2]);
		return 4;
	}
	case 0x10:
		if (rm == 0 && rs1 == rs2)
			{ MCC_TRACE("br\n"); P(dc, "fmv.%c\t%s, %s", fc, frn[rd], frn[rs1]); }
		else if (rm == 0)
			{ MCC_TRACE("br\n"); P(dc, "fsgnj.%c\t%s, %s, %s", fc, frn[rd], frn[rs1], frn[rs2]); }
		else if (rm == 1 && rs1 == rs2)
			{ MCC_TRACE("br\n"); P(dc, "fneg.%c\t%s, %s", fc, frn[rd], frn[rs1]); }
		else if (rm == 2 && rs1 == rs2)
			{ MCC_TRACE("br\n"); P(dc, "fabs.%c\t%s, %s", fc, frn[rd], frn[rs1]); }
		else
			{ MCC_TRACE("br\n"); break; }
		return 4;
	case 0x14:
		if (rm > 1)
			{ MCC_TRACE("br\n"); break; }
		P(dc, "%s.%c\t%s, %s, %s", rm ? "fmax" : "fmin", fc,
			frn[rd], frn[rs1], frn[rs2]);
		return 4;
	case 0x20: {
		static const char *const rmn[5] =
				{"rne", "rtz", "rdn", "rup", "rmm"};
		const char *nm;
		if (f7 == 0x20 && rs2 == 1)
			{ MCC_TRACE("br\n"); nm = "fcvt.s.d"; }
		else if (f7 == 0x21 && rs2 == 0)
			{ MCC_TRACE("br\n"); nm = "fcvt.d.s"; }
		else
			{ MCC_TRACE("br\n"); break; }
		if (rm == 7)
			{ MCC_TRACE("br\n"); P(dc, "%s\t%s, %s", nm, frn[rd], frn[rs1]); }
		else if (rm <= 4)
			{ MCC_TRACE("br\n"); P(dc, "%s\t%s, %s, %s", nm, frn[rd], frn[rs1], rmn[rm]); }
		else
			{ MCC_TRACE("br\n"); break; }
		return 4;
	}
	case 0x2c:
		if (rm != 7 || rs2 != 0)
			{ MCC_TRACE("br\n"); break; }
		P(dc, "fsqrt.%c\t%s, %s", fc, frn[rd], frn[rs1]);
		return 4;
	case 0x50: {
		static const char *const nm[3] = {"fle", "flt", "feq"};
		if (rm > 2)
			{ MCC_TRACE("br\n"); break; }
		P(dc, "%s.%c\t%s, %s, %s", nm[rm], fc,
			xrn[rd], frn[rs1], frn[rs2]);
		return 4;
	}
	case 0x60:
		if (rs2 > 3 || (rm != 1 && rm != 7))
			{ MCC_TRACE("br\n"); break; }
		P(dc, "fcvt.%s.%c\t%s, %s%s", cvt[rs2], fc, xrn[rd], frn[rs1],
			rm == 1 ? ", rtz" : "");
		return 4;
	case 0x68:
		if (rs2 > 3 || rm != 7)
			{ MCC_TRACE("br\n"); break; }
		P(dc, "fcvt.%c.%s\t%s, %s", fc, cvt[rs2], frn[rd], xrn[rs1]);
		return 4;
	case 0x70:
		if (rm == 1 && rs2 == 0) { MCC_TRACE("br\n");
			P(dc, "fclass.%c\t%s, %s", fc, xrn[rd], frn[rs1]);
			return 4;
		}
		if (rm == 0 && rs2 == 0) { MCC_TRACE("br\n");
			P(dc, "fmv.x.%c\t%s, %s", (f7 & 1) ? 'd' : 'w',
				xrn[rd], frn[rs1]);
			return 4;
		}
		break;
	case 0x78:
		if (rm == 0 && rs2 == 0) { MCC_TRACE("br\n");
			P(dc, "fmv.%c.x\t%s, %s", (f7 & 1) ? 'd' : 'w',
				frn[rd], xrn[rs1]);
			return 4;
		}
		break;
	}
	return raw32(dc, w);
}

static int dis_insn(disasm_ctx *dc) { MCC_TRACE("enter\n");
	addr_t pc = dc->pc;
	uint32_t w;
	int rd, f3, rs1, rs2, rtype;
	const char *rel;

	if (pc + 2 > dc->size) { MCC_TRACE("br\n");
		P(dc, ".byte\t0x%02x", dc->data[pc]);
		return 1;
	}
	w = read16le(dc->data + pc);
	if ((w & 3) != 3 || pc + 4 > dc->size) { MCC_TRACE("br\n");
		P(dc, ".short\t0x%04x", w);
		return 2;
	}
	w = read32le(dc->data + pc);
	rd = (w >> 7) & 0x1f;
	f3 = (w >> 12) & 7;
	rs1 = (w >> 15) & 0x1f;
	rs2 = (w >> 20) & 0x1f;

	switch (w & 0x7f) { MCC_TRACE("br\n");
	case 0x37:
		if (disasm_reloc(dc, pc, 4, &rtype))
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		P(dc, "lui\t%s, 0x%x", xrn[rd], (w >> 12) & 0xfffff);
		return 4;

	case 0x17:
		return dis_auipc(dc, w);

	case 0x6f: {
		int32_t off = imm_j(w);
		disasm_label(dc, pc + off);
		if (rd == 0)
			{ MCC_TRACE("br\n"); P(dc, "j\t%d", off); }
		else if (rd == 1)
			{ MCC_TRACE("br\n"); P(dc, "jal\t%d", off); }
		else
			{ MCC_TRACE("br\n"); P(dc, "jal\t%s, %d", xrn[rd], off); }
		return 4;
	}

	case 0x67:
		if (f3 != 0 || disasm_reloc(dc, pc, 4, &rtype))
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		if (rd == 0 && rs1 == 1 && imm_i(w) == 0)
			{ MCC_TRACE("br\n"); P(dc, "ret"); }
		else
			{ MCC_TRACE("br\n"); P(dc, "jalr\t%s, %d(%s)", xrn[rd], imm_i(w), xrn[rs1]); }
		return 4;

	case 0x63: {
		static const char *const nm[8] =
				{"beq", "bne", 0, 0, "blt", "bge", "bltu", "bgeu"};
		int32_t off = imm_b(w);
		if (!nm[f3] || disasm_reloc(dc, pc, 4, &rtype))
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		disasm_label(dc, pc + off);
		P(dc, "%s\t%s, %s, %d", nm[f3], xrn[rs1], xrn[rs2], off);
		return 4;
	}

	case 0x03: {
		static const char *const nm[8] =
				{"lb", "lh", "lw", "ld", "lbu", "lhu", "lwu", 0};

		rel = disasm_reloc(dc, pc, 4, &rtype);
		if (!nm[f3] || (rel && rtype != R_RISCV_PCREL_LO12_I))
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		if (rel) { MCC_TRACE("br\n");
			addr_t A = pcrel_lo_target(dc, pc, rtype);
			if (auipc_spelling(dc, A) == 2) { MCC_TRACE("br\n");
				P(dc, "%s\t%s, %%pcrel_lo(.Lpc%llx)(%s)", nm[f3], xrn[rd],
					(unsigned long long)A, xrn[rs1]);
				return 4;
			}
		}
		P(dc, "%s\t%s, %d(%s)", nm[f3], xrn[rd], imm_i(w), xrn[rs1]);
		return 4;
	}

	case 0x23: {
		static const char *const nm[8] = {"sb", "sh", "sw", "sd"};
		rel = disasm_reloc(dc, pc, 4, &rtype);
		if (f3 > 3 || (rel && rtype != R_RISCV_PCREL_LO12_S))
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		if (rel) { MCC_TRACE("br\n");
			addr_t A = pcrel_lo_target(dc, pc, rtype);
			if (auipc_spelling(dc, A) == 2) { MCC_TRACE("br\n");
				P(dc, "%s\t%s, %%pcrel_lo(.Lpc%llx)(%s)", nm[f3], xrn[rs2],
					(unsigned long long)A, xrn[rs1]);
				return 4;
			}
		}
		P(dc, "%s\t%s, %d(%s)", nm[f3], xrn[rs2], imm_s(w), xrn[rs1]);
		return 4;
	}

	case 0x07:
		rel = disasm_reloc(dc, pc, 4, &rtype);
		if ((f3 != 2 && f3 != 3) || (rel && rtype != R_RISCV_PCREL_LO12_I))
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		if (rel) { MCC_TRACE("br\n");
			addr_t A = pcrel_lo_target(dc, pc, rtype);
			if (auipc_spelling(dc, A) == 2) { MCC_TRACE("br\n");
				P(dc, "%s\t%s, %%pcrel_lo(.Lpc%llx)(%s)",
					f3 == 3 ? "fld" : "flw", frn[rd],
					(unsigned long long)A, xrn[rs1]);
				return 4;
			}
		}
		P(dc, "%s\t%s, %d(%s)", f3 == 3 ? "fld" : "flw", frn[rd],
			imm_i(w), xrn[rs1]);
		return 4;

	case 0x27:
		rel = disasm_reloc(dc, pc, 4, &rtype);
		if ((f3 != 2 && f3 != 3) || (rel && rtype != R_RISCV_PCREL_LO12_S))
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		if (rel) { MCC_TRACE("br\n");
			addr_t A = pcrel_lo_target(dc, pc, rtype);
			if (auipc_spelling(dc, A) == 2) { MCC_TRACE("br\n");
				P(dc, "%s\t%s, %%pcrel_lo(.Lpc%llx)(%s)",
					f3 == 3 ? "fsd" : "fsw", frn[rs2],
					(unsigned long long)A, xrn[rs1]);
				return 4;
			}
		}
		P(dc, "%s\t%s, %d(%s)", f3 == 3 ? "fsd" : "fsw", frn[rs2],
			imm_s(w), xrn[rs1]);
		return 4;

	case 0x13: {
		static const char *const nm[8] =
				{"addi", 0, "slti", "sltiu", "xori", 0, "ori", "andi"};
		int32_t imm = imm_i(w);
		rel = disasm_reloc(dc, pc, 4, &rtype);
		if (rel && rtype != R_RISCV_PCREL_LO12_I && rtype != R_RISCV_TPREL_LO12_I)
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		if (f3 == 1 || f3 == 5) { MCC_TRACE("br\n");
			int top6 = w >> 26;
			if (f3 == 1 && top6 == 0)
				{ MCC_TRACE("br\n"); P(dc, "slli\t%s, %s, %d", xrn[rd], xrn[rs1], imm & 63); }
			else if (f3 == 5 && top6 == 0)
				{ MCC_TRACE("br\n"); P(dc, "srli\t%s, %s, %d", xrn[rd], xrn[rs1], imm & 63); }
			else if (f3 == 5 && top6 == 0x10)
				{ MCC_TRACE("br\n"); P(dc, "srai\t%s, %s, %d", xrn[rd], xrn[rs1], imm & 63); }
			else
				{ MCC_TRACE("br\n"); return raw32(dc, w); }
			return 4;
		}
		if (f3 == 0 && rel && rtype == R_RISCV_PCREL_LO12_I) { MCC_TRACE("br\n");
			addr_t A = pcrel_lo_target(dc, pc, rtype);
			if (auipc_spelling(dc, A) == 2) { MCC_TRACE("br\n");
				P(dc, "addi\t%s, %s, %%pcrel_lo(.Lpc%llx)", xrn[rd],
					xrn[rs1], (unsigned long long)A);
				return 4;
			}
		}
		if (f3 == 0 && w == 0x13)
			{ MCC_TRACE("br\n"); P(dc, "nop"); }
		else if (f3 == 0 && imm == 0 && !rel)
			{ MCC_TRACE("br\n"); P(dc, "mv\t%s, %s", xrn[rd], xrn[rs1]); }
		else
			{ MCC_TRACE("br\n"); P(dc, "%s\t%s, %s, %d", nm[f3], xrn[rd], xrn[rs1], imm); }
		return 4;
	}

	case 0x1b:
		if (disasm_reloc(dc, pc, 4, &rtype))
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		if (f3 == 0)
			{ MCC_TRACE("br\n"); P(dc, "addiw\t%s, %s, %d", xrn[rd], xrn[rs1], imm_i(w)); }
		else if (f3 == 1 && (w >> 25) == 0)
			{ MCC_TRACE("br\n"); P(dc, "slliw\t%s, %s, %d", xrn[rd], xrn[rs1], rs2); }
		else if (f3 == 5 && (w >> 25) == 0)
			{ MCC_TRACE("br\n"); P(dc, "srliw\t%s, %s, %d", xrn[rd], xrn[rs1], rs2); }
		else if (f3 == 5 && (w >> 25) == 0x20)
			{ MCC_TRACE("br\n"); P(dc, "sraiw\t%s, %s, %d", xrn[rd], xrn[rs1], rs2); }
		else
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		return 4;

	case 0x33:
	case 0x3b: {
		static const char *const base[8] =
				{"add", "sll", "slt", "sltu", "xor", "srl", "or", "and"};
		static const char *const muldiv[8] =
				{"mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu"};
		int w32 = (w & 0x7f) == 0x3b;
		int f7 = w >> 25;
		const char *nm = NULL;
		if (f7 == 0x00)
			{ MCC_TRACE("br\n"); nm = base[f3]; }
		else if (f7 == 0x20 && f3 == 0)
			{ MCC_TRACE("br\n"); nm = "sub"; }
		else if (f7 == 0x20 && f3 == 5)
			{ MCC_TRACE("br\n"); nm = "sra"; }
		else if (f7 == 0x01)
			{ MCC_TRACE("br\n"); nm = muldiv[f3]; }
		if (!nm)
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		if (w32) { MCC_TRACE("br\n");
			if (strcmp(nm, "add") && strcmp(nm, "sub") && strcmp(nm, "sll") && strcmp(nm, "srl") &&
					strcmp(nm, "sra") && strcmp(nm, "mul") && strcmp(nm, "div") && strcmp(nm, "divu") &&
					strcmp(nm, "rem") && strcmp(nm, "remu"))
				{ MCC_TRACE("br\n"); return raw32(dc, w); }
			P(dc, "%sw\t%s, %s, %s", nm, xrn[rd], xrn[rs1], xrn[rs2]);
		} else { MCC_TRACE("br\n");
			P(dc, "%s\t%s, %s, %s", nm, xrn[rd], xrn[rs1], xrn[rs2]);
		}
		return 4;
	}

	case 0x53:
		return dis_op_fp(dc, w);

	case 0x0f:
		if (w == 0x0ff0000f)
			{ MCC_TRACE("br\n"); P(dc, "fence"); }
		else if (w == 0x0000100f)
			{ MCC_TRACE("br\n"); P(dc, "fence.i"); }
		else
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		return 4;

	case 0x73:
		if (w == 0x00000073)
			{ MCC_TRACE("br\n"); P(dc, "ecall"); }
		else if (w == 0x00100073)
			{ MCC_TRACE("br\n"); P(dc, "ebreak"); }
		else
			{ MCC_TRACE("br\n"); return raw32(dc, w); }
		return 4;
	}
	return raw32(dc, w);
}

ST_FUNC int mcc_disasm_insn(disasm_ctx *dc) { MCC_TRACE("enter\n");
	return dis_insn(dc);
}

ST_FUNC int mcc_disasm_reloc_size(int type) { MCC_TRACE("enter\n");
	switch (type) { MCC_TRACE("br\n");
	case R_RISCV_64:
		return 8;
	case R_RISCV_32:
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
