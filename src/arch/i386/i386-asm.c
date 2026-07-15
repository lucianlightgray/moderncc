#define USING_GLOBALS
#include "mcc.h"
#if MCC_CONFIG_ASM

#define MAX_OPERANDS 3

#define TOK_ASM_first TOK_ASM_clc
#define TOK_ASM_last TOK_ASM_emms
#define TOK_ASM_alllast TOK_ASM_subps

#define OPC_B 0x01
#define OPC_WL 0x02
#define OPC_BWL (OPC_B | OPC_WL)
#define OPC_REG 0x04
#define OPC_MODRM 0x08

#define OPCT_MASK 0x70
#define OPC_FWAIT 0x10
#define OPC_SHIFT 0x20
#define OPC_ARITH 0x30
#define OPC_FARITH 0x40
#define OPC_TEST 0x50
#define OPC_0F01 0x60
#define OPCT_IS(v, i) (((v) & OPCT_MASK) == (i))

#define OPC_0F 0x100
#define OPC_48 0x200
#ifdef MCC_TARGET_X86_64
#define OPC_WLQ 0x1000
#define OPC_BWLQ (OPC_B | OPC_WLQ)
#define OPC_WLX OPC_WLQ
#define OPC_BWLX OPC_BWLQ
#else
#define OPC_WLX OPC_WL
#define OPC_BWLX OPC_BWL
#endif

#define OPC_GROUP_SHIFT 13

enum {
	OPT_REG8 = 0,
	OPT_REG16,
	OPT_REG32,
#ifdef MCC_TARGET_X86_64
	OPT_REG64,
#endif
	OPT_MMX,
	OPT_SSE,
	OPT_CR,
	OPT_TR,
	OPT_DB,
	OPT_SEG,
	OPT_ST,
#ifdef MCC_TARGET_X86_64
	OPT_REG8_LOW,
#endif
	OPT_IM8,
	OPT_IM8S,
	OPT_IM16,
	OPT_IM32,
#ifdef MCC_TARGET_X86_64
	OPT_IM64,
#endif
	OPT_EAX,
	OPT_ST0,
	OPT_CL,
	OPT_DX,
	OPT_ADDR,
	OPT_INDIR,
	OPT_COMPOSITE_FIRST,
	OPT_IM,
	OPT_REG,
	OPT_REGW,
	OPT_IMW,
	OPT_MMXSSE,
	OPT_DISP,
	OPT_DISP8,
	OPT_EA = 0x80
};

#define OP_REG8 (1 << OPT_REG8)
#define OP_REG16 (1 << OPT_REG16)
#define OP_REG32 (1 << OPT_REG32)
#define OP_MMX (1 << OPT_MMX)
#define OP_SSE (1 << OPT_SSE)
#define OP_CR (1 << OPT_CR)
#define OP_TR (1 << OPT_TR)
#define OP_DB (1 << OPT_DB)
#define OP_SEG (1 << OPT_SEG)
#define OP_ST (1 << OPT_ST)
#define OP_IM8 (1 << OPT_IM8)
#define OP_IM8S (1 << OPT_IM8S)
#define OP_IM16 (1 << OPT_IM16)
#define OP_IM32 (1 << OPT_IM32)
#define OP_EAX (1 << OPT_EAX)
#define OP_ST0 (1 << OPT_ST0)
#define OP_CL (1 << OPT_CL)
#define OP_DX (1 << OPT_DX)
#define OP_ADDR (1 << OPT_ADDR)
#define OP_INDIR (1 << OPT_INDIR)
#ifdef MCC_TARGET_X86_64
#define OP_REG64 (1 << OPT_REG64)
#define OP_REG8_LOW (1 << OPT_REG8_LOW)
#define OP_IM64 (1 << OPT_IM64)
#define OP_EA32 (OP_EA << 1)
#else
#define OP_REG64 0
#define OP_REG8_LOW 0
#define OP_IM64 0
#define OP_EA32 0
#endif

#define OP_EA 0x40000000u
#define OP_REG (OP_REG8 | OP_REG16 | OP_REG32 | OP_REG64)

#ifdef MCC_TARGET_X86_64
#define MCC_TREG_XAX MCC_TREG_RAX
#define MCC_TREG_XCX MCC_TREG_RCX
#define MCC_TREG_XDX MCC_TREG_RDX
#define TOK_ASM_xax TOK_ASM_rax
#else
#define MCC_TREG_XAX MCC_TREG_EAX
#define MCC_TREG_XCX MCC_TREG_ECX
#define MCC_TREG_XDX MCC_TREG_EDX
#define TOK_ASM_xax TOK_ASM_eax
#endif

typedef struct ASMInstr {
	uint16_t sym;
	uint16_t opcode;
	uint16_t instr_type;
	uint8_t nb_ops;
	uint8_t op_type[MAX_OPERANDS];
} ASMInstr;

typedef struct Operand {
	uint32_t type;
	int8_t reg;
	int8_t reg2;
	uint8_t shift;
	uint8_t ntpoff;
	ExprValue e;
} Operand;

static const uint8_t reg_to_size[9] = {
		0, 0, 1, 0, 2, 0, 0, 0, 3};

#define NB_TEST_OPCODES 30

static const uint8_t test_bits[NB_TEST_OPCODES] = {
		0x00,
		0x01,
		0x02,
		0x02,
		0x02,
		0x03,
		0x03,
		0x03,
		0x04,
		0x04,
		0x05,
		0x05,
		0x06,
		0x06,
		0x07,
		0x07,
		0x08,
		0x09,
		0x0a,
		0x0a,
		0x0b,
		0x0b,
		0x0c,
		0x0c,
		0x0d,
		0x0d,
		0x0e,
		0x0e,
		0x0f,
		0x0f,
};

static const uint8_t segment_prefixes[] = {
		0x26,
		0x2e,
		0x36,
		0x3e,
		0x64,
		0x65};

static const ASMInstr asm_instrs[] = {
#define ALT(x) x
#define O(o) ((uint64_t)((((o) & 0xff00) == 0x0f00) ? ((((o) >> 8) & ~0xff) | ((o) & 0xff)) : (o)))
#define T(o, i, g) ((i) | ((g) << OPC_GROUP_SHIFT) | ((((o) & 0xff00) == 0x0f00) ? OPC_0F : 0))
#define DEF_ASM_OP0(name, opcode)
#define DEF_ASM_OP0L(name, opcode, group, instr_type) {TOK_ASM_##name, O(opcode), T(opcode, instr_type, group), 0, {0}},
#define DEF_ASM_OP1(name, opcode, group, instr_type, op0) {TOK_ASM_##name, O(opcode), T(opcode, instr_type, group), 1, {op0}},
#define DEF_ASM_OP2(name, opcode, group, instr_type, op0, op1) {TOK_ASM_##name, O(opcode), T(opcode, instr_type, group), 2, {op0, op1}},
#define DEF_ASM_OP3(name, opcode, group, instr_type, op0, op1, op2) {TOK_ASM_##name, O(opcode), T(opcode, instr_type, group), 3, {op0, op1, op2}},
#ifdef MCC_TARGET_X86_64
#include "x86_64-asm.h"

#else
#include "i386-asm.h"
#endif
		{
				0,
		},
};

static const uint16_t op0_codes[] = {
#define ALT(x)
#define DEF_ASM_OP0(x, opcode) opcode,
#define DEF_ASM_OP0L(name, opcode, group, instr_type)
#define DEF_ASM_OP1(name, opcode, group, instr_type, op0)
#define DEF_ASM_OP2(name, opcode, group, instr_type, op0, op1)
#define DEF_ASM_OP3(name, opcode, group, instr_type, op0, op1, op2)
#ifdef MCC_TARGET_X86_64
#include "x86_64-asm.h"

#else
#include "i386-asm.h"
#endif
};

static inline int get_reg_shift(MCCState *s1) { MCC_TRACE("enter\n");
	int shift, v;
	v = asm_int_expr(s1);
	switch (v) { MCC_TRACE("br\n");
	case 1:
		shift = 0;
		break;
	case 2:
		shift = 1;
		break;
	case 4:
		shift = 2;
		break;
	case 8:
		shift = 3;
		break;
	default:
		expect("1, 2, 4 or 8 constant");
		shift = 0;
		break;
	}
	return shift;
}

#ifdef MCC_TARGET_X86_64
static int asm_parse_numeric_reg(int t, unsigned int *type) { MCC_TRACE("enter\n");
	int reg = -1;
	if (t >= TOK_IDENT && t < tok_ident) { MCC_TRACE("br\n");
		const char *s = table_ident[t - TOK_IDENT]->str;
		char c;
		*type = OP_REG64;
		if (*s == 'c') { MCC_TRACE("br\n");
			s++;
			*type = OP_CR;
		}
		if (*s++ != 'r')
			{ MCC_TRACE("br\n"); return -1; }
		if ((c = *s++) >= '1' && c <= '9')
			{ MCC_TRACE("br\n"); reg = c - '0'; }
		else
			{ MCC_TRACE("br\n"); return -1; }
		if ((c = *s) >= '0' && c <= '5')
			{ MCC_TRACE("br\n"); s++, reg = reg * 10 + c - '0'; }
		if (reg > 15)
			{ MCC_TRACE("br\n"); return -1; }
		if ((c = *s) == 0)
			{ MCC_TRACE("br\n"); ; }
		else if (*type != OP_REG64)
			{ MCC_TRACE("br\n"); return -1; }
		else if (c == 'b' && !s[1])
			{ MCC_TRACE("br\n"); *type = OP_REG8; }
		else if (c == 'w' && !s[1])
			{ MCC_TRACE("br\n"); *type = OP_REG16; }
		else if (c == 'd' && !s[1])
			{ MCC_TRACE("br\n"); *type = OP_REG32; }
		else
			{ MCC_TRACE("br\n"); return -1; }
	}
	return reg;
}
#endif

static int asm_parse_reg(unsigned int *type) { MCC_TRACE("enter\n");
	int reg = 0;
	*type = 0;
	if (tok != '%')
		{ MCC_TRACE("br\n"); goto error_32; }
	next();
	if (tok >= TOK_ASM_eax && tok <= TOK_ASM_edi) { MCC_TRACE("br\n");
		reg = tok - TOK_ASM_eax;
		*type = OP_REG32;
#ifdef MCC_TARGET_X86_64
	} else if (tok >= TOK_ASM_rax && tok <= TOK_ASM_rdi) { MCC_TRACE("br\n");
		reg = tok - TOK_ASM_rax;
		*type = OP_REG64;
	} else if (tok == TOK_ASM_rip) { MCC_TRACE("br\n");
		reg = -2;
		*type = OP_REG64;
	} else if ((reg = asm_parse_numeric_reg(tok, type)) >= 0 && (*type == OP_REG32 || *type == OP_REG64)) { MCC_TRACE("br\n");
		;
#endif
	} else { MCC_TRACE("br\n");
	error_32:
		expect("register");
	}
	next();
	return reg;
}

#ifndef MCC_TARGET_X86_64

static int asm_parse_ntpoff(void) { MCC_TRACE("enter\n");
	if (tok != '@')
		{ MCC_TRACE("br\n"); return 0; }
	next();
	if (tok < TOK_IDENT || strcmp(get_tok_str(tok, NULL), "ntpoff"))
		{ MCC_TRACE("br\n"); mcc_error("unsupported relocation operator '@%s'",
							get_tok_str(tok, &tokc)); }
	next();
	return 1;
}
#else
#define asm_parse_ntpoff() 0
#endif

static void parse_operand(MCCState *s1, Operand *op) { MCC_TRACE("enter\n");
	ExprValue e;
	int reg, indir;
	const char *p;

	op->ntpoff = 0;
	indir = 0;
	if (tok == '*') { MCC_TRACE("br\n");
		next();
		indir = OP_INDIR;
	}

	if (tok == '%') { MCC_TRACE("br\n");
		next();
		if (tok >= TOK_ASM_al && tok <= TOK_ASM_db7) { MCC_TRACE("br\n");
			reg = tok - TOK_ASM_al;
			op->type = 1 << (reg >> 3);
			op->reg = reg & 7;
			if ((op->type & OP_REG) && op->reg == MCC_TREG_XAX)
				{ MCC_TRACE("br\n"); op->type |= OP_EAX; }
			else if (op->type == OP_REG8 && op->reg == MCC_TREG_XCX)
				{ MCC_TRACE("br\n"); op->type |= OP_CL; }
			else if (op->type == OP_REG16 && op->reg == MCC_TREG_XDX)
				{ MCC_TRACE("br\n"); op->type |= OP_DX; }
		} else if (tok >= TOK_ASM_dr0 && tok <= TOK_ASM_dr7) { MCC_TRACE("br\n");
			op->type = OP_DB;
			op->reg = tok - TOK_ASM_dr0;
		} else if (tok >= TOK_ASM_es && tok <= TOK_ASM_gs) { MCC_TRACE("br\n");
			op->type = OP_SEG;
			op->reg = tok - TOK_ASM_es;
		} else if (tok == TOK_ASM_st) { MCC_TRACE("br\n");
			op->type = OP_ST;
			op->reg = 0;
			next();
			if (tok == '(') { MCC_TRACE("br\n");
				next();
				if (tok != TOK_PPNUM)
					{ MCC_TRACE("br\n"); goto reg_error; }
				p = tokc.str.data;
				reg = p[0] - '0';
				if ((unsigned)reg >= 8 || p[1] != '\0')
					{ MCC_TRACE("br\n"); goto reg_error; }
				op->reg = reg;
				next();
				skip(')');
			}
			if (op->reg == 0)
				{ MCC_TRACE("br\n"); op->type |= OP_ST0; }
			goto no_skip;
#ifdef MCC_TARGET_X86_64
		} else if (tok >= TOK_ASM_spl && tok <= TOK_ASM_dil) { MCC_TRACE("br\n");
			op->type = OP_REG8 | OP_REG8_LOW;
			op->reg = 4 + tok - TOK_ASM_spl;
		} else if ((op->reg = asm_parse_numeric_reg(tok, &op->type)) >= 0) { MCC_TRACE("br\n");
			;
#endif
		} else { MCC_TRACE("br\n");
		reg_error:
			mcc_error("unknown register %%%s", get_tok_str(tok, &tokc));
		}
		next();
	no_skip:;
	} else if (tok == '$') { MCC_TRACE("br\n");
		next();
		asm_expr(s1, &e);
		op->type = OP_IM32;
		op->e = e;
		if (op->e.sym)
			{ MCC_TRACE("br\n"); op->ntpoff = asm_parse_ntpoff(); }
		if (!op->e.sym) { MCC_TRACE("br\n");
			if (op->e.v == (uint8_t)op->e.v)
				{ MCC_TRACE("br\n"); op->type |= OP_IM8; }
			if (op->e.v == (int8_t)op->e.v)
				{ MCC_TRACE("br\n"); op->type |= OP_IM8S; }
			if (op->e.v == (uint16_t)op->e.v)
				{ MCC_TRACE("br\n"); op->type |= OP_IM16; }
#ifdef MCC_TARGET_X86_64
			if (op->e.v != (int32_t)op->e.v && op->e.v != (uint32_t)op->e.v)
				{ MCC_TRACE("br\n"); op->type = OP_IM64; }
#endif
		}
	} else { MCC_TRACE("br\n");
		op->type = OP_EA;
		op->reg = -1;
		op->reg2 = -1;
		op->shift = 0;
		if (tok != '(') { MCC_TRACE("br\n");
			asm_expr(s1, &e);
			op->e = e;
			if (op->e.sym)
				{ MCC_TRACE("br\n"); op->ntpoff = asm_parse_ntpoff(); }
		} else { MCC_TRACE("br\n");
			next();
			if (tok == '%') { MCC_TRACE("br\n");
				unget_tok('(');
				op->e.v = 0;
				op->e.sym = NULL;
			} else { MCC_TRACE("br\n");
				asm_expr(s1, &e);
				if (tok != ')')
					{ MCC_TRACE("br\n"); expect(")"); }
				next();
				op->e.v = e.v;
				op->e.sym = e.sym;
			}
			op->e.pcrel = 0;
		}
		if (tok == '(') { MCC_TRACE("br\n");
			unsigned int type = 0;
			next();
			if (tok != ',') { MCC_TRACE("br\n");
				op->reg = asm_parse_reg(&type);
			}
			if (tok == ',') { MCC_TRACE("br\n");
				next();
				if (tok != ',') { MCC_TRACE("br\n");
					op->reg2 = asm_parse_reg(&type);
				}
				if (tok == ',') { MCC_TRACE("br\n");
					next();
					op->shift = get_reg_shift(s1);
				}
			}
			if (type & OP_REG32)
				{ MCC_TRACE("br\n"); op->type |= OP_EA32; }
			skip(')');
		}
		if (op->reg == -1 && op->reg2 == -1)
			{ MCC_TRACE("br\n"); op->type |= OP_ADDR; }
	}
	op->type |= indir;
}

ST_FUNC void gen_expr32(ExprValue *pe) { MCC_TRACE("enter\n");
	if (pe->pcrel)
		{ MCC_TRACE("br\n"); gen_addrpc32(VT_SYM, pe->sym, pe->v + (ind + 4)); }
	else
		{ MCC_TRACE("br\n"); gen_addr32(pe->sym ? VT_SYM : 0, pe->sym, pe->v); }
}

static void gen_disp32(ExprValue *pe) { MCC_TRACE("enter\n");
	Sym *sym = pe->sym;
	ElfSym *esym = elfsym(sym);
	if (esym && esym->st_shndx == cur_text_section->sh_num) { MCC_TRACE("br\n");
		gen_le32(pe->v + esym->st_value - ind - 4);
	} else { MCC_TRACE("br\n");
		if (sym && sym->type.t == VT_VOID) { MCC_TRACE("br\n");
			sym->type.t = VT_FUNC;
			sym->type.ref = NULL;
		}
#ifdef MCC_TARGET_X86_64
		greloca(cur_text_section, sym, ind, R_X86_64_PLT32, pe->v - 4);
		gen_le32(0);
#else
		gen_addrpc32(VT_SYM, sym, pe->v);
#endif
	}
}

static void gen_op32(Operand *op) { MCC_TRACE("enter\n");
#ifndef MCC_TARGET_X86_64
	if (op->ntpoff) { MCC_TRACE("br\n");
		greloc(cur_text_section, op->e.sym, ind, R_386_TLS_LE);
		gen_le32(op->e.v);
		return;
	}
#endif
	gen_expr32(&op->e);
}

static inline int asm_modrm(int reg, Operand *op) { MCC_TRACE("enter\n");
	int mod, reg1, reg2, sib_reg1;

	if (op->type & (OP_REG | OP_MMX | OP_SSE)) { MCC_TRACE("br\n");
		g(0xc0 + (reg << 3) + op->reg);
	} else if (op->reg == -1 && op->reg2 == -1) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_X86_64
		g(0x04 + (reg << 3));
		g(0x25);
#else
		if (op->ntpoff) { MCC_TRACE("br\n");
			g(0x04 + (reg << 3));
			g(0x25);
		} else
			{ MCC_TRACE("br\n"); g(0x05 + (reg << 3)); }
#endif
		gen_op32(op);
#ifdef MCC_TARGET_X86_64
	} else if (op->reg == -2) { MCC_TRACE("br\n");
		ExprValue *pe = &op->e;
		g(0x05 + (reg << 3));
		gen_addrpc32(pe->sym ? VT_SYM : 0, pe->sym, pe->v);
		return ind;
#endif
	} else { MCC_TRACE("br\n");
		sib_reg1 = op->reg;
		if (sib_reg1 == -1) { MCC_TRACE("br\n");
			sib_reg1 = 5;
			mod = 0x00;
		} else if (op->e.v == 0 && !op->e.sym && op->reg != 5) { MCC_TRACE("br\n");
			mod = 0x00;
		} else if (op->e.v == (int8_t)op->e.v && !op->e.sym) { MCC_TRACE("br\n");
			mod = 0x40;
		} else { MCC_TRACE("br\n");
			mod = 0x80;
		}
		reg1 = op->reg;
		if (op->reg2 != -1)
			{ MCC_TRACE("br\n"); reg1 = 4; }
		g(mod + (reg << 3) + reg1);
		if (reg1 == 4) { MCC_TRACE("br\n");
			reg2 = op->reg2;
			if (reg2 == -1)
				{ MCC_TRACE("br\n"); reg2 = 4; }
			g((op->shift << 6) + (reg2 << 3) + sib_reg1);
		}
		if (mod == 0x40) { MCC_TRACE("br\n");
			g(op->e.v);
		} else if (mod == 0x80 || op->reg == -1) { MCC_TRACE("br\n");
			gen_op32(op);
		}
	}
	return 0;
}

#ifdef MCC_TARGET_X86_64
#define REX_W 0x48
#define REX_R 0x44
#define REX_X 0x42
#define REX_B 0x41

static void asm_rex(int width64, Operand *ops, int nb_ops, int *op_type,
										int regi, int rmi) { MCC_TRACE("enter\n");
	unsigned char rex = width64 ? 0x48 : 0;
	int saw_high_8bit = 0;
	if (rmi == -1) { MCC_TRACE("br\n");
		for (int i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
			if (op_type[i] & (OP_REG | OP_ST)) { MCC_TRACE("br\n");
				if (ops[i].reg >= 8) { MCC_TRACE("br\n");
					rex |= REX_B;
					ops[i].reg -= 8;
				} else if (ops[i].type & OP_REG8_LOW)
					{ MCC_TRACE("br\n"); rex |= 0x40; }
				else if (ops[i].type & OP_REG8 && ops[i].reg >= 4)
					{ MCC_TRACE("br\n"); saw_high_8bit = ops[i].reg; }
				break;
			}
		}
	} else { MCC_TRACE("br\n");
		if (regi != -1) { MCC_TRACE("br\n");
			if (ops[regi].reg >= 8) { MCC_TRACE("br\n");
				rex |= REX_R;
				ops[regi].reg -= 8;
			} else if (ops[regi].type & OP_REG8_LOW)
				{ MCC_TRACE("br\n"); rex |= 0x40; }
			else if (ops[regi].type & OP_REG8 && ops[regi].reg >= 4)
				{ MCC_TRACE("br\n"); saw_high_8bit = ops[regi].reg; }
		}
		if (ops[rmi].type & (OP_REG | OP_MMX | OP_SSE | OP_CR | OP_EA)) { MCC_TRACE("br\n");
			if (ops[rmi].reg >= 8) { MCC_TRACE("br\n");
				rex |= REX_B;
				ops[rmi].reg -= 8;
			} else if (ops[rmi].type & OP_REG8_LOW)
				{ MCC_TRACE("br\n"); rex |= 0x40; }
			else if (ops[rmi].type & OP_REG8 && ops[rmi].reg >= 4)
				{ MCC_TRACE("br\n"); saw_high_8bit = ops[rmi].reg; }
		}
		if (ops[rmi].type & OP_EA && ops[rmi].reg2 >= 8) { MCC_TRACE("br\n");
			rex |= REX_X;
			ops[rmi].reg2 -= 8;
		}
	}
	if (rex) { MCC_TRACE("br\n");
		if (saw_high_8bit)
			{ MCC_TRACE("br\n"); mcc_error("can't encode register %%%ch when REX prefix is required",
								"acdb"[saw_high_8bit - 4]); }
		g(rex);
	}
}
#endif

static void maybe_print_stats(void) { MCC_TRACE("enter\n");
	int already = 0;

	if (0 && !already) { MCC_TRACE("br\n");
		const struct ASMInstr *pa;
		int freq[4];
		int op_vals[500];
		int nb_op_vals, i, j;

		already = 1;
		nb_op_vals = 0;
		memset(freq, 0, sizeof(freq));
		for (pa = asm_instrs; pa->sym != 0; pa++) { MCC_TRACE("br\n");
			freq[pa->nb_ops]++;
			for (j = 0; j < nb_op_vals; j++) { MCC_TRACE("br\n");
				if (pa->instr_type == op_vals[j])
					{ MCC_TRACE("br\n"); goto found; }
			}
			op_vals[nb_op_vals++] = pa->instr_type;
		found:;
		}
		for (i = 0; i < nb_op_vals; i++) { MCC_TRACE("br\n");
			int v = op_vals[i];
			printf("%3d: %08x\n", i, v);
		}
		printf("size=%d nb=%d f0=%d f1=%d f2=%d f3=%d\n",
					 (int)sizeof(asm_instrs),
					 (int)sizeof(asm_instrs) / (int)sizeof(ASMInstr),
					 freq[0], freq[1], freq[2], freq[3]);
	}
}

ST_FUNC void asm_opcode(MCCState *s1, int opcode) { MCC_TRACE("enter\n");
	const ASMInstr *pa;
	int i, modrm_index, modreg_index, reg, v, op1, seg_prefix, pc, p;
	int nb_ops, s;
	Operand ops[MAX_OPERANDS], *pop;
	int op_type[3];
	int alltypes;
	int autosize;
	int p66;
#ifdef MCC_TARGET_X86_64
	int rex64;
#endif

	maybe_print_stats();
	if (opcode >= TOK_ASM_wait && opcode <= TOK_ASM_repnz)
		{ MCC_TRACE("br\n"); unget_tok(';'); }

	pop = ops;
	nb_ops = 0;
	seg_prefix = 0;
	alltypes = 0;
	for (;;) { MCC_TRACE("br\n");
		if (tok == ';' || tok == TOK_LINEFEED)
			{ MCC_TRACE("br\n"); break; }
		if (nb_ops >= MAX_OPERANDS) { MCC_TRACE("br\n");
			mcc_error("incorrect number of operands");
		}
		parse_operand(s1, pop);
		if (tok == ':') { MCC_TRACE("br\n");
			if (!(pop->type & OP_SEG) || seg_prefix)
				{ MCC_TRACE("br\n"); mcc_error("incorrect prefix"); }
			seg_prefix = segment_prefixes[pop->reg];
			next();
			parse_operand(s1, pop);
			if (!(pop->type & OP_EA)) { MCC_TRACE("br\n");
				mcc_error("segment prefix must be followed by memory reference");
			}
		}
		pop++;
		nb_ops++;
		if (tok != ',')
			{ MCC_TRACE("br\n"); break; }
		next();
	}

	s = 0;

again:
	for (pa = asm_instrs; pa->sym != 0; pa++) { MCC_TRACE("br\n");
		int it = pa->instr_type & OPCT_MASK;
		s = 0;
		if (it == OPC_FARITH) { MCC_TRACE("br\n");
			v = opcode - pa->sym;
			if (!((unsigned)v < 8 * 6 && (v % 6) == 0))
				{ MCC_TRACE("br\n"); continue; }
		} else if (it == OPC_ARITH) { MCC_TRACE("br\n");
			if (!(opcode >= pa->sym && opcode < pa->sym + 8 * NBWLX))
				{ MCC_TRACE("br\n"); continue; }
			s = (opcode - pa->sym) % NBWLX;
			if ((pa->instr_type & OPC_BWLX) == OPC_WLX) { MCC_TRACE("br\n");
				if (((opcode - pa->sym + 1) % NBWLX) == 0)
					{ MCC_TRACE("br\n"); continue; }
				s++;
			}
		} else if (it == OPC_SHIFT) { MCC_TRACE("br\n");
			if (!(opcode >= pa->sym && opcode < pa->sym + 7 * NBWLX))
				{ MCC_TRACE("br\n"); continue; }
			s = (opcode - pa->sym) % NBWLX;
		} else if (it == OPC_TEST) { MCC_TRACE("br\n");
			if (!(opcode >= pa->sym && opcode < pa->sym + NB_TEST_OPCODES))
				{ MCC_TRACE("br\n"); continue; }
			if (pa->instr_type & OPC_WLX)
				{ MCC_TRACE("br\n"); s = NBWLX - 1; }
		} else if (pa->instr_type & OPC_B) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_X86_64
			if ((pa->instr_type & OPC_WLQ) != OPC_WLQ && !(opcode >= pa->sym && opcode < pa->sym + NBWLX - 1))
				{ MCC_TRACE("br\n"); continue; }
#endif
			if (!(opcode >= pa->sym && opcode < pa->sym + NBWLX))
				{ MCC_TRACE("br\n"); continue; }
			s = opcode - pa->sym;
		} else if (pa->instr_type & OPC_WLX) { MCC_TRACE("br\n");
			if (!(opcode >= pa->sym && opcode < pa->sym + NBWLX - 1))
				{ MCC_TRACE("br\n"); continue; }
			s = opcode - pa->sym + 1;
		} else { MCC_TRACE("br\n");
			if (pa->sym != opcode)
				{ MCC_TRACE("br\n"); continue; }
		}
		if (pa->nb_ops != nb_ops)
			{ MCC_TRACE("br\n"); continue; }
#ifdef MCC_TARGET_X86_64
		if (pa->opcode == 0xb0 && ops[0].type != OP_IM64 && (ops[1].type & OP_REG) == OP_REG64 && !(pa->instr_type & OPC_0F))
			{ MCC_TRACE("br\n"); continue; }
#endif
		alltypes = 0;
		for (i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
			int op1, op2;
			op1 = pa->op_type[i];
			op2 = op1 & 0x1f;
			switch (op2) { MCC_TRACE("br\n");
			case OPT_IM:
				v = OP_IM8 | OP_IM16 | OP_IM32;
				break;
			case OPT_REG:
				v = OP_REG8 | OP_REG16 | OP_REG32 | OP_REG64;
				break;
			case OPT_REGW:
				v = OP_REG16 | OP_REG32 | OP_REG64;
				break;
			case OPT_IMW:
				v = OP_IM16 | OP_IM32;
				break;
			case OPT_MMXSSE:
				v = OP_MMX | OP_SSE;
				break;
			case OPT_DISP:
			case OPT_DISP8:
				v = OP_ADDR;
				break;
			default:
				v = 1 << op2;
				break;
			}
			if (op1 & OPT_EA)
				{ MCC_TRACE("br\n"); v |= OP_EA; }
			op_type[i] = v;
			if ((ops[i].type & v) == 0)
				{ MCC_TRACE("br\n"); goto next; }
			alltypes |= ops[i].type;
		}
#ifndef MCC_TARGET_X86_64

		if (!(pa->instr_type & OPC_MODRM)) { MCC_TRACE("br\n");
			for (i = 0; i < nb_ops; i++)
				if (ops[i].ntpoff)
					{ MCC_TRACE("br\n"); goto next; }
		}
#endif
		(void)alltypes;
		break;
	next:;
	}
	if (pa->sym == 0) { MCC_TRACE("br\n");
		if (opcode >= TOK_ASM_first && opcode <= TOK_ASM_last) { MCC_TRACE("br\n");
			int b;
			b = op0_codes[opcode - TOK_ASM_first];
			if (b & 0xff00)
				{ MCC_TRACE("br\n"); g(b >> 8); }
			g(b);
			return;
		} else if (opcode <= TOK_ASM_alllast) { MCC_TRACE("br\n");
			mcc_error("bad operand with opcode '%s'",
								get_tok_str(opcode, NULL));
		} else { MCC_TRACE("br\n");
			TokenSym *ts = table_ident[opcode - TOK_IDENT];
			if (ts->len >= 6 && strchr("wlq", ts->str[ts->len - 1]) && !memcmp(ts->str, "cmov", 4)) { MCC_TRACE("br\n");
				opcode = tok_alloc(ts->str, ts->len - 1)->tok;
				goto again;
			}
			mcc_error("unknown opcode '%s'", ts->str);
		}
	}
	autosize = NBWLX - 1;
#ifdef MCC_TARGET_X86_64
	if ((pa->instr_type & OPC_BWLQ) == OPC_B)
		{ MCC_TRACE("br\n"); autosize = NBWLX - 2; }
#endif
	if (s == autosize) { MCC_TRACE("br\n");
		for (i = nb_ops - 1; s == autosize && i >= 0; i--) { MCC_TRACE("br\n");
			if ((ops[i].type & OP_REG) && !(op_type[i] & (OP_CL | OP_DX)))
				{ MCC_TRACE("br\n"); s = reg_to_size[ops[i].type & OP_REG]; }
		}
		if (s == autosize) { MCC_TRACE("br\n");
			if ((opcode == TOK_ASM_push || opcode == TOK_ASM_pop) &&
					(ops[0].type & (OP_SEG | OP_IM8S | OP_IM32)))
				{ MCC_TRACE("br\n"); s = 2; }
			else if ((opcode == TOK_ASM_push || opcode == TOK_ASM_pop) &&
							 (ops[0].type & OP_EA))
				{ MCC_TRACE("br\n"); s = NBWLX - 2; }
			else
				{ MCC_TRACE("br\n"); mcc_error("cannot infer opcode suffix"); }
		}
	}

#ifdef MCC_TARGET_X86_64
	rex64 = 0;
	if (pa->instr_type & OPC_48)
		{ MCC_TRACE("br\n"); rex64 = 1; }
	else if (s == 3 || (alltypes & OP_REG64)) { MCC_TRACE("br\n");
		int default64 = 0;
		for (i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
			if (op_type[i] == OP_REG64 && pa->opcode != 0xb8) { MCC_TRACE("br\n");
				default64 = 1;
				break;
			}
		}
		if (((opcode != TOK_ASM_push && opcode != TOK_ASM_pop && opcode != TOK_ASM_pushw && opcode != TOK_ASM_pushl &&
					opcode != TOK_ASM_pushq && opcode != TOK_ASM_popw && opcode != TOK_ASM_popl && opcode != TOK_ASM_popq &&
					opcode != TOK_ASM_call && opcode != TOK_ASM_jmp)) &&
				!default64)
			{ MCC_TRACE("br\n"); rex64 = 1; }
	}
#endif

	if (OPCT_IS(pa->instr_type, OPC_FWAIT))
		{ MCC_TRACE("br\n"); g(0x9b); }
	if (seg_prefix)
		{ MCC_TRACE("br\n"); g(seg_prefix); }
#ifdef MCC_TARGET_X86_64
	for (i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
		if (ops[i].type & OP_EA32) { MCC_TRACE("br\n");
			g(0x67);
			break;
		}
	}
#endif
	p66 = 0;
	if (s == 1)
		{ MCC_TRACE("br\n"); p66 = 1; }
	else { MCC_TRACE("br\n");
		for (i = 0; i < nb_ops; i++)
			if ((op_type[i] & (OP_MMX | OP_SSE)) == (OP_MMX | OP_SSE) && ops[i].type & OP_SSE)
				{ MCC_TRACE("br\n"); p66 = 1; }
	}
	if (p66)
		{ MCC_TRACE("br\n"); g(0x66); }

	v = pa->opcode;
	p = v >> 8;
	switch (p) { MCC_TRACE("br\n");
	case 0:
		break;
	case 0x48:
		break;
	case 0x66:
	case 0x67:
	case 0xf2:
	case 0xf3:
		v = v & 0xff;
		g(p);
		break;
	case 0xd4:
	case 0xd5:
		break;
	case 0xd8:
	case 0xd9:
	case 0xda:
	case 0xdb:
	case 0xdc:
	case 0xdd:
	case 0xde:
	case 0xdf:
		break;
	default:
		mcc_error("bad prefix 0x%2x in opcode table", p);
		break;
	}
	if (pa->instr_type & OPC_0F)
		{ MCC_TRACE("br\n"); v = ((v & ~0xff) << 8) | 0x0f00 | (v & 0xff); }
	if ((v == 0x69 || v == 0x6b) && nb_ops == 2) { MCC_TRACE("br\n");
		nb_ops = 3;
		ops[2] = ops[1];
		op_type[2] = op_type[1];
	} else if (v == 0xcd && ops[0].e.v == 3 && !ops[0].e.sym) { MCC_TRACE("br\n");
		v--;
		nb_ops = 0;
	} else if ((v == 0x06 || v == 0x07)) { MCC_TRACE("br\n");
		if (ops[0].reg >= 4) { MCC_TRACE("br\n");
			v = 0x0fa0 + (v - 0x06) + ((ops[0].reg - 4) << 3);
		} else { MCC_TRACE("br\n");
			v += ops[0].reg << 3;
		}
		nb_ops = 0;
	} else if (v <= 0x05) { MCC_TRACE("br\n");
		v += ((opcode - TOK_ASM_addb) / NBWLX) << 3;
	} else if ((pa->instr_type & (OPCT_MASK | OPC_MODRM)) == OPC_FARITH) { MCC_TRACE("br\n");
		v += ((opcode - pa->sym) / 6) << 3;
	}

	modrm_index = -1;
	modreg_index = -1;
	if (pa->instr_type & OPC_MODRM) { MCC_TRACE("br\n");
		if (!nb_ops) { MCC_TRACE("br\n");
			i = 0;
			ops[i].type = OP_REG;
#ifdef MCC_TARGET_X86_64
			if (pa->sym == TOK_ASM_endbr64)
				{ MCC_TRACE("br\n"); ops[i].reg = 2; }
			else if (pa->sym >= TOK_ASM_lfence && pa->sym <= TOK_ASM_sfence)
				{ MCC_TRACE("br\n"); ops[i].reg = 0; }
#else
			if (pa->sym == TOK_ASM_endbr32)
				{ MCC_TRACE("br\n"); ops[i].reg = 3; }
#endif
			else
				{ MCC_TRACE("br\n"); mcc_error("bad MODR/M opcode without operands"); }
			goto modrm_found;
		}
		for (i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
			if (op_type[i] & OP_EA)
				{ MCC_TRACE("br\n"); goto modrm_found; }
		}
		for (i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
			if (op_type[i] & (OP_REG | OP_MMX | OP_SSE | OP_INDIR))
				{ MCC_TRACE("br\n"); goto modrm_found; }
		}
		mcc_error("bad op table");
	modrm_found:
		modrm_index = i;
		for (i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
			int t = op_type[i];
			if (i != modrm_index &&
					(t & (OP_REG | OP_MMX | OP_SSE | OP_CR | OP_TR | OP_DB | OP_SEG))) { MCC_TRACE("br\n");
				modreg_index = i;
				break;
			}
		}
	}
#ifdef MCC_TARGET_X86_64
	asm_rex(rex64, ops, nb_ops, op_type, modreg_index, modrm_index);
#endif

	if (pa->instr_type & OPC_REG) { MCC_TRACE("br\n");
		if (v == 0xb0 && s >= 1)
			{ MCC_TRACE("br\n"); v += 7; }
		for (i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
			if (op_type[i] & (OP_REG | OP_ST)) { MCC_TRACE("br\n");
				v += ops[i].reg;
				break;
			}
		}
	}
	if (pa->instr_type & OPC_B)
		{ MCC_TRACE("br\n"); v += s >= 1; }
	if (nb_ops == 1 && pa->op_type[0] == OPT_DISP8) { MCC_TRACE("br\n");
		ElfSym *esym;
		int jmp_disp;

		esym = elfsym(ops[0].e.sym);
		if (!esym || esym->st_shndx != cur_text_section->sh_num)
			{ MCC_TRACE("br\n"); goto no_short_jump; }
		jmp_disp = ops[0].e.v + esym->st_value - ind - 2 - (v >= 0xff);
		if (jmp_disp == (int8_t)jmp_disp) { MCC_TRACE("br\n");
			ops[0].e.sym = 0;
			ops[0].e.v = jmp_disp;
			op_type[0] = OP_IM8S;
		} else { MCC_TRACE("br\n");
		no_short_jump:
			if (v == 0xeb)
				{ MCC_TRACE("br\n"); v = 0xe9; }
			else if (v == 0x70)
				{ MCC_TRACE("br\n"); v += 0x0f10; }
			else
				{ MCC_TRACE("br\n"); mcc_error("invalid displacement"); }
		}
	}
	if (OPCT_IS(pa->instr_type, OPC_TEST))
		{ MCC_TRACE("br\n"); v += test_bits[opcode - pa->sym]; }
	else if (OPCT_IS(pa->instr_type, OPC_0F01))
		{ MCC_TRACE("br\n"); v |= 0x0f0100; }
	op1 = v >> 16;
	if (op1)
		{ MCC_TRACE("br\n"); g(op1); }
	op1 = (v >> 8) & 0xff;
	if (op1)
		{ MCC_TRACE("br\n"); g(op1); }
	g(v);

	if (OPCT_IS(pa->instr_type, OPC_SHIFT)) { MCC_TRACE("br\n");
		reg = (opcode - pa->sym) / NBWLX;
		if (reg == 6)
			{ MCC_TRACE("br\n"); reg = 7; }
	} else if (OPCT_IS(pa->instr_type, OPC_ARITH)) { MCC_TRACE("br\n");
		reg = (opcode - pa->sym) / NBWLX;
	} else if (OPCT_IS(pa->instr_type, OPC_FARITH)) { MCC_TRACE("br\n");
		reg = (opcode - pa->sym) / 6;
	} else { MCC_TRACE("br\n");
		reg = (pa->instr_type >> OPC_GROUP_SHIFT) & 7;
	}

	pc = 0;
	if (pa->instr_type & OPC_MODRM) { MCC_TRACE("br\n");
		if (modreg_index >= 0)
			{ MCC_TRACE("br\n"); reg = ops[modreg_index].reg; }
		pc = asm_modrm(reg, &ops[modrm_index]);
	}

#ifndef MCC_TARGET_X86_64
	if (!(pa->instr_type & OPC_0F) && (pa->opcode == 0x9a || pa->opcode == 0xea)) { MCC_TRACE("br\n");
		gen_expr32(&ops[1].e);
		if (ops[0].e.sym)
			{ MCC_TRACE("br\n"); mcc_error("cannot relocate"); }
		gen_le16(ops[0].e.v);
		return;
	}
#endif
	for (i = 0; i < nb_ops; i++) { MCC_TRACE("br\n");
		v = op_type[i];
		if (v & (OP_IM8 | OP_IM16 | OP_IM32 | OP_IM64 | OP_IM8S | OP_ADDR)) { MCC_TRACE("br\n");
			if ((v | OP_IM8 | OP_IM64) == (OP_IM8 | OP_IM16 | OP_IM32 | OP_IM64)) { MCC_TRACE("br\n");
				if (s == 0)
					{ MCC_TRACE("br\n"); v = OP_IM8; }
				else if (s == 1)
					{ MCC_TRACE("br\n"); v = OP_IM16; }
				else if (s == 2 || (v & OP_IM64) == 0)
					{ MCC_TRACE("br\n"); v = OP_IM32; }
				else
					{ MCC_TRACE("br\n"); v = OP_IM64; }
			}

			if ((v & (OP_IM8 | OP_IM8S | OP_IM16)) && ops[i].e.sym)
				{ MCC_TRACE("br\n"); mcc_error("cannot relocate"); }

			if (v & (OP_IM8 | OP_IM8S)) { MCC_TRACE("br\n");
				g(ops[i].e.v);
			} else if (v & OP_IM16) { MCC_TRACE("br\n");
				gen_le16(ops[i].e.v);
#ifdef MCC_TARGET_X86_64
			} else if (v & OP_IM64) { MCC_TRACE("br\n");
				gen_expr64(&ops[i].e);
#endif
			} else if (pa->op_type[i] == OPT_DISP || pa->op_type[i] == OPT_DISP8) { MCC_TRACE("br\n");
				gen_disp32(&ops[i].e);
			} else { MCC_TRACE("br\n");
				gen_op32(&ops[i]);
			}
		}
	}

	if (pc)
		{ MCC_TRACE("br\n"); add32le(cur_text_section->data + pc - 4, pc - ind); }
}

static inline int constraint_priority(const char *str) { MCC_TRACE("enter\n");
	int priority, c, pr;

	priority = 0;
	for (;;) { MCC_TRACE("br\n");
		c = *str;
		if (c == '\0')
			{ MCC_TRACE("br\n"); break; }
		str++;
		switch (c) { MCC_TRACE("br\n");
		case 'A':
			pr = 0;
			break;
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'S':
		case 'D':
			pr = 1;
			break;
		case 'q':
		case 'Q':
			pr = 2;
			break;
		case 'r':
		case 'R':
		case 'p':
			pr = 3;
			break;
		case 'N':
		case 'M':
		case 'I':
		case 'e':
		case 'i':
		case 'm':
		case 'g':
			pr = 4;
			break;
		default:
			mcc_error("unknown constraint '%c'", c);
			pr = 0;
		}
		if (pr > priority)
			{ MCC_TRACE("br\n"); priority = pr; }
	}
	return priority;
}

ST_FUNC int asm_parse_regvar(int t) { MCC_TRACE("enter\n");
	const char *s;
	Operand op;
	if (t < TOK_IDENT || (t & SYM_FIELD))
		{ MCC_TRACE("br\n"); return -1; }
	s = table_ident[t - TOK_IDENT]->str;
	if (s[0] == '%')
		{ MCC_TRACE("br\n"); ++s; }
	t = tok_alloc_const(s);
	unget_tok(t);
	unget_tok('%');
	parse_operand(mcc_state, &op);
	if (op.type & OP_REG)
		{ MCC_TRACE("br\n"); return op.reg; }
	else
		{ MCC_TRACE("br\n"); return -1; }
}

#define REG_OUT_MASK 0x01
#define REG_IN_MASK 0x02

#define is_reg_allocated(reg) (regs_allocated[reg] & reg_mask)

#include "arch/asm-constraints.inc.c"

ST_FUNC void asm_compute_constraints(ASMOperand *operands,
																		 int nb_operands, int nb_outputs,
																		 const uint8_t *clobber_regs,
																		 int *pout_reg) { MCC_TRACE("enter\n");
	ASMOperand *op;
	int sorted_op[MAX_ASM_OPERANDS];
	int j, reg, c, reg_mask;
	const char *str;
	uint8_t regs_allocated[MCC_NB_ASM_REGS];

	asm_constraints_prologue(operands, nb_operands, nb_outputs,
													 clobber_regs, sorted_op, regs_allocated);
	regs_allocated[4] = REG_IN_MASK | REG_OUT_MASK;
	regs_allocated[5] = REG_IN_MASK | REG_OUT_MASK;

	for (int i = 0; i < nb_operands; i++) { MCC_TRACE("br\n");
		j = sorted_op[i];
		op = &operands[j];
		str = op->constraint;
		if (op->ref_index >= 0)
			{ MCC_TRACE("br\n"); continue; }
		if (op->input_index >= 0) { MCC_TRACE("br\n");
			reg_mask = REG_IN_MASK | REG_OUT_MASK;
		} else if (j < nb_outputs) { MCC_TRACE("br\n");
			reg_mask = REG_OUT_MASK;
		} else { MCC_TRACE("br\n");
			reg_mask = REG_IN_MASK;
		}
		if (op->reg >= 0) { MCC_TRACE("br\n");
			if (is_reg_allocated(op->reg))
				{ MCC_TRACE("br\n"); mcc_error("asm regvar requests register that's taken already"); }
			reg = op->reg;
		}
	try_next:
		c = *str++;
		switch (c) { MCC_TRACE("br\n");
		case '=':
			goto try_next;
		case '+':
			op->is_rw = 1;
			FALLTHROUGH;
		case '&':
			if (j >= nb_outputs)
				{ MCC_TRACE("br\n"); mcc_error("'%c' modifier can only be applied to outputs", c); }
			reg_mask = REG_IN_MASK | REG_OUT_MASK;
			goto try_next;
		case 'A':
#ifdef MCC_TARGET_X86_64
			if (is_reg_allocated(MCC_TREG_XAX))
				{ MCC_TRACE("br\n"); goto try_next; }
			op->is_llong = 0;
			op->reg = MCC_TREG_XAX;
			regs_allocated[MCC_TREG_XAX] |= reg_mask;
#else
			if (is_reg_allocated(MCC_TREG_XAX) ||
					is_reg_allocated(MCC_TREG_XDX))
				{ MCC_TRACE("br\n"); goto try_next; }
			op->is_llong = 1;
			op->reg = MCC_TREG_XAX;
			regs_allocated[MCC_TREG_XAX] |= reg_mask;
			regs_allocated[MCC_TREG_XDX] |= reg_mask;
#endif
			break;
		case 'a':
			reg = MCC_TREG_XAX;
			goto alloc_reg;
		case 'b':
			reg = 3;
			goto alloc_reg;
		case 'c':
			reg = MCC_TREG_XCX;
			goto alloc_reg;
		case 'd':
			reg = MCC_TREG_XDX;
			goto alloc_reg;
		case 'S':
			reg = 6;
			goto alloc_reg;
		case 'D':
			reg = 7;
		alloc_reg:
			if (op->reg >= 0 && reg != op->reg)
				{ MCC_TRACE("br\n"); goto try_next; }
			if (is_reg_allocated(reg))
				{ MCC_TRACE("br\n"); goto try_next; }
			goto reg_found;
		case 'q':
		case 'Q':
			if (op->reg >= 0) { MCC_TRACE("br\n");
				if ((reg = op->reg) < 4)
					{ MCC_TRACE("br\n"); goto reg_found; }
			} else
				{ MCC_TRACE("br\n"); for (reg = 0; reg < 4; reg++) { MCC_TRACE("br\n");
					if (!is_reg_allocated(reg))
						{ MCC_TRACE("br\n"); goto reg_found; }
				} }
			goto try_next;
		case 'r':
		case 'R':
		case 'p':
			if ((reg = op->reg) >= 0)
				{ MCC_TRACE("br\n"); goto reg_found; }
			else
				{ MCC_TRACE("br\n"); for (reg = 0; reg < MCC_NB_ASM_REGS; reg++) { MCC_TRACE("br\n");
					if (!is_reg_allocated(reg))
						{ MCC_TRACE("br\n"); goto reg_found; }
				} }
			goto try_next;
		reg_found:
			op->is_llong = 0;
			op->reg = reg;
			regs_allocated[reg] |= reg_mask;
			break;
		case 'e':
		case 'i':
			if (!((op->vt->r & (VT_VALMASK | VT_LVAL)) == VT_CONST))
				{ MCC_TRACE("br\n"); goto try_next; }
			break;
		case 'I':
		case 'N':
		case 'M':
			if (!((op->vt->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST))
				{ MCC_TRACE("br\n"); goto try_next; }
			break;
		case 'm':
		case 'g':
			if (j < nb_outputs || c == 'm') { MCC_TRACE("br\n");
				if ((op->vt->r & VT_VALMASK) == VT_LLOCAL) { MCC_TRACE("br\n");
					for (reg = 0; reg < MCC_NB_ASM_REGS; reg++) { MCC_TRACE("br\n");
						if (!(regs_allocated[reg] & REG_IN_MASK))
							{ MCC_TRACE("br\n"); goto reg_found1; }
					}
					goto try_next;
				reg_found1:
					regs_allocated[reg] |= REG_IN_MASK;
					op->reg = reg;
					op->is_memory = 1;
				}
			}
			break;
		default:
			mcc_error("asm constraint %d ('%s') could not be satisfied",
								j, op->constraint);
			break;
		}
		if (op->input_index >= 0) { MCC_TRACE("br\n");
			operands[op->input_index].reg = op->reg;
			operands[op->input_index].is_llong = op->is_llong;
		}
	}

	*pout_reg = -1;
	for (int i = 0; i < nb_operands; i++) { MCC_TRACE("br\n");
		op = &operands[i];
		if (op->reg >= 0 &&
				(op->vt->r & VT_VALMASK) == VT_LLOCAL &&
				!op->is_memory) { MCC_TRACE("br\n");
			for (reg = 0; reg < MCC_NB_ASM_REGS; reg++) { MCC_TRACE("br\n");
				if (!(regs_allocated[reg] & REG_OUT_MASK))
					{ MCC_TRACE("br\n"); goto reg_found2; }
			}
			mcc_error("could not find free output register for reloading");
		reg_found2:
			*pout_reg = reg;
			break;
		}
	}

	if (g_debug & MCC_DBG_ASM) { MCC_TRACE("br\n");
		for (int i = 0; i < nb_operands; i++) { MCC_TRACE("br\n");
			j = sorted_op[i];
			op = &operands[j];
			printf("%%%d [%s]: \"%s\" r=0x%04x reg=%d\n",
						 j,
						 op->id ? get_tok_str(op->id, NULL) : "",
						 op->constraint,
						 op->vt->r,
						 op->reg);
		}
		if (*pout_reg >= 0)
			{ MCC_TRACE("br\n"); printf("out_reg=%d\n", *pout_reg); }
	}
}

ST_FUNC void subst_asm_operand(CString *add_str,
															 SValue *sv, int modifier) { MCC_TRACE("enter\n");
	int r, reg, size, val;

	r = sv->r;
	if ((r & VT_VALMASK) == VT_CONST) { MCC_TRACE("br\n");
		if (!(r & VT_LVAL) && modifier != 'c' && modifier != 'n' &&
				modifier != 'P')
			{ MCC_TRACE("br\n"); cstr_ccat(add_str, '$'); }
		if (r & VT_SYM) { MCC_TRACE("br\n");
			const char *name = get_tok_str(sv->sym->v, NULL);
			if (sv->sym->v >= SYM_FIRST_ANOM) { MCC_TRACE("br\n");
				get_asm_sym(tok_alloc_const(name), sv->sym);
			}
			if (mcc_state->leading_underscore)
				{ MCC_TRACE("br\n"); cstr_ccat(add_str, '_'); }
			cstr_cat(add_str, name, -1);
			if ((uint32_t)sv->c.i == 0)
				{ MCC_TRACE("br\n"); goto no_offset; }
			cstr_ccat(add_str, '+');
		}
		val = sv->c.i;
		if (modifier == 'n')
			{ MCC_TRACE("br\n"); val = -val; }
		cstr_printf(add_str, "%d", (int)sv->c.i);
	no_offset:;
#ifdef MCC_TARGET_X86_64
		if (r & VT_LVAL)
			{ MCC_TRACE("br\n"); cstr_cat(add_str, "(%rip)", -1); }
#endif
	} else if ((r & VT_VALMASK) == VT_LOCAL) { MCC_TRACE("br\n");
		cstr_printf(add_str, "%d(%%%s)", (int)sv->c.i, get_tok_str(TOK_ASM_xax + 5, NULL));
	} else if (r & VT_LVAL) { MCC_TRACE("br\n");
		reg = r & VT_VALMASK;
		if (reg >= VT_CONST)
			{ MCC_TRACE("br\n"); mcc_internal_error(""); }
		cstr_printf(add_str, "(%%%s)", get_tok_str(TOK_ASM_xax + reg, NULL));
	} else { MCC_TRACE("br\n");
		reg = r & VT_VALMASK;
		if (reg >= VT_CONST)
			{ MCC_TRACE("br\n"); mcc_internal_error(""); }

		if ((sv->type.t & VT_BTYPE) == VT_BYTE ||
				(sv->type.t & VT_BTYPE) == VT_BOOL)
			{ MCC_TRACE("br\n"); size = 1; }
		else if ((sv->type.t & VT_BTYPE) == VT_SHORT)
			{ MCC_TRACE("br\n"); size = 2; }
#ifdef MCC_TARGET_X86_64
		else if ((sv->type.t & VT_BTYPE) == VT_LLONG ||
						 (sv->type.t & VT_BTYPE) == VT_PTR)
			{ MCC_TRACE("br\n"); size = 8; }
#endif
		else
			{ MCC_TRACE("br\n"); size = 4; }
		if (size == 1 && reg >= 4)
			{ MCC_TRACE("br\n"); size = 4; }

		if (modifier == 'b') { MCC_TRACE("br\n");
			if (reg >= 4)
				{ MCC_TRACE("br\n"); mcc_error("cannot use byte register"); }
			size = 1;
		} else if (modifier == 'h') { MCC_TRACE("br\n");
			if (reg >= 4)
				{ MCC_TRACE("br\n"); mcc_error("cannot use byte register"); }
			size = -1;
		} else if (modifier == 'w') { MCC_TRACE("br\n");
			size = 2;
		} else if (modifier == 'k') { MCC_TRACE("br\n");
			size = 4;
#ifdef MCC_TARGET_X86_64
		} else if (modifier == 'q') { MCC_TRACE("br\n");
			size = 8;
#endif
		}

		if (reg >= 8) { MCC_TRACE("br\n");
			cstr_printf(add_str, "%%r%d%c", reg, (size == 1) ? 'b' : ((size == 2) ? 'w' : ((size == 4) ? 'd' : ' ')));
			return;
		}
		switch (size) { MCC_TRACE("br\n");
		case -1:
			reg = TOK_ASM_ah + reg;
			break;
		case 1:
			reg = TOK_ASM_al + reg;
			break;
		case 2:
			reg = TOK_ASM_ax + reg;
			break;
		default:
			reg = TOK_ASM_eax + reg;
			break;
#ifdef MCC_TARGET_X86_64
		case 8:
			reg = TOK_ASM_rax + reg;
			break;
#endif
		}
		cstr_printf(add_str, "%%%s", get_tok_str(reg, NULL));
	}
}

ST_FUNC void asm_gen_code(ASMOperand *operands, int nb_operands,
													int nb_outputs, int is_output,
													uint8_t *clobber_regs,
													int out_reg) { MCC_TRACE("enter\n");
	uint8_t regs_allocated[MCC_NB_ASM_REGS];
	ASMOperand *op;
	int reg;

#ifdef MCC_TARGET_X86_64
#ifdef MCC_TARGET_PE
	static const uint8_t reg_saved[] = {3, 6, 7, 12, 13, 14, 15};
#else
	static const uint8_t reg_saved[] = {3, 12, 13, 14, 15};
#endif
#else
	static const uint8_t reg_saved[] = {3, 6, 7};
#endif

	memcpy(regs_allocated, clobber_regs, sizeof(regs_allocated));
	for (int i = 0; i < nb_operands; i++) { MCC_TRACE("br\n");
		op = &operands[i];
		if (op->reg >= 0)
			{ MCC_TRACE("br\n"); regs_allocated[op->reg] = 1; }
	}
	if (!is_output) { MCC_TRACE("br\n");
		for (int i = 0; i < sizeof(reg_saved) / sizeof(reg_saved[0]); i++) { MCC_TRACE("br\n");
			reg = reg_saved[i];
			if (regs_allocated[reg]) { MCC_TRACE("br\n");
				if (reg >= 8)
					{ MCC_TRACE("br\n"); g(0x41), reg -= 8; }
				g(0x50 + reg);
			}
		}

		for (int i = 0; i < nb_operands; i++) { MCC_TRACE("br\n");
			op = &operands[i];
			if (op->reg >= 0) { MCC_TRACE("br\n");
				if ((op->vt->r & VT_VALMASK) == VT_LLOCAL &&
						op->is_memory) { MCC_TRACE("br\n");
					SValue sv;
					sv = *op->vt;
					sv.r = (sv.r & ~VT_VALMASK) | VT_LOCAL | VT_LVAL;
					sv.type.t = VT_PTR;
					load(op->reg, &sv);
				} else if (i >= nb_outputs || op->is_rw) { MCC_TRACE("br\n");
					load(op->reg, op->vt);
					if (op->is_llong) { MCC_TRACE("br\n");
						SValue sv;
						sv = *op->vt;
						sv.c.i += 4;
						load(MCC_TREG_XDX, &sv);
					}
				}
			}
		}
	} else { MCC_TRACE("br\n");
		for (int i = 0; i < nb_outputs; i++) { MCC_TRACE("br\n");
			op = &operands[i];
			if (op->reg >= 0) { MCC_TRACE("br\n");
				if ((op->vt->r & VT_VALMASK) == VT_LLOCAL) { MCC_TRACE("br\n");
					if (!op->is_memory) { MCC_TRACE("br\n");
						SValue sv;
						sv = *op->vt;
						sv.r = (sv.r & ~VT_VALMASK) | VT_LOCAL;
						sv.type.t = VT_PTR;
						load(out_reg, &sv);

						sv = *op->vt;
						sv.r = (sv.r & ~VT_VALMASK) | out_reg;
						store(op->reg, &sv);
					}
				} else { MCC_TRACE("br\n");
					store(op->reg, op->vt);
					if (op->is_llong) { MCC_TRACE("br\n");
						SValue sv;
						sv = *op->vt;
						sv.c.i += 4;
						store(MCC_TREG_XDX, &sv);
					}
				}
			}
		}
		for (int i = sizeof(reg_saved) / sizeof(reg_saved[0]) - 1; i >= 0; i--) { MCC_TRACE("br\n");
			reg = reg_saved[i];
			if (regs_allocated[reg]) { MCC_TRACE("br\n");
				if (reg >= 8)
					{ MCC_TRACE("br\n"); g(0x41), reg -= 8; }
				g(0x58 + reg);
			}
		}
	}
}

ST_FUNC void asm_clobber(uint8_t *clobber_regs, const char *str) { MCC_TRACE("enter\n");
	int reg;
#ifdef MCC_TARGET_X86_64
	unsigned int type;
#endif

	if (!strcmp(str, "memory") ||
			!strcmp(str, "cc") ||
			!strcmp(str, "flags"))
		{ MCC_TRACE("br\n"); return; }
	reg = tok_alloc_const(str);
	if (reg >= TOK_ASM_eax && reg <= TOK_ASM_edi) { MCC_TRACE("br\n");
		reg -= TOK_ASM_eax;
	} else if (reg >= TOK_ASM_ax && reg <= TOK_ASM_di) { MCC_TRACE("br\n");
		reg -= TOK_ASM_ax;
#ifdef MCC_TARGET_X86_64
	} else if (reg >= TOK_ASM_rax && reg <= TOK_ASM_rdi) { MCC_TRACE("br\n");
		reg -= TOK_ASM_rax;
	} else if ((reg = asm_parse_numeric_reg(reg, &type)) >= 0) { MCC_TRACE("br\n");
		;
#endif
	} else { MCC_TRACE("br\n");
		mcc_error("invalid clobber register '%s'", str);
	}
	clobber_regs[reg] = 1;
}

#endif
