#define USING_GLOBALS
#include "mcc.h"

ST_DATA const char *const target_machine_defs =
		"__arm__\0"
		"__arm\0"
		"arm\0"
		"__arm_elf__\0"
		"__arm_elf\0"
		"arm_elf\0"
		"__ARM_ARCH_4__\0"
		"__ARMEL__\0"
		"__APCS_32__\0"
#if defined MCC_ARM_EABI
		"__ARM_EABI__\0"
#endif
		;

ST_DATA const int reg_classes[MCC_NB_REGS] = {
		MCC_RC_INT | MCC_RC_R0,
		MCC_RC_INT | MCC_RC_R1,
		MCC_RC_INT | MCC_RC_R2,
		MCC_RC_INT | MCC_RC_R3,
		MCC_RC_INT | MCC_RC_R12,
		MCC_RC_FLOAT | MCC_RC_F0,
		MCC_RC_FLOAT | MCC_RC_F1,
		MCC_RC_FLOAT | MCC_RC_F2,
		MCC_RC_FLOAT | MCC_RC_F3,
#ifdef MCC_ARM_VFP
		MCC_RC_FLOAT | MCC_RC_F4,
		MCC_RC_FLOAT | MCC_RC_F5,
		MCC_RC_FLOAT | MCC_RC_F6,
		MCC_RC_FLOAT | MCC_RC_F7,
#endif
};

#define cg_float_abi (mcc_state->cg_arm_float_abi)
#define func_sub_sp_offset (mcc_state->cg_func_sub_sp_offset)
#define last_itod_magic (mcc_state->cg_last_itod_magic)
#define leaffunc (mcc_state->cg_leaffunc)

#if MCC_CONFIG_BCHECK
#define func_bound_offset (mcc_state->cg_func_bound_offset)
#define func_bound_ind (mcc_state->cg_func_bound_ind)
ST_DATA int func_bound_add_epilog;
#endif

#if defined(MCC_ARM_EABI) && defined(MCC_ARM_VFP)
#define float_type (mcc_state->cg_float_type)
#define double_type (mcc_state->cg_double_type)
#define func_float_type (mcc_state->cg_func_float_type)
#define func_double_type (mcc_state->cg_func_double_type)
ST_FUNC void arm_init(struct MCCState *s) {
	float_type.t = VT_FLOAT;
	double_type.t = VT_DOUBLE;
	func_float_type.t = VT_FUNC;
	func_float_type.ref = sym_push(SYM_FIELD, &float_type, FUNC_CDECL, FUNC_OLD);
	func_double_type.t = VT_FUNC;
	func_double_type.ref = sym_push(SYM_FIELD, &double_type, FUNC_CDECL, FUNC_OLD);

	cg_float_abi = s->float_abi;
#ifndef MCC_ARM_HARDFLOAT
#endif
}
#else
#define func_float_type func_old_type
#define func_double_type func_old_type
#define func_ldouble_type func_old_type
ST_FUNC void arm_init(struct MCCState *s) {
}
#endif

#define CHECK_R(r) ((r) >= MCC_TREG_R0 && (r) <= MCC_TREG_LR)

static int two2mask(int a, int b) {
	if (!CHECK_R(a) || !CHECK_R(b))
		mcc_error("compiler error! registers %i,%i is not valid", a, b);
	return (reg_classes[a] | reg_classes[b]) & ~(MCC_RC_INT | MCC_RC_FLOAT);
}

static int regmask(int r) {
	if (!CHECK_R(r))
		mcc_error("compiler error! register %i is not valid", r);
	return reg_classes[r] & ~(MCC_RC_INT | MCC_RC_FLOAT);
}

void o(uint32_t i) {
	int ind1;
	if (nocode_wanted)
		return;
	ind1 = ind + 4;
	if (!cur_text_section)
		mcc_error("compiler error! This happens f.ex. if the compiler\n"
							"can't evaluate constant expressions outside of a function.");
	if (ind1 > cur_text_section->data_allocated)
		section_realloc(cur_text_section, ind1);
	cur_text_section->data[ind++] = i & 255;
	i >>= 8;
	cur_text_section->data[ind++] = i & 255;
	i >>= 8;
	cur_text_section->data[ind++] = i & 255;
	i >>= 8;
	cur_text_section->data[ind++] = i;
}

static uint32_t stuff_const(uint32_t op, uint32_t c) {
	int try_neg = 0;
	uint32_t nc = 0, negop = 0;

	switch (op & 0x1F00000) {
	case 0x800000:
	case 0x400000:
		try_neg = 1;
		negop = op ^ 0xC00000;
		nc = -c;
		break;
	case 0x1A00000:
	case 0x1E00000:
		try_neg = 1;
		negop = op ^ 0x400000;
		nc = ~c;
		break;
	case 0x200000:
		if (c == ~0)
			return (op & 0xF010F000) | ((op >> 16) & 0xF) | 0x1E00000;
		break;
	case 0x0:
		if (c == ~0)
			return (op & 0xF010F000) | ((op >> 16) & 0xF) | 0x1A00000;
	case 0x1C00000:
		try_neg = 1;
		negop = op ^ 0x1C00000;
		nc = ~c;
		break;
	case 0x1800000:
		if (c == ~0)
			return (op & 0xFFF0FFFF) | 0x1E00000;
		break;
	}
	do {
		uint32_t m;
		if (c < 256)
			return op | c;
		for (int i = 2; i < 32; i += 2) {
			m = (0xffu >> i) | (0xffu << (32 - i));
			if (!(c & ~m))
				return op | (i << 7) | (c << i) | (c >> (32 - i));
		}
		op = negop;
		c = nc;
	} while (try_neg--);
	return 0;
}

void stuff_const_harder(uint32_t op, uint32_t v) {
	uint32_t x;
	x = stuff_const(op, v);
	if (x)
		o(x);
	else {
		uint32_t a[16], nv, no, o2, n2;
		a[0] = 0xff;
		o2 = (op & 0xfff0ffff) | ((op & 0xf000) << 4);
		;
		for (int i = 1; i < 16; i++)
			a[i] = (a[i - 1] >> 2) | (a[i - 1] << 30);
		for (int i = 0; i < 12; i++)
			for (int j = i < 4 ? i + 12 : 15; j >= i + 4; j--)
				if ((v & (a[i] | a[j])) == v) {
					o(stuff_const(op, v & a[i]));
					o(stuff_const(o2, v & a[j]));
					return;
				}
		no = op ^ 0xC00000;
		n2 = o2 ^ 0xC00000;
		nv = -v;
		for (int i = 0; i < 12; i++)
			for (int j = i < 4 ? i + 12 : 15; j >= i + 4; j--)
				if ((nv & (a[i] | a[j])) == nv) {
					o(stuff_const(no, nv & a[i]));
					o(stuff_const(n2, nv & a[j]));
					return;
				}
		for (int i = 0; i < 8; i++)
			for (int j = i + 4; j < 12; j++)
				for (int k = i < 4 ? i + 12 : 15; k >= j + 4; k--)
					if ((v & (a[i] | a[j] | a[k])) == v) {
						o(stuff_const(op, v & a[i]));
						o(stuff_const(o2, v & a[j]));
						o(stuff_const(o2, v & a[k]));
						return;
					}
		no = op ^ 0xC00000;
		nv = -v;
		for (int i = 0; i < 8; i++)
			for (int j = i + 4; j < 12; j++)
				for (int k = i < 4 ? i + 12 : 15; k >= j + 4; k--)
					if ((nv & (a[i] | a[j] | a[k])) == nv) {
						o(stuff_const(no, nv & a[i]));
						o(stuff_const(n2, nv & a[j]));
						o(stuff_const(n2, nv & a[k]));
						return;
					}
		o(stuff_const(op, v & a[0]));
		o(stuff_const(o2, v & a[4]));
		o(stuff_const(o2, v & a[8]));
		o(stuff_const(o2, v & a[12]));
	}
}

uint32_t encbranch(int pos, int addr, int fail) {
	addr -= pos + 8;
	addr /= 4;
	if (addr >= 0x1000000 || addr < -0x1000000) {
		if (fail)
			mcc_error("branch target out of range (ARM B/BL reach is +/-32MB); veneers not implemented");
		return 0;
	}
	return 0x0A000000 | (addr & 0xffffff);
}

int decbranch(int pos) {
	int x;
	x = *(uint32_t *)(cur_text_section->data + pos);
	x &= 0x00ffffff;
	if (x & 0x800000)
		x -= 0x1000000;
	return x * 4 + pos + 8;
}

void gsym_addr(int t, int a) {
	uint32_t *x;
	int lt;
	while (t) {
		x = (uint32_t *)(cur_text_section->data + t);
		t = decbranch(lt = t);
		if (a == lt + 4)
			*x = 0xE1A00000;
		else {
			*x &= 0xff000000;
			*x |= encbranch(lt, a, 1);
		}
	}
}

#ifdef MCC_ARM_VFP
static uint32_t vfpr(int r) {
	if (r < MCC_TREG_F0 || r > MCC_TREG_F7)
		mcc_error("compiler error! register %i is no vfp register", r);
	return r - MCC_TREG_F0;
}
#else
static uint32_t fpr(int r) {
	if (r < MCC_TREG_F0 || r > MCC_TREG_F3)
		mcc_error("compiler error! register %i is no fpa register", r);
	return r - MCC_TREG_F0;
}
#endif

static uint32_t intr(int r) {
	if (r == MCC_TREG_R12)
		return 12;
	if (r >= MCC_TREG_R0 && r <= MCC_TREG_R3)
		return r - MCC_TREG_R0;
	if (!(r >= MCC_TREG_SP && r <= MCC_TREG_LR))
		mcc_error("compiler error! register %i is no int register", r);
	return r + (13 - MCC_TREG_SP);
}

static void calcaddr(uint32_t *base, int *off, int *sgn, int maxoff, unsigned shift) {
	if (*off > maxoff || *off & ((1 << shift) - 1)) {
		uint32_t x, y;
		x = 0xE280E000;
		if (*sgn)
			x = 0xE240E000;
		x |= (*base) << 16;
		*base = 14;
		y = stuff_const(x, *off & ~maxoff);
		if (y) {
			o(y);
			*off &= maxoff;
			return;
		}
		y = stuff_const(x, (*off + maxoff) & ~maxoff);
		if (y) {
			o(y);
			*sgn = !*sgn;
			*off = ((*off + maxoff) & ~maxoff) - *off;
			return;
		}
		stuff_const_harder(x, *off & ~maxoff);
		*off &= maxoff;
	}
}

static uint32_t mapcc(int cc) {
	switch (cc) {
	case TOK_ULT:
		return 0x30000000;
	case TOK_UGE:
		return 0x20000000;
	case TOK_EQ:
		return 0x00000000;
	case TOK_NE:
		return 0x10000000;
	case TOK_ULE:
		return 0x90000000;
	case TOK_UGT:
		return 0x80000000;
	case TOK_Nset:
		return 0x40000000;
	case TOK_Nclear:
		return 0x50000000;
	case TOK_LT:
		return 0xB0000000;
	case TOK_GE:
		return 0xA0000000;
	case TOK_LE:
		return 0xD0000000;
	case TOK_GT:
		return 0xC0000000;
	}
	mcc_error("unexpected condition code");
	return 0xE0000000;
}

static int negcc(int cc) {
	switch (cc) {
	case TOK_ULT:
		return TOK_UGE;
	case TOK_UGE:
		return TOK_ULT;
	case TOK_EQ:
		return TOK_NE;
	case TOK_NE:
		return TOK_EQ;
	case TOK_ULE:
		return TOK_UGT;
	case TOK_UGT:
		return TOK_ULE;
	case TOK_Nset:
		return TOK_Nclear;
	case TOK_Nclear:
		return TOK_Nset;
	case TOK_LT:
		return TOK_GE;
	case TOK_GE:
		return TOK_LT;
	case TOK_LE:
		return TOK_GT;
	case TOK_GT:
		return TOK_LE;
	}
	mcc_error("unexpected condition code");
	return TOK_NE;
}

static void load_value(SValue *sv, int r) {
#if MCC_CONFIG_CPUVER >= 7
	if (!(sv->r & VT_SYM)) {
		unsigned x = sv->c.i;
		o(0xE3000000 | intr(r) << 12 | (x & 0xFFF) | (x << 4 & 0xF0000));
		if (x & 0xFFFF0000)
			o(0xE3400000 | intr(r) << 12 | (x >> 16 & 0xFFF) | (x >> 12 & 0xF0000));
		return;
	}
#endif
	o(0xE59F0000 | (intr(r) << 12));
	o(0xEA000000);
	if (!mcc_state->pic) {
		if (sv->r & VT_SYM)
			greloc(cur_text_section, sv->sym, ind, R_ARM_ABS32);
		o(sv->c.i);
	} else {
		if (sv->r & VT_SYM) {
			if (sv->sym->type.t & VT_STATIC) {
				greloc(cur_text_section, sv->sym, ind, R_ARM_REL32);
				o(sv->c.i - 12);
				o(0xe080000f | (intr(r) << 12) | (intr(r) << 16));
			} else {
				greloc(cur_text_section, sv->sym, ind, R_ARM_GOT_PREL);
				o(-12);
				o(0xe080000f | (intr(r) << 12) | (intr(r) << 16));
				o(0xe5900000 | (intr(r) << 12) | (intr(r) << 16));
				if (sv->c.i)
					stuff_const_harder(0xe2800000 | (intr(r) << 12) | (intr(r) << 16),
														 sv->c.i);
			}
		} else
			o(sv->c.i);
	}
}

static void arm_tls_addr(Sym *sym, int coff) {
	o(0xe52dc004);
	o(0xee1def70);
	o(0xe59fc000);
	o(0xea000000);
	greloca(cur_text_section, sym, ind, R_ARM_TLS_LE32, 0);
	o(coff);
	o(0xe08ee00c);
	o(0xe49dc004);
}

void load(int r, SValue *sv) {
	int v, ft, fc, fr, sign;
	uint32_t op, base;
	SValue v1;

	fr = sv->r;
	ft = sv->type.t;
	fc = sv->c.i;

	if (fc >= 0)
		sign = 0;
	else {
		sign = 1;
		fc = -(unsigned)fc;
	}

	v = fr & VT_VALMASK;
	if (fr & VT_LVAL) {
		base = 0xB;
		if ((fr & VT_SYM) && sv->sym->type.t & VT_TLS) {
			arm_tls_addr(sv->sym, sv->c.i);
			base = 14;
			fc = sign = 0;
			v = VT_LOCAL;
		} else if (v == VT_LLOCAL) {
			v1.type.t = VT_PTR;
			v1.r = VT_LOCAL | VT_LVAL;
			v1.c.i = sv->c.i;
			load(MCC_TREG_LR, &v1);
			base = 14;
			fc = sign = 0;
			v = VT_LOCAL;
		} else if (v == VT_CONST) {
			v1.type.t = VT_PTR;
			v1.r = fr & ~VT_LVAL;
			v1.c.i = sv->c.i;
			v1.sym = sv->sym;
			load(MCC_TREG_LR, &v1);
			base = 14;
			fc = sign = 0;
			v = VT_LOCAL;
		} else if (v < VT_CONST) {
			base = intr(v);
			fc = sign = 0;
			v = VT_LOCAL;
		}
		if (v == VT_LOCAL) {
			if (is_float(ft)) {
				calcaddr(&base, &fc, &sign, 1020, 2);
#ifdef MCC_ARM_VFP
				op = 0xED100A00;
				if (!sign)
					op |= 0x800000;
				if ((ft & VT_BTYPE) != VT_FLOAT)
					op |= 0x100;
				o(op | (vfpr(r) << 12) | (fc >> 2) | (base << 16));
#else
				op = 0xED100100;
				if (!sign)
					op |= 0x800000;
				if ((ft & VT_BTYPE) == VT_DOUBLE)
					op |= 0x8000;
#if MCC_LDOUBLE_SIZE != 8
				else if ((ft & VT_BTYPE) == VT_LDOUBLE)
					op |= 0x400000;
#endif
				o(op | (fpr(r) << 12) | (fc >> 2) | (base << 16));
#endif
			} else if ((ft & (VT_BTYPE | VT_UNSIGNED)) == VT_BYTE || (ft & VT_BTYPE) == VT_SHORT) {
				calcaddr(&base, &fc, &sign, 255, 0);
				op = 0xE1500090;
				if ((ft & VT_BTYPE) == VT_SHORT)
					op |= 0x20;
				if ((ft & VT_UNSIGNED) == 0)
					op |= 0x40;
				if (!sign)
					op |= 0x800000;
				o(op | (intr(r) << 12) | (base << 16) | ((fc & 0xf0) << 4) | (fc & 0xf));
			} else {
				calcaddr(&base, &fc, &sign, 4095, 0);
				op = 0xE5100000;
				if (!sign)
					op |= 0x800000;
				if ((ft & VT_BTYPE) == VT_BYTE || (ft & VT_BTYPE) == VT_BOOL)
					op |= 0x400000;
				o(op | (intr(r) << 12) | fc | (base << 16));
			}
			return;
		}
	} else {
		if (v == VT_CONST) {
			if ((fr & VT_SYM) && sv->sym->type.t & VT_TLS) {
				arm_tls_addr(sv->sym, sv->c.i);
				o(0xe1a0000e | (intr(r) << 12));
				return;
			}
			op = stuff_const(0xE3A00000 | (intr(r) << 12), sv->c.i);
			if (fr & VT_SYM || !op)
				load_value(sv, r);
			else
				o(op);
			return;
		} else if (v == VT_LOCAL) {
			op = stuff_const(0xE28B0000 | (intr(r) << 12), sv->c.i);
			if (fr & VT_SYM || !op) {
				load_value(sv, r);
				o(0xE08B0000 | (intr(r) << 12) | intr(r));
			} else
				o(op);
			return;
		} else if (v == VT_CMP) {
			o(mapcc(sv->c.i) | 0x3A00001 | (intr(r) << 12));
			o(mapcc(negcc(sv->c.i)) | 0x3A00000 | (intr(r) << 12));
			return;
		} else if (v == VT_JMP || v == VT_JMPI) {
			int t;
			t = v & 1;
			o(0xE3A00000 | (intr(r) << 12) | t);
			o(0xEA000000);
			gsym(sv->c.i);
			o(0xE3A00000 | (intr(r) << 12) | (t ^ 1));
			return;
		} else if (v < VT_CONST) {
			if (is_float(ft))
#ifdef MCC_ARM_VFP
				o(0xEEB00A40 | (vfpr(r) << 12) | vfpr(v) | T2CPR(ft));
#else
				o(0xEE008180 | (fpr(r) << 12) | fpr(v));
#endif
			else
				o(0xE1A00000 | (intr(r) << 12) | intr(v));
			return;
		}
	}
	mcc_error("load unimplemented!");
}

void store(int r, SValue *sv) {
	SValue v1;
	int v, ft, fc, fr, sign;
	uint32_t op, base;

	fr = sv->r;
	ft = sv->type.t;
	fc = sv->c.i;

	if (fc >= 0)
		sign = 0;
	else {
		sign = 1;
		fc = -fc;
	}

	v = fr & VT_VALMASK;
	if (fr & VT_LVAL || fr == VT_LOCAL) {
		base = 0xb;
		if ((fr & VT_SYM) && sv->sym->type.t & VT_TLS) {
			arm_tls_addr(sv->sym, sv->c.i);
			base = 14;
			v = VT_LOCAL;
			fc = sign = 0;
		} else if (v < VT_CONST) {
			base = intr(v);
			v = VT_LOCAL;
			fc = sign = 0;
		} else if (v == VT_CONST) {
			v1.type.t = ft;
			v1.r = fr & ~VT_LVAL;
			v1.c.i = sv->c.i;
			v1.sym = sv->sym;
			load(MCC_TREG_LR, &v1);
			base = 14;
			fc = sign = 0;
			v = VT_LOCAL;
		}
		if (v == VT_LOCAL) {
			if (is_float(ft)) {
				calcaddr(&base, &fc, &sign, 1020, 2);
#ifdef MCC_ARM_VFP
				op = 0xED000A00;
				if (!sign)
					op |= 0x800000;
				if ((ft & VT_BTYPE) != VT_FLOAT)
					op |= 0x100;
				o(op | (vfpr(r) << 12) | (fc >> 2) | (base << 16));
#else
				op = 0xED000100;
				if (!sign)
					op |= 0x800000;
				if ((ft & VT_BTYPE) == VT_DOUBLE)
					op |= 0x8000;
#if MCC_LDOUBLE_SIZE != 8
				else if ((ft & VT_BTYPE) == VT_LDOUBLE)
					op |= 0x400000;
#endif
				o(op | (fpr(r) << 12) | (fc >> 2) | (base << 16));
#endif
				return;
			} else if ((ft & VT_BTYPE) == VT_SHORT) {
				calcaddr(&base, &fc, &sign, 255, 0);
				op = 0xE14000B0;
				if (!sign)
					op |= 0x800000;
				o(op | (intr(r) << 12) | (base << 16) | ((fc & 0xf0) << 4) | (fc & 0xf));
			} else {
				calcaddr(&base, &fc, &sign, 4095, 0);
				op = 0xE5000000;
				if (!sign)
					op |= 0x800000;
				if ((ft & VT_BTYPE) == VT_BYTE || (ft & VT_BTYPE) == VT_BOOL)
					op |= 0x400000;
				o(op | (intr(r) << 12) | fc | (base << 16));
			}
			return;
		}
	}
	mcc_error("store unimplemented");
}

static void gadd_sp(int val) {
	stuff_const_harder(0xE28DD000, val);
}

static void gcall_or_jmp(int is_jmp) {
	int r;
	uint32_t x;
	if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST) {
		if (vtop->r & VT_SYM) {
			x = encbranch(ind, ind + vtop->c.i, 0);
			if (x) {
				greloc(cur_text_section, vtop->sym, ind, R_ARM_PC24);
				o(x | (is_jmp ? 0xE0000000 : 0xE1000000));
			} else {
				r = MCC_TREG_LR;
				load_value(vtop, r);
				if (is_jmp)
					o(0xE1A0F000 | intr(r));
				else
					o(0xe12fff30 | intr(r));
			}
		} else {
			if (!is_jmp)
				o(0xE28FE004);
			o(0xE51FF004);
			o(vtop->c.i);
		}
	} else {
#if MCC_CONFIG_BCHECK
		vtop->r &= ~VT_MUSTBOUND;
#endif
		r = gv(MCC_RC_INT);
		if (!is_jmp)
			o(0xE1A0E00F);
		o(0xE1A0F000 | intr(r));
	}
}

#if MCC_CONFIG_BCHECK

static void gen_bounds_call(int v) {
	Sym *sym = external_helper_sym(v);

	greloc(cur_text_section, sym, ind, R_ARM_PC24);
	o(0xebfffffe);
}

static void gen_bounds_prolog(void) {
	func_bound_offset = lbounds_section->data_offset;
	func_bound_ind = ind;
	func_bound_add_epilog = 0;
	o(0xe1a00000);
	o(0xe1a00000);
	o(0xe1a00000);
	o(0xe1a00000);
	o(0xe1a00000);
}

static void gen_bounds_epilog(void) {
	addr_t saved_ind;
	Sym *sym_data;
	int offset_modified;

	if (!gen_bounds_epilog_head(func_bound_offset, &sym_data, &offset_modified))
		return;

	if (offset_modified) {
		saved_ind = ind;
		ind = func_bound_ind;
		o(0xe59f0000);
		o(0xea000000);
		greloc(cur_text_section, sym_data, ind, R_ARM_REL32);
		o(-12);
		o(0xe080000f);
		gen_bounds_call(TOK___bound_local_new);
		ind = saved_ind;
	}

	o(0xe92d0003);
	o(0xed2d0b04);
	o(0xe59f0000);
	o(0xea000000);
	greloc(cur_text_section, sym_data, ind, R_ARM_REL32);
	o(-12);
	o(0xe080000f);
	gen_bounds_call(TOK___bound_local_delete);
	o(0xecbd0b04);
	o(0xe8bd0003);
}
#endif

static int is_hgen_float_aggr(CType *type) {
	if ((type->t & VT_BTYPE) == VT_STRUCT) {
		struct Sym *ref;
		int btype, nb_fields = 0;

		ref = type->ref->next;
		if (ref) {
			btype = ref->type.t & VT_BTYPE;
			if (btype == VT_FLOAT || btype == VT_DOUBLE) {
				for (; ref && btype == (ref->type.t & VT_BTYPE); ref = ref->next, nb_fields++)
					;
				return !ref && nb_fields <= 4;
			}
		}
	}
	return 0;
}

struct avail_regs {
	signed char avail[3];
	int first_hole;
	int last_hole;
	int first_free_reg;
};

int assign_vfpreg(struct avail_regs *avregs, int align, int size) {
	int first_reg = 0;

	if (avregs->first_free_reg == -1)
		return -1;
	if (align >> 3) {
		first_reg = avregs->first_free_reg;
		if (first_reg & 1)
			avregs->avail[avregs->last_hole++] = first_reg++;
	} else {
		if (size == 4 && avregs->first_hole != avregs->last_hole)
			return avregs->avail[avregs->first_hole++];
		else
			first_reg = avregs->first_free_reg;
	}
	if (first_reg + size / 4 <= 16) {
		avregs->first_free_reg = first_reg + size / 4;
		return first_reg;
	}
	avregs->first_free_reg = -1;
	return -1;
}

int floats_in_core_regs(SValue *sval) {
	if (!sval->sym)
		return 0;

	switch (sval->sym->v) {
	case TOK___floatundisf:
	case TOK___floatundidf:
	case TOK___fixunssfdi:
	case TOK___fixunsdfdi:
#ifndef MCC_ARM_VFP
	case TOK___fixunsxfdi:
#endif
	case TOK___floatdisf:
	case TOK___floatdidf:
	case TOK___fixsfdi:
	case TOK___fixdfdi:
		return 1;

	default:
		return 0;
	}
}

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) {
#ifdef MCC_ARM_EABI
	int size, align;
	size = type_size(vt, &align);
	if (cg_float_abi == ARM_HARD_FLOAT && !variadic &&
			(is_float(vt->t) || is_hgen_float_aggr(vt))) {
		*ret_align = 8;
		*regsize = 8;
		ret->ref = NULL;
		ret->t = VT_DOUBLE;
		return (size + 7) >> 3;
	} else if (size > 0 && size <= 4) {
		*ret_align = 4;
		*regsize = 4;
		ret->ref = NULL;
		ret->t = VT_INT;
		return 1;
	} else
		return 0;
#else
	return 0;
#endif
}

enum reg_class {
	STACK_CLASS = 0,
	CORE_STRUCT_CLASS,
	VFP_CLASS,
	VFP_STRUCT_CLASS,
	CORE_CLASS,
	NB_CLASSES
};

struct param_plan {
	int start;
	int end;
	SValue *sval;
	struct param_plan *prev;
};

struct plan {
	struct param_plan *pplans;
	struct param_plan *clsplans[NB_CLASSES];
	int nb_plans;
};

static void add_param_plan(struct plan *plan, int cls, int start, int end, SValue *v) {
	struct param_plan *p = &plan->pplans[plan->nb_plans++];
	p->prev = plan->clsplans[cls];
	plan->clsplans[cls] = p;
	p->start = start, p->end = end, p->sval = v;
}

static int assign_regs(int nb_args, int float_abi, struct plan *plan, int *todo) {
	int size, align;
	int ncrn, nsaa;
	struct avail_regs avregs = {0};

	ncrn = nsaa = 0;
	*todo = 0;

	for (int i = nb_args; i--;) {
		int j, start_vfpreg = 0;
		CType type = vtop[-i].type;
		type.t &= ~VT_ARRAY;
		size = type_size(&type, &align);
		size = (size + 3) & ~3;
		align = (align + 3) & ~3;
		switch (vtop[-i].type.t & VT_BTYPE) {
		case VT_STRUCT:
		case VT_FLOAT:
		case VT_DOUBLE:
		case VT_LDOUBLE:
			if (float_abi == ARM_HARD_FLOAT) {
				int is_hfa = 0;

				if (is_float(vtop[-i].type.t) || (is_hfa = is_hgen_float_aggr(&vtop[-i].type))) {
					int end_vfpreg;

					start_vfpreg = assign_vfpreg(&avregs, align, size);
					end_vfpreg = start_vfpreg + ((size - 1) >> 2);
					if (start_vfpreg >= 0) {
						add_param_plan(plan, is_hfa ? VFP_STRUCT_CLASS : VFP_CLASS,
													 start_vfpreg, end_vfpreg, &vtop[-i]);
						continue;
					} else
						break;
				}
			}
			ncrn = (ncrn + (align - 1) / 4) & ~((align / 4) - 1);
			if (ncrn + size / 4 <= 4 || (ncrn < 4 && start_vfpreg != -1)) {
				for (j = ncrn; j < 4 && j < ncrn + size / 4; j++)
					*todo |= (1 << j);
				add_param_plan(plan, CORE_STRUCT_CLASS, ncrn, j, &vtop[-i]);
				ncrn += size / 4;
				if (ncrn > 4)
					nsaa = (ncrn - 4) * 4;
			} else {
				ncrn = 4;
				break;
			}
			continue;
		default:
			if (ncrn < 4) {
				int is_long = (vtop[-i].type.t & VT_BTYPE) == VT_LLONG;

				if (is_long) {
					ncrn = (ncrn + 1) & -2;
					if (ncrn == 4)
						break;
				}
				add_param_plan(plan, CORE_CLASS, ncrn, ncrn + is_long, &vtop[-i]);
				ncrn += 1 + is_long;
				continue;
			}
		}
		nsaa = (nsaa + (align - 1)) & ~(align - 1);
		add_param_plan(plan, STACK_CLASS, nsaa, nsaa + size, &vtop[-i]);
		nsaa += size;
	}
	return nsaa;
}

static int copy_params(int nb_args, struct plan *plan, int todo) {
	int size, align, r, nb_extra_sval = 0;
	struct param_plan *pplan;
	int pass = 0;

again:
	for (int i = 0; i < NB_CLASSES; i++) {
		for (pplan = plan->clsplans[i]; pplan; pplan = pplan->prev) {
			if (pass && (i != CORE_CLASS || pplan->sval->r < VT_CONST))
				continue;

			vpushv(pplan->sval);
			pplan->sval->r = pplan->sval->r2 = VT_CONST;
			switch (i) {
			case STACK_CLASS:
			case CORE_STRUCT_CLASS:
			case VFP_STRUCT_CLASS:
				if ((pplan->sval->type.t & VT_BTYPE) == VT_STRUCT) {
					int padding = 0;
					size = type_size(&pplan->sval->type, &align);
					size = (size + 3) & ~3;
					if (i == STACK_CLASS && pplan->prev)
						padding = pplan->start - pplan->prev->end;
					size += padding;
					gadd_sp(-size);
					r = get_reg(MCC_RC_INT);
					o(0xE28D0000 | (intr(r) << 12) | padding);
					vset(&vtop->type, r | VT_LVAL, 0);
					vswap();
					o(0xED2D0A00 | (0 & 1) << 22 | (0 >> 1) << 12 | 16);
					vstore();
					o(0xECBD0A00 | (0 & 1) << 22 | (0 >> 1) << 12 | 16);

					if (i == VFP_STRUCT_CLASS) {
						int first = pplan->start, nb = pplan->end - first + 1;
						o(0xECBD0A00 | (first & 1) << 22 | (first >> 1) << 12 | nb);
					}
				} else {
					if (is_float(pplan->sval->type.t)) {
#ifdef MCC_ARM_VFP
						r = vfpr(gv(MCC_RC_FLOAT)) << 12;
						if ((pplan->sval->type.t & VT_BTYPE) == VT_FLOAT)
							size = 4;
						else {
							size = 8;
							r |= 0x101;
						}
						o(0xED2D0A01 + r);
#else
						r = fpr(gv(MCC_RC_FLOAT)) << 12;
						if ((pplan->sval->type.t & VT_BTYPE) == VT_FLOAT)
							size = 4;
						else if ((pplan->sval->type.t & VT_BTYPE) == VT_DOUBLE)
							size = 8;
						else
							size = MCC_LDOUBLE_SIZE;
						if (size == 12)
							r |= 0x400000;
						else if (size == 8)
							r |= 0x8000;

						o(0xED2D0100 | r | (size >> 2));
#endif
					} else {
						size = 4;
						if ((pplan->sval->type.t & VT_BTYPE) == VT_LLONG) {
							lexpand();
							size = 8;
							r = gv(MCC_RC_INT);
							o(0xE52D0004 | (intr(r) << 12));
							vtop--;
						}
						r = gv(MCC_RC_INT);
						o(0xE52D0004 | (intr(r) << 12));
					}
					if (i == STACK_CLASS && pplan->prev)
						gadd_sp(pplan->prev->end - pplan->start);
				}
				break;

			case VFP_CLASS:
				gv(regmask(MCC_TREG_F0 + (pplan->start >> 1)));
				if (pplan->start & 1) {
					o(0xEEF00A40 | ((pplan->start >> 1) << 12) | (pplan->start >> 1));
					vtop->r = VT_CONST;
				}
				break;

			case CORE_CLASS:
				if ((pplan->sval->type.t & VT_BTYPE) == VT_LLONG) {
					lexpand();
					gv(regmask(pplan->end));
					pplan->sval->r2 = vtop->r;
					vtop--;
				}
				gv(regmask(pplan->start));
				pplan->sval->r = vtop->r;
				break;
			}
			vtop--;
		}
	}

	if (++pass < 2)
		goto again;

	if (todo) {
		o(0xE8BD0000 | todo);
		for (pplan = plan->clsplans[CORE_STRUCT_CLASS]; pplan; pplan = pplan->prev) {
			pplan->sval->r = pplan->start;
			for (int r = pplan->start + 1; r <= pplan->end; r++) {
				if (todo & (1 << r)) {
					nb_extra_sval++;
					vpushi(0);
					vtop->r = r;
				}
			}
		}
	}
	return nb_extra_sval;
}

void gfunc_call(int nb_args) {
	int args_size;
	int def_float_abi = cg_float_abi;
	int todo;
	struct plan plan;
#ifdef MCC_ARM_EABI
	int variadic;
#endif

#if MCC_CONFIG_BCHECK
	if (mcc_state->do_bounds_check)
		gbound_args(nb_args);
#endif

	save_regs(nb_args + 1);

#ifdef MCC_ARM_EABI
	if (cg_float_abi == ARM_HARD_FLOAT) {
		variadic = (vtop[-nb_args].type.ref->f.func_type == FUNC_ELLIPSIS);
		if (variadic || floats_in_core_regs(&vtop[-nb_args]))
			cg_float_abi = ARM_SOFTFP_FLOAT;
	}
#endif

	memset(&plan, 0, sizeof plan);
	if (nb_args)
		plan.pplans = mcc_malloc(nb_args * sizeof(*plan.pplans));

	args_size = assign_regs(nb_args, cg_float_abi, &plan, &todo);

#ifdef MCC_ARM_EABI
	if (args_size & 7) {
		args_size = (args_size + 7) & ~7;
		o(0xE24DD004);
	}
#endif

	nb_args += copy_params(nb_args, &plan, todo);
	mcc_free(plan.pplans);

	vrotb(nb_args + 1);
	gcall_or_jmp(0);
	if (args_size)
		gadd_sp(args_size);
#if defined(MCC_ARM_EABI) && defined(MCC_ARM_VFP)
	if (cg_float_abi == ARM_SOFTFP_FLOAT && is_float(vtop->type.ref->type.t)) {
		if ((vtop->type.ref->type.t & VT_BTYPE) == VT_FLOAT) {
			o(0xEE000A10);
		} else {
			o(0xEE000B10);
			o(0xEE201B10);
		}
	}
#endif
	vtop -= nb_args + 1;
	leaffunc = 0;
	cg_float_abi = def_float_abi;
}

void gfunc_prolog(Sym *func_sym) {
	CType *func_type = &func_sym->type;
	Sym *sym, *sym2;
	int n, nf, size, align, rs, struct_ret = 0;
	int addr, pn, sn;
	CType ret_type;

#ifdef MCC_ARM_EABI
	struct avail_regs avregs = {0};
#endif

	sym = func_type->ref;

	n = nf = 0;
	if ((func_vt.t & VT_BTYPE) == VT_STRUCT &&
			!gfunc_sret(&func_vt, func_var, &ret_type, &align, &rs)) {
		n++;
		struct_ret = 1;
		func_vc = 16;
	}
	for (sym2 = sym->next; sym2 && (n < 4 || nf < 16); sym2 = sym2->next) {
		size = type_size(&sym2->type, &align);
#ifdef MCC_ARM_EABI
		if (cg_float_abi == ARM_HARD_FLOAT && !func_var &&
				(is_float(sym2->type.t) || is_hgen_float_aggr(&sym2->type))) {
			int tmpnf = assign_vfpreg(&avregs, align, size);
			tmpnf += (size + 3) / 4;
			nf = (tmpnf > nf) ? tmpnf : nf;
		} else
#endif
				if (n < 4)
			n += (size + 3) / 4;
	}
	o(0xE1A0C00D);
	if (func_var)
		n = 4;
	if (n) {
		if (n > 4)
			n = 4;
#ifdef MCC_ARM_EABI
		n = (n + 1) & -2;
#endif
		o(0xE92D0000 | ((1 << n) - 1));
	}
	if (nf) {
		if (nf > 16)
			nf = 16;
		nf = (nf + 1) & -2;
		o(0xED2D0A00 | nf);
	}
	o(0xE92D5C00);
	o(0xE1A0B00D);
	func_sub_sp_offset = ind;
	o(0xE1A00000);

#ifdef MCC_ARM_EABI
	if (cg_float_abi == ARM_HARD_FLOAT) {
		func_vc += nf * 4;
		memset(&avregs, 0, sizeof avregs);
	}
#endif
	pn = struct_ret, sn = 0;
	while ((sym = sym->next)) {
		CType *type;
		type = &sym->type;
		size = type_size(type, &align);
		size = (size + 3) >> 2;
		align = (align + 3) & ~3;
#ifdef MCC_ARM_EABI
		if (cg_float_abi == ARM_HARD_FLOAT && !func_var && (is_float(sym->type.t) || is_hgen_float_aggr(&sym->type))) {
			int fpn = assign_vfpreg(&avregs, align, size << 2);
			if (fpn >= 0)
				addr = fpn * 4;
			else
				goto from_stack;
		} else
#endif
				if (pn < 4) {
#ifdef MCC_ARM_EABI
			pn = (pn + (align - 1) / 4) & -(align / 4);
#endif
			addr = (nf + pn) * 4;
			pn += size;
			if (!sn && pn > 4)
				sn = (pn - 4);
		} else {
#ifdef MCC_ARM_EABI
		from_stack:
			sn = (sn + (align - 1) / 4) & -(align / 4);
#endif
			addr = (n + nf + sn) * 4;
			sn += size;
		}
		gfunc_set_param(sym, addr + 16, 0);
	}
	last_itod_magic = 0;
	leaffunc = 1;
	loc = 0;
#if MCC_CONFIG_BCHECK
	if (mcc_state->do_bounds_check)
		gen_bounds_prolog();
#endif
}

void gfunc_epilog(void) {
	uint32_t x;
	int diff;

#if MCC_CONFIG_BCHECK
	if (mcc_state->do_bounds_check)
		gen_bounds_epilog();
#endif
#if defined(MCC_ARM_EABI) && defined(MCC_ARM_VFP)
	if ((cg_float_abi == ARM_SOFTFP_FLOAT || func_var) && is_float(func_vt.t)) {
		if ((func_vt.t & VT_BTYPE) == VT_FLOAT)
			o(0xEE100A10);
		else {
			o(0xEE100B10);
			o(0xEE301B10);
		}
	}
#endif
	o(0xE89BAC00);
	diff = (-loc + 3) & -4;
#ifdef MCC_ARM_EABI
	if (!leaffunc)
		diff = (diff + 7) & -8;
#endif
	if (diff > 0) {
		x = stuff_const(0xE24BD000, diff);
		if (x)
			*(uint32_t *)(cur_text_section->data + func_sub_sp_offset) = x;
		else {
			int addr;
			addr = ind;
			o(0xE59FC004);
			o(0xE04BD00C);
			o(0xE1A0F00E);
			o(diff);
			*(uint32_t *)(cur_text_section->data + func_sub_sp_offset) = 0xE1000000 | encbranch(
																																										func_sub_sp_offset, addr, 1);
		}
	}
}

ST_FUNC void gen_fill_nops(int bytes) {
	if ((bytes & 3))
		mcc_error("alignment of code section not multiple of 4");
	while (bytes > 0) {
		o(0xE1A00000);
		bytes -= 4;
	}
}

ST_FUNC int gjmp(int t) {
	int r;
	if (nocode_wanted)
		return t;
	r = ind;
	o(0xE0000000 | encbranch(r, t, 1));
	return r;
}

ST_FUNC void gjmp_addr(int a) {
	gjmp(a);
}

ST_FUNC int gjmp_cond(int op, int t) {
	int r;
	if (nocode_wanted)
		return t;
	r = ind;
	op = mapcc(op);
	op |= encbranch(r, t, 1);
	o(op);
	return r;
}

ST_FUNC int gjmp_append(int n, int t) {
	uint32_t *x;
	int p, lp;
	if (n) {
		p = n;
		do {
			p = decbranch(lp = p);
		} while (p);
		x = (uint32_t *)(cur_text_section->data + lp);
		*x &= 0xff000000;
		*x |= encbranch(lp, t, 1);
		t = n;
	}
	return t;
}

void gen_opi(int op) {
	int c, func = 0;
	uint32_t opc = 0, r, fr;
	unsigned short retreg = REG_IRET;

	c = 0;
	switch (op) {
	case '+':
		opc = 0x8;
		c = 1;
		break;
	case TOK_ADDC1:
		opc = 0x9;
		c = 1;
		break;
	case '-':
		opc = 0x4;
		c = 1;
		break;
	case TOK_SUBC1:
		opc = 0x5;
		c = 1;
		break;
	case TOK_ADDC2:
		opc = 0xA;
		c = 1;
		break;
	case TOK_SUBC2:
		opc = 0xC;
		c = 1;
		break;
	case '&':
		opc = 0x0;
		c = 1;
		break;
	case '^':
		opc = 0x2;
		c = 1;
		break;
	case '|':
		opc = 0x18;
		c = 1;
		break;
	case '*':
		gv2(MCC_RC_INT, MCC_RC_INT);
		r = vtop[-1].r;
		fr = vtop[0].r;
		vtop--;
		o(0xE0000090 | (intr(r) << 16) | (intr(r) << 8) | intr(fr));
		return;
	case TOK_SHL:
		opc = 0;
		c = 2;
		break;
	case TOK_SHR:
		opc = 1;
		c = 2;
		break;
	case TOK_SAR:
		opc = 2;
		c = 2;
		break;
	case '/':
	case TOK_PDIV:
		func = TOK___divsi3;
		c = 3;
		break;
	case TOK_UDIV:
		func = TOK___udivsi3;
		c = 3;
		break;
	case '%':
#ifdef MCC_ARM_EABI
		func = TOK___aeabi_idivmod;
		retreg = REG_IRE2;
#else
		func = TOK___modsi3;
#endif
		c = 3;
		break;
	case TOK_UMOD:
#ifdef MCC_ARM_EABI
		func = TOK___aeabi_uidivmod;
		retreg = REG_IRE2;
#else
		func = TOK___umodsi3;
#endif
		c = 3;
		break;
	case TOK_UMULL:
		gv2(MCC_RC_INT, MCC_RC_INT);
		r = intr(vtop[-1].r2 = get_reg(MCC_RC_INT));
		c = vtop[-1].r;
		vtop[-1].r = get_reg_ex(MCC_RC_INT, regmask(c));
		vtop--;
		o(0xE0800090 | (r << 16) | (intr(vtop->r) << 12) | (intr(c) << 8) | intr(vtop[1].r));
		return;
	default:
		opc = 0x15;
		c = 1;
		break;
	}
	switch (c) {
	case 1:
		if ((vtop[-1].r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			if (opc == 4 || opc == 5 || opc == 0xc) {
				vswap();
				opc |= 2;
			}
		}
		vswap();
		c = intr(gv(MCC_RC_INT));
		vswap();
		opc = 0xE0000000 | (opc << 20);
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			uint32_t x;
			x = stuff_const(opc | 0x2000000 | (c << 16), vtop->c.i);
			if (x) {
				if ((x & 0xfff00000) == 0xe3500000)
					o(x);
				else {
					r = intr(vtop[-1].r = get_reg_ex(MCC_RC_INT, regmask(vtop[-1].r)));
					o(x | (r << 12));
				}
				goto done;
			}
		}
		fr = intr(gv(MCC_RC_INT));
#if MCC_CONFIG_BCHECK
		if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
			vswap();
			c = intr(gv(MCC_RC_INT));
			vswap();
		}
#endif
		if ((opc & 0xfff00000) == 0xe1500000)
			o(opc | (c << 16) | fr);
		else {
			r = intr(vtop[-1].r = get_reg_ex(MCC_RC_INT, two2mask(vtop->r, vtop[-1].r)));
			o(opc | (c << 16) | (r << 12) | fr);
		}
	done:
		vtop--;
		if (op >= TOK_ULT && op <= TOK_GT)
			vset_VT_CMP(op);
		break;
	case 2:
		opc = 0xE1A00000 | (opc << 5);
		vswap();
		r = intr(gv(MCC_RC_INT));
		vswap();
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) {
			fr = intr(vtop[-1].r = get_reg_ex(MCC_RC_INT, regmask(vtop[-1].r)));
			c = vtop->c.i & 0x1f;
			o(opc | r | (c << 7) | (fr << 12));
		} else {
			fr = intr(gv(MCC_RC_INT));
#if MCC_CONFIG_BCHECK
			if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
				vswap();
				r = intr(gv(MCC_RC_INT));
				vswap();
			}
#endif
			c = intr(vtop[-1].r = get_reg_ex(MCC_RC_INT, two2mask(vtop->r, vtop[-1].r)));
			o(opc | r | (c << 12) | (fr << 8) | 0x10);
		}
		vtop--;
		break;
	case 3:
		vpush_helper_func(func);
		vrott(3);
		gfunc_call(2);
		vpushi(0);
		vtop->r = retreg;
		break;
	default:
		mcc_error("gen_opi %i unimplemented!", op);
	}
}

#ifdef MCC_ARM_VFP
static int is_zero(int i) {
	if ((vtop[i].r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
		return 0;
	if (vtop[i].type.t == VT_FLOAT)
		return (vtop[i].c.f == 0.f);
	else if (vtop[i].type.t == VT_DOUBLE)
		return (vtop[i].c.d == 0.0);
	return (vtop[i].c.ld == 0.l);
}

void gen_opf(int op) {
	uint32_t x;
	int fneg = 0, r;
	x = 0xEE000A00 | T2CPR(vtop->type.t);
	switch (op) {
	case '+':
		if (is_zero(-1))
			vswap();
		if (is_zero(0)) {
			vtop--;
			return;
		}
		x |= 0x300000;
		break;
	case '-':
		x |= 0x300040;
		if (is_zero(0)) {
			vtop--;
			return;
		}
		if (is_zero(-1)) {
			x |= 0x810000;
			vswap();
			vtop--;
			fneg = 1;
		}
		break;
	case '*':
		x |= 0x200000;
		break;
	case '/':
		x |= 0x800000;
		break;
	default:
		if (op < TOK_ULT || op > TOK_GT) {
			mcc_error("unknown fp op %x!", op);
			return;
		}
		if (is_zero(-1)) {
			vswap();
			switch (op) {
			case TOK_LT:
				op = TOK_GT;
				break;
			case TOK_GE:
				op = TOK_ULE;
				break;
			case TOK_LE:
				op = TOK_GE;
				break;
			case TOK_GT:
				op = TOK_ULT;
				break;
			}
		}
		x |= 0xB40040;
		if (op != TOK_EQ && op != TOK_NE)
			x |= 0x80;
		if (is_zero(0)) {
			vtop--;
			o(x | 0x10000 | (vfpr(gv(MCC_RC_FLOAT)) << 12));
		} else {
			gv2(MCC_RC_FLOAT, MCC_RC_FLOAT);
			x |= vfpr(vtop[0].r);
			o(x | (vfpr(vtop[-1].r) << 12));
			vtop--;
		}
		o(0xEEF1FA10);

		switch (op) {
		case TOK_LE:
			op = TOK_ULE;
			break;
		case TOK_LT:
			op = TOK_ULT;
			break;
		case TOK_UGE:
			op = TOK_GE;
			break;
		case TOK_UGT:
			op = TOK_GT;
			break;
		}
		vset_VT_CMP(op);
		return;
	}
	r = gv(MCC_RC_FLOAT);
	x |= vfpr(r);
	r = regmask(r);
	if (!fneg) {
		int r2;
		vswap();
		r2 = gv(MCC_RC_FLOAT);
		x |= vfpr(r2) << 16;
		r |= regmask(r2);
#if MCC_CONFIG_BCHECK
		if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
			vswap();
			r = gv(MCC_RC_FLOAT);
			vswap();
			x = (x & ~0xf) | vfpr(r);
		}
#endif
	}
	vtop->r = get_reg_ex(MCC_RC_FLOAT, r);
	if (!fneg)
		vtop--;
	o(x | (vfpr(vtop->r) << 12));
}

#else
static uint32_t is_fconst() {
	long double f;
	uint32_t r;
	if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
		return 0;
	if (vtop->type.t == VT_FLOAT)
		f = vtop->c.f;
	else if (vtop->type.t == VT_DOUBLE)
		f = vtop->c.d;
	else
		f = vtop->c.ld;
	if (!ieee_finite(f))
		return 0;
	r = 0x8;
	if (f < 0.0) {
		r = 0x18;
		f = -f;
	}
	if (f == 0.0)
		return r;
	if (f == 1.0)
		return r | 1;
	if (f == 2.0)
		return r | 2;
	if (f == 3.0)
		return r | 3;
	if (f == 4.0)
		return r | 4;
	if (f == 5.0)
		return r | 5;
	if (f == 0.5)
		return r | 6;
	if (f == 10.0)
		return r | 7;
	return 0;
}

void gen_opf(int op) {
	uint32_t x, r, r2, c1, c2;
	vswap();
	c1 = is_fconst();
	vswap();
	c2 = is_fconst();
	x = 0xEE000100;
	if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE)
		x |= 0x80;
#if MCC_LDOUBLE_SIZE != 8
	else if ((vtop->type.t & VT_BTYPE) == VT_LDOUBLE)
		x |= 0x80000;
#endif

	switch (op) {
	case '+':
		if (!c2) {
			vswap();
			c2 = c1;
		}
		vswap();
		r = fpr(gv(MCC_RC_FLOAT));
		vswap();
		if (c2) {
			if (c2 > 0xf)
				x |= 0x200000;
			r2 = c2 & 0xf;
		} else {
			r2 = fpr(gv(MCC_RC_FLOAT));
#if MCC_CONFIG_BCHECK
			if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
				vswap();
				r = fpr(gv(MCC_RC_FLOAT));
				vswap();
			}
#endif
		}
		break;
	case '-':
		if (c2) {
			if (c2 <= 0xf)
				x |= 0x200000;
			r2 = c2 & 0xf;
			vswap();
			r = fpr(gv(MCC_RC_FLOAT));
			vswap();
		} else if (c1 && c1 <= 0xf) {
			x |= 0x300000;
			r2 = c1;
			r = fpr(gv(MCC_RC_FLOAT));
			vswap();
		} else {
			x |= 0x200000;
			vswap();
			r = fpr(gv(MCC_RC_FLOAT));
			vswap();
			r2 = fpr(gv(MCC_RC_FLOAT));
#if MCC_CONFIG_BCHECK
			if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
				vswap();
				r = fpr(gv(MCC_RC_FLOAT));
				vswap();
			}
#endif
		}
		break;
	case '*':
		if (!c2 || c2 > 0xf) {
			vswap();
			c2 = c1;
		}
		vswap();
		r = fpr(gv(MCC_RC_FLOAT));
		vswap();
		if (c2 && c2 <= 0xf)
			r2 = c2;
		else {
			r2 = fpr(gv(MCC_RC_FLOAT));
#if MCC_CONFIG_BCHECK
			if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
				vswap();
				r = fpr(gv(MCC_RC_FLOAT));
				vswap();
			}
#endif
		}
		x |= 0x100000;
		break;
	case '/':
		if (c2 && c2 <= 0xf) {
			x |= 0x400000;
			r2 = c2;
			vswap();
			r = fpr(gv(MCC_RC_FLOAT));
			vswap();
		} else if (c1 && c1 <= 0xf) {
			x |= 0x500000;
			r2 = c1;
			r = fpr(gv(MCC_RC_FLOAT));
			vswap();
		} else {
			x |= 0x400000;
			vswap();
			r = fpr(gv(MCC_RC_FLOAT));
			vswap();
			r2 = fpr(gv(MCC_RC_FLOAT));
#if MCC_CONFIG_BCHECK
			if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
				vswap();
				r = fpr(gv(MCC_RC_FLOAT));
				vswap();
			}
#endif
		}
		break;
	default:
		if (op >= TOK_ULT && op <= TOK_GT) {
			x |= 0xd0f110;
			switch (op) {
			case TOK_ULT:
			case TOK_UGE:
			case TOK_ULE:
			case TOK_UGT:
				mcc_error("unsigned comparison on floats?");
				break;
			case TOK_LT:
				op = TOK_Nset;
				break;
			case TOK_LE:
				op = TOK_ULE;
				break;
			case TOK_EQ:
			case TOK_NE:
				x &= ~0x400000;
				break;
			}
			if (c1 && !c2) {
				c2 = c1;
				vswap();
				switch (op) {
				case TOK_Nset:
					op = TOK_GT;
					break;
				case TOK_GE:
					op = TOK_ULE;
					break;
				case TOK_ULE:
					op = TOK_GE;
					break;
				case TOK_GT:
					op = TOK_Nset;
					break;
				}
			}
			vswap();
			r = fpr(gv(MCC_RC_FLOAT));
			vswap();
			if (c2) {
				if (c2 > 0xf)
					x |= 0x200000;
				r2 = c2 & 0xf;
			} else {
				r2 = fpr(gv(MCC_RC_FLOAT));
#if MCC_CONFIG_BCHECK
				if ((vtop[-1].r & VT_VALMASK) >= VT_CONST) {
					vswap();
					r = fpr(gv(MCC_RC_FLOAT));
					vswap();
				}
#endif
			}
			--vtop;
			vset_VT_CMP(op);
			++vtop;
		} else {
			mcc_error("unknown fp op %x!", op);
			return;
		}
	}
	if (vtop[-1].r == VT_CMP)
		c1 = 15;
	else {
		c1 = vtop->r;
		if (r2 & 0x8)
			c1 = vtop[-1].r;
		vtop[-1].r = get_reg_ex(MCC_RC_FLOAT, two2mask(vtop[-1].r, c1));
		c1 = fpr(vtop[-1].r);
	}
	vtop--;
	o(x | (r << 16) | (c1 << 12) | r2);
}
#endif

ST_FUNC void gen_cvt_itof(int t) {
	uint32_t r, r2;
	int bt;
	bt = vtop->type.t & VT_BTYPE;
	if (bt == VT_INT || bt == VT_SHORT || bt == VT_BYTE) {
#ifndef MCC_ARM_VFP
		uint32_t dsize = 0;
#endif
		r = intr(gv(MCC_RC_INT));
#ifdef MCC_ARM_VFP
		r2 = vfpr(vtop->r = get_reg(MCC_RC_FLOAT));
		o(0xEE000A10 | (r << 12) | (r2 << 16));
		r2 |= r2 << 12;
		if (!(vtop->type.t & VT_UNSIGNED))
			r2 |= 0x80;
		o(0xEEB80A40 | r2 | T2CPR(t));
#else
		r2 = fpr(vtop->r = get_reg(MCC_RC_FLOAT));
		if ((t & VT_BTYPE) != VT_FLOAT)
			dsize = 0x80;
		o(0xEE000110 | dsize | (r2 << 16) | (r << 12));
		if ((vtop->type.t & (VT_UNSIGNED | VT_BTYPE)) == (VT_UNSIGNED | VT_INT)) {
			uint32_t off = 0;
			o(0xE3500000 | (r << 12));
			r = fpr(get_reg(MCC_RC_FLOAT));
			if (last_itod_magic) {
				off = ind + 8 - last_itod_magic;
				off /= 4;
				if (off > 255)
					off = 0;
			}
			o(0xBD1F0100 | (r << 12) | off);
			if (!off) {
				o(0xEA000000);
				last_itod_magic = ind;
				o(0x4F800000);
			}
			o(0xBE000100 | dsize | (r2 << 16) | (r2 << 12) | r);
		}
#endif
		return;
	} else if (bt == VT_LLONG) {
		int func;
		CType *func_type = 0;
		if ((t & VT_BTYPE) == VT_FLOAT) {
			func_type = &func_float_type;
			if (vtop->type.t & VT_UNSIGNED)
				func = TOK___floatundisf;
			else
				func = TOK___floatdisf;
		} else if ((t & VT_BTYPE) == VT_DOUBLE) {
			func_type = &func_double_type;
			if (vtop->type.t & VT_UNSIGNED)
				func = TOK___floatundidf;
			else
				func = TOK___floatdidf;
#if MCC_LDOUBLE_SIZE != 8
		} else if ((t & VT_BTYPE) == VT_LDOUBLE) {
			func_type = &func_ldouble_type;
			if (vtop->type.t & VT_UNSIGNED)
				func = TOK___floatundixf;
			else
				func = TOK___floatdixf;
#endif
		}
		if (func_type) {
			vpushsym(func_type, external_helper_sym(func));
			vswap();
			gfunc_call(1);
			vpushi(0);
			vtop->r = MCC_TREG_F0;
			return;
		}
	}
	mcc_error("unimplemented gen_cvt_itof %x!", vtop->type.t);
}

void gen_cvt_ftoi(int t) {
	uint32_t r, r2;
	int u, func = 0;
	u = t & VT_UNSIGNED;
	t &= VT_BTYPE;
	r2 = vtop->type.t & VT_BTYPE;
	if (t == VT_INT) {
#ifdef MCC_ARM_VFP
		r = vfpr(gv(MCC_RC_FLOAT));
		u = u ? 0 : 0x10000;
		o(0xEEBC0AC0 | (r << 12) | r | T2CPR(r2) | u);
		r2 = intr(vtop->r = get_reg(MCC_RC_INT));
		o(0xEE100A10 | (r << 16) | (r2 << 12));
		return;
#else
		if (u) {
			if (r2 == VT_FLOAT)
				func = TOK___fixunssfsi;
			else if (r2 == VT_DOUBLE)
				func = TOK___fixunsdfsi;
#if MCC_LDOUBLE_SIZE != 8
			else if (r2 == VT_LDOUBLE)
				func = TOK___fixunsxfsi;
#endif
		} else {
			r = fpr(gv(MCC_RC_FLOAT));
			r2 = intr(vtop->r = get_reg(MCC_RC_INT));
			o(0xEE100170 | (r2 << 12) | r);
			return;
		}
#endif
	} else if (t == VT_LLONG) {
		if (r2 == VT_FLOAT)
			func = TOK___fixsfdi;
		else if (r2 == VT_DOUBLE)
			func = TOK___fixdfdi;
#if MCC_LDOUBLE_SIZE != 8
		else if (r2 == VT_LDOUBLE)
			func = TOK___fixxfdi;
#endif
	}
	if (func) {
		vpush_helper_func(func);
		vswap();
		gfunc_call(1);
		vpushi(0);
		if (t == VT_LLONG)
			vtop->r2 = REG_IRE2;
		vtop->r = REG_IRET;
		return;
	}
	mcc_error("unimplemented gen_cvt_ftoi!");
}

void gen_cvt_ftof(int t) {
#ifdef MCC_ARM_VFP
	uint32_t r = gv(MCC_RC_FLOAT);
	if (((vtop->type.t & VT_BTYPE) == VT_FLOAT) != ((t & VT_BTYPE) == VT_FLOAT)) {
		r = vfpr(r);
		o(0xEEB70AC0 | (r << 12) | r | T2CPR(vtop->type.t));
	}
#else
	gv(MCC_RC_FLOAT);
#endif
}

ST_FUNC void gen_increment_tcov(SValue *sv) {
	int r1, r2;

	vpushv(sv);
	vtop->r = r1 = get_reg(MCC_RC_INT);
	r2 = get_reg(MCC_RC_INT);
	o(0xE59F0000 | (intr(r1) << 12));
	o(0xEA000000);
	greloc(cur_text_section, sv->sym, ind, R_ARM_REL32);
	o(-12);
	o(0xe080000f | (intr(r1) << 16) | (intr(r1) << 12));
	o(0xe5900000 | (intr(r1) << 16) | (intr(r2) << 12));
	o(0xe2900001 | (intr(r2) << 16) | (intr(r2) << 12));
	o(0xe5800000 | (intr(r1) << 16) | (intr(r2) << 12));
	o(0xe2800004 | (intr(r1) << 16) | (intr(r1) << 12));
	o(0xe5900000 | (intr(r1) << 16) | (intr(r2) << 12));
	o(0xe2a00000 | (intr(r2) << 16) | (intr(r2) << 12));
	o(0xe5800000 | (intr(r1) << 16) | (intr(r2) << 12));
	vpop();
}

void ggoto(void) {
	gcall_or_jmp(1);
	vtop--;
}

ST_FUNC void gen_vla_sp_save(int addr) {
	SValue v;
	v.type.t = VT_PTR;
	v.r = VT_LOCAL | VT_LVAL;
	v.c.i = addr;
	store(MCC_TREG_SP, &v);
}

ST_FUNC void gen_vla_sp_restore(int addr) {
	SValue v;
	v.type.t = VT_PTR;
	v.r = VT_LOCAL | VT_LVAL;
	v.c.i = addr;
	load(MCC_TREG_SP, &v);
}

ST_FUNC void gen_vla_alloc(CType *type, int align) {
	int r;
#if MCC_CONFIG_BCHECK
	if (mcc_state->do_bounds_check)
		vpushv(vtop);
#endif
	r = intr(gv(MCC_RC_INT));
#if MCC_CONFIG_BCHECK
	if (mcc_state->do_bounds_check)
		o(0xe2800001 | (r << 16) | (r << 12));
#endif
	o(0xE04D0000 | (r << 12) | r);
#ifdef MCC_ARM_EABI
	if (align < 8)
		align = 8;
#else
	if (align < 4)
		align = 4;
#endif
	if (align & (align - 1))
		mcc_error("alignment is not a power of 2: %i", align);
	o(stuff_const(0xE3C0D000 | (r << 16), align - 1));
	vpop();
#if MCC_CONFIG_BCHECK
	if (mcc_state->do_bounds_check) {
		vpushi(0);
		vtop->r = MCC_TREG_R0;
		o(0xe1a0000d | (vtop->r << 12));
		vswap();
		vpush_helper_func(TOK___bound_new_region);
		vrott(3);
		gfunc_call(2);
		func_bound_add_epilog = 1;
	}
#endif
}

