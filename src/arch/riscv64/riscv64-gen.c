#define USING_GLOBALS
#include "mcc.h"
#include <assert.h>

#define UPPER(x) (((unsigned)(x) + 0x800u) & 0xfffff000)
#define LOW_OVERFLOW(x) UPPER(x)
#define SIGN7(x) ((((x) & 0xff) ^ 0x80) - 0x80)
#define SIGN11(x) ((((x) & 0xfff) ^ 0x800) - 0x800)

ST_DATA const char *const target_machine_defs =
		"__riscv\0"
		"__riscv_xlen 64\0"
		"__riscv_flen 64\0"
		"__riscv_div\0"
		"__riscv_mul\0"
		"__riscv_fdiv\0"
		"__riscv_fsqrt\0"
		"__riscv_float_abi_double\0";

#define XLEN 8

#define MCC_TREG_RA 17
#define MCC_TREG_SP 18

ST_DATA const int reg_classes[MCC_NB_REGS] = {
		MCC_RC_INT | MCC_RC_R(0),
		MCC_RC_INT | MCC_RC_R(1),
		MCC_RC_INT | MCC_RC_R(2),
		MCC_RC_INT | MCC_RC_R(3),
		MCC_RC_INT | MCC_RC_R(4),
		MCC_RC_INT | MCC_RC_R(5),
		MCC_RC_INT | MCC_RC_R(6),
		MCC_RC_INT | MCC_RC_R(7),
		MCC_RC_FLOAT | MCC_RC_F(0),
		MCC_RC_FLOAT | MCC_RC_F(1),
		MCC_RC_FLOAT | MCC_RC_F(2),
		MCC_RC_FLOAT | MCC_RC_F(3),
		MCC_RC_FLOAT | MCC_RC_F(4),
		MCC_RC_FLOAT | MCC_RC_F(5),
		MCC_RC_FLOAT | MCC_RC_F(6),
		MCC_RC_FLOAT | MCC_RC_F(7),
		0,
		1 << MCC_TREG_RA,
		1 << MCC_TREG_SP};

#if MCC_CONFIG_DIAG_RT >= 2
#define func_bound_offset (mcc_state->cg_func_bound_offset)
#define func_bound_ind (mcc_state->cg_func_bound_ind)
ST_DATA int func_bound_add_epilog;
#endif

static int ireg(int r) { MCC_TRACE("enter\n");
	if (r == MCC_TREG_RA)
		return 1;
	if (r == MCC_TREG_SP)
		return 2;
	assert(r >= 0 && r < 8);
	return r + 10;
}

static int is_ireg(int r) { MCC_TRACE("enter\n");
	return (unsigned)r < 8 || r == MCC_TREG_RA || r == MCC_TREG_SP;
}

static int freg(int r) { MCC_TRACE("enter\n");
	assert(r >= 8 && r < 16);
	return r - 8 + 10;
}

static int is_freg(int r) { MCC_TRACE("enter\n");
	return r >= 8 && r < 16;
}

ST_FUNC void o(unsigned int c) { MCC_TRACE("enter\n");
	int ind1 = ind + 4;
	if (nocode_wanted)
		return;
	if (ind1 > cur_text_section->data_allocated)
		section_realloc(cur_text_section, ind1);
	write32le(cur_text_section->data + ind, c);
	ind = ind1;
}

static void EIu(uint32_t opcode, uint32_t func3,
								uint32_t rd, uint32_t rs1, uint32_t imm) { MCC_TRACE("enter\n");
	o(opcode | (func3 << 12) | (rd << 7) | (rs1 << 15) | (imm << 20));
}

static void ER(uint32_t opcode, uint32_t func3,
							 uint32_t rd, uint32_t rs1, uint32_t rs2, uint32_t func7) { MCC_TRACE("enter\n");
	o(opcode | func3 << 12 | rd << 7 | rs1 << 15 | rs2 << 20 | func7 << 25);
}

static void EI(uint32_t opcode, uint32_t func3,
							 uint32_t rd, uint32_t rs1, uint32_t imm) { MCC_TRACE("enter\n");
	assert(!LOW_OVERFLOW(imm));
	EIu(opcode, func3, rd, rs1, imm);
}

static void ES(uint32_t opcode, uint32_t func3,
							 uint32_t rs1, uint32_t rs2, uint32_t imm) { MCC_TRACE("enter\n");
	assert(!LOW_OVERFLOW(imm));
	o(opcode | (func3 << 12) | ((imm & 0x1f) << 7) | (rs1 << 15) | (rs2 << 20) | ((imm >> 5) << 25));
}

ST_FUNC void gsym_addr(int t_, int a_) { MCC_TRACE("enter\n");
	uint32_t t = t_;
	uint32_t a = a_;
	while (t) { MCC_TRACE("br\n");
		unsigned char *ptr = cur_text_section->data + t;
		uint32_t next = read32le(ptr);
		uint32_t r = a - t, imm;
		if ((r + (1 << 21)) & ~((1U << 22) - 2))
			mcc_error("out-of-range branch chain");
		imm = (((r >> 12) & 0xff) << 12) | (((r >> 11) & 1) << 20) | (((r >> 1) & 0x3ff) << 21) | (((r >> 20) & 1) << 31);
		write32le(ptr, r == 4 ? 0x33 : 0x6f | imm);
		t = next;
	}
}

static int load_symofs(int r, SValue *sv, int forstore, int *new_fc) { MCC_TRACE("enter\n");
	int rr, doload = 0, large_addend = 0;
	int fc = sv->c.i, v = sv->r & VT_VALMASK;
	if (sv->r & VT_SYM) { MCC_TRACE("br\n");
		Sym label = {0};
		assert(v == VT_CONST);
		if (sv->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
			rr = is_ireg(r) ? ireg(r) : 5;
			greloca(cur_text_section, sv->sym, ind,
							R_RISCV_TPREL_HI20, sv->c.i);
			o(0x37 | (rr << 7));
			greloca(cur_text_section, sv->sym, ind,
							R_RISCV_TPREL_LO12_I, sv->c.i);
			EI(0x13, 0, rr, rr, 0);
			ER(0x33, 0, rr, rr, 4, 0);
			*new_fc = 0;
			return rr;
		}
		if (sv->sym->type.t & VT_STATIC) { MCC_TRACE("br\n");
			greloca(cur_text_section, sv->sym, ind,
							R_RISCV_PCREL_HI20, sv->c.i);
			*new_fc = 0;
		} else { MCC_TRACE("br\n");
			if (LOW_OVERFLOW(fc)) { MCC_TRACE("br\n");
				large_addend = 1;
			}
			greloca(cur_text_section, sv->sym, ind,
							R_RISCV_GOT_HI20, 0);
			doload = 1;
		}
		label.type.t = VT_VOID | VT_STATIC;
		if (!nocode_wanted)
			put_extern_sym(&label, cur_text_section, ind, 0);
		rr = is_ireg(r) ? ireg(r) : 5;
		o(0x17 | (rr << 7));
		greloca(cur_text_section, &label, ind,
						doload || !forstore
								? R_RISCV_PCREL_LO12_I
								: R_RISCV_PCREL_LO12_S,
						0);
		if (doload) { MCC_TRACE("br\n");
			EI(0x03, 3, rr, rr, 0);
			if (large_addend) { MCC_TRACE("br\n");
				o(0x37 | (6 << 7) | UPPER(fc));
				ER(0x33, 0, rr, rr, 6, 0);
				*new_fc = SIGN11(fc);
			}
		}
	} else if (v == VT_LOCAL || v == VT_LLOCAL) { MCC_TRACE("br\n");
		rr = 8;
		if (fc != sv->c.i)
			mcc_error("unimp: store(giant local off) (0x%lx)", (long)sv->c.i);
		if (LOW_OVERFLOW(fc)) { MCC_TRACE("br\n");
			rr = is_ireg(r) ? ireg(r) : 5;
			o(0x37 | (rr << 7) | UPPER(fc));
			ER(0x33, 0, rr, rr, 8, 0);
			*new_fc = SIGN11(fc);
		}
	} else
		mcc_error("uhh");
	return rr;
}

static void load_large_constant(int rr, int fc, uint32_t pi) { MCC_TRACE("enter\n");
	if (fc < 0)
		pi++;
	o(0x37 | (rr << 7) | UPPER(pi));
	EI(0x13, 0, rr, rr, SIGN11(pi));
	EI(0x13, 1, rr, rr, 12);
	EI(0x13, 0, rr, rr, SIGN11(((unsigned)fc + (1 << 19)) >> 20));
	EI(0x13, 1, rr, rr, 12);
	EI(0x13, 0, rr, rr, SIGN11(((unsigned)fc + (1 << 7)) >> 8));
	EI(0x13, 1, rr, rr, 8);
}

ST_FUNC void load(int r, SValue *sv) { MCC_TRACE("enter\n");
	int fr = sv->r;
	int v = fr & VT_VALMASK;
	int rr = is_ireg(r) ? ireg(r) : freg(r);
	int fc = sv->c.i;
	int bt = sv->type.t & VT_BTYPE;
	int align, size;
	if (fr & VT_LVAL) { MCC_TRACE("br\n");
		int func3, opcode = is_freg(r) ? 0x07 : 0x03, br;
		size = type_size(&sv->type, &align);
		assert(!is_freg(r) || bt == VT_FLOAT || bt == VT_DOUBLE);
		if (bt == VT_PTR || bt == VT_FUNC)
			size = MCC_PTR_SIZE;
		func3 = size == 1
								? 0
						: size == 2
								? 1
						: size == 4
								? 2
								: 3;
		if (size < 4 && !is_float(sv->type.t) && (sv->type.t & VT_UNSIGNED))
			func3 |= 4;
		if (v == VT_LOCAL || (fr & VT_SYM)) { MCC_TRACE("br\n");
			br = load_symofs(r, sv, 0, &fc);
		} else if (v < VT_CONST) { MCC_TRACE("br\n");
			br = ireg(v);
			fc = 0;
		} else if (v == VT_LLOCAL) { MCC_TRACE("br\n");
			int ptr = is_ireg(r) ? rr : 5;
			br = load_symofs(r, sv, 0, &fc);
			EI(0x03, 3, ptr, br, fc);
			br = ptr;
			fc = 0;
		} else if (v == VT_CONST) { MCC_TRACE("br\n");
			int64_t si = sv->c.i;
			si >>= 32;
			if (si != 0) { MCC_TRACE("br\n");
				load_large_constant(rr, fc, si);
				fc = SIGN7(fc);
			} else { MCC_TRACE("br\n");
				o(0x37 | (rr << 7) | UPPER(fc));
				fc = SIGN11(fc);
			}
			br = rr;
		} else { MCC_TRACE("br\n");
			mcc_error("unimp: load(non-local lval)");
		}
		EI(opcode, func3, rr, br, fc);
	} else if (v == VT_CONST) { MCC_TRACE("br\n");
		int rb = 0, do32bit = 8, zext = 0;
		if (is_float(sv->type.t) && bt != VT_LDOUBLE) { MCC_TRACE("br\n");
			uint64_t val = sv->c.i;
			int is_dbl = bt == VT_DOUBLE;
			if (val == 0) { MCC_TRACE("br\n");
				o(0x53 | (rr << 7) | ((unsigned)(0x78 | is_dbl) << 25));
				return;
			}
			if (is_dbl) { MCC_TRACE("br\n");
				load_large_constant(6, (int)val, (int)(val >> 32));
			} else { MCC_TRACE("br\n");
				if (LOW_OVERFLOW(fc))
					o(0x37 | (6 << 7) | UPPER(fc));
				EI(0x13 | 8, 0, 6, LOW_OVERFLOW(fc) ? 6 : 0, SIGN11(fc));
			}
			o(0x53 | (rr << 7) | (6 << 15) | ((unsigned)(0x78 | is_dbl) << 25));
			return;
		}
		assert(is_ireg(r) || bt == VT_LDOUBLE);
		if (fr & VT_SYM) { MCC_TRACE("br\n");
			rb = load_symofs(r, sv, 0, &fc);
			do32bit = 0;
		}
		if (do32bit && fc != sv->c.i) { MCC_TRACE("br\n");
			int64_t si = sv->c.i;
			si >>= 32;
			if (si != 0) { MCC_TRACE("br\n");
				load_large_constant(rr, fc, si);
				fc = SIGN7(fc);
				rb = rr;
				do32bit = 0;
			} else if (bt == VT_LLONG) { MCC_TRACE("br\n");
				zext = 1;
			}
		}
		if (LOW_OVERFLOW(fc))
			o(0x37 | (rr << 7) | UPPER(fc)), rb = rr;
		if (fc || (rr != rb) || do32bit || (fr & VT_SYM))
			EI(0x13 | do32bit, 0, rr, rb, SIGN11(fc));
		if (zext) { MCC_TRACE("br\n");
			EI(0x13, 1, rr, rr, 32);
			EI(0x13, 5, rr, rr, 32);
		}
	} else if (v == VT_LOCAL) { MCC_TRACE("br\n");
		int br = load_symofs(r, sv, 0, &fc);
		assert(is_ireg(r));
		EI(0x13, 0, rr, br, fc);
	} else if (v == VT_LLOCAL) { MCC_TRACE("br\n");
		int br = load_symofs(r, sv, 0, &fc);
		assert(is_ireg(r));
		EI(0x03, 3, rr, br, fc);
	} else if (v < VT_CONST) { MCC_TRACE("br\n");
		if (is_freg(r) && is_freg(v))
			ER(0x53, 0, rr, freg(v), freg(v), bt == VT_DOUBLE ? 0x11 : 0x10);
		else if (is_ireg(r) && is_ireg(v))
			EI(0x13, 0, rr, ireg(v), 0);
		else { MCC_TRACE("br\n");
			int func7 = is_ireg(r) ? 0x70 : 0x78;
			size = type_size(&sv->type, &align);
			if (size == 8)
				func7 |= 1;
			assert(size == 4 || size == 8);
			o(0x53 | (rr << 7) | ((is_freg(v) ? freg(v) : ireg(v)) << 15) | ((unsigned)func7 << 25));
		}
	} else if (v == VT_CMP) { MCC_TRACE("br\n");
		int op = vtop->cmp_op;
		int a = vtop->cmp_r & 0xff;
		int b = (vtop->cmp_r >> 8) & 0xff;
		int inv = 0;
		switch (op) { MCC_TRACE("br\n");
		case TOK_ULT:
		case TOK_UGE:
		case TOK_ULE:
		case TOK_UGT:
		case TOK_LT:
		case TOK_GE:
		case TOK_LE:
		case TOK_GT:
			if (op & 1) { MCC_TRACE("br\n");
				inv = 1;
				op--;
			}
			if ((op & 7) == 6) { MCC_TRACE("br\n");
				int t = a;
				a = b;
				b = t;
				inv ^= 1;
			}
			ER(0x33, (op > TOK_UGT) ? 2 : 3, rr, a, b, 0);
			if (inv)
				EI(0x13, 4, rr, rr, 1);
			break;
		case TOK_NE:
		case TOK_EQ:
			if (rr != a || b)
				ER(0x33, 0, rr, a, b, 0x20);
			if (op == TOK_NE)
				ER(0x33, 3, rr, 0, rr, 0);
			else
				EI(0x13, 3, rr, rr, 1);
			break;
		}
	} else if ((v & ~1) == VT_JMP) { MCC_TRACE("br\n");
		int t = v & 1;
		assert(is_ireg(r));
		EI(0x13, 0, rr, 0, t);
		gjmp_addr(ind + 8);
		gsym(fc);
		EI(0x13, 0, rr, 0, t ^ 1);
	} else
		mcc_error("unimp: load(non-const)");
}

ST_FUNC void store(int r, SValue *sv) { MCC_TRACE("enter\n");
	int fr = sv->r & VT_VALMASK;
	int rr = is_ireg(r) ? ireg(r) : freg(r), ptrreg;
	int fc = sv->c.i;
	int bt = sv->type.t & VT_BTYPE;
	int align, size = type_size(&sv->type, &align);
	assert(!is_float(bt) || is_freg(r) || bt == VT_LDOUBLE);
	if (bt == VT_LDOUBLE)
		size = align = 8;
	if (bt == VT_STRUCT)
		mcc_error("unimp: store(struct)");
	if (size > 8)
		mcc_error("unimp: large sized store");
	assert(sv->r & VT_LVAL);
	if (fr == VT_LOCAL || (sv->r & VT_SYM)) { MCC_TRACE("br\n");
		ptrreg = load_symofs(-1, sv, 1, &fc);
	} else if (fr < VT_CONST) { MCC_TRACE("br\n");
		ptrreg = ireg(fr);
		fc = 0;
	} else if (fr == VT_CONST) { MCC_TRACE("br\n");
		int64_t si = sv->c.i;
		ptrreg = 8;
		si >>= 32;
		if (si != 0) { MCC_TRACE("br\n");
			load_large_constant(ptrreg, fc, si);
			fc = SIGN7(fc);
		} else { MCC_TRACE("br\n");
			o(0x37 | (ptrreg << 7) | UPPER(fc));
			fc = SIGN11(fc);
		}
	} else
		mcc_error("implement me: %s(!local)", __FUNCTION__);
	ES(is_freg(r) ? 0x27 : 0x23,
		 size == 1
				 ? 0
		 : size == 2
				 ? 1
		 : size == 4
				 ? 2
				 : 3,
		 ptrreg, rr, fc);
}

static void gcall_or_jmp(int docall) { MCC_TRACE("enter\n");
	int tr = docall ? 1 : 5;
	if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
			((vtop->r & VT_SYM) && vtop->c.i == (int)vtop->c.i)) { MCC_TRACE("br\n");
		greloca(cur_text_section, vtop->sym, ind,
						R_RISCV_CALL_PLT, (int)vtop->c.i);
		o(0x17 | (tr << 7));
		EI(0x67, 0, tr, tr, 0);
	} else if (vtop->r < VT_CONST) { MCC_TRACE("br\n");
		int r = ireg(vtop->r);
		EI(0x67, 0, tr, r, 0);
	} else { MCC_TRACE("br\n");
		int r = MCC_TREG_RA;
		load(r, vtop);
		r = ireg(r);
		EI(0x67, 0, tr, r, 0);
	}
}

#if MCC_CONFIG_DIAG_RT >= 2

static void gen_bounds_call(int v) { MCC_TRACE("enter\n");
	Sym *sym = external_helper_sym(v);

	greloca(cur_text_section, sym, ind, R_RISCV_CALL_PLT, 0);
	o(0x17 | (1 << 7));
	EI(0x67, 0, 1, 1, 0);
}

static void gen_bounds_prolog(void) { MCC_TRACE("enter\n");
	func_bound_offset = lbounds_section->data_offset;
	func_bound_ind = ind;
	func_bound_add_epilog = 0;
	o(0x00000013);
	o(0x00000013);
	o(0x00000013);
	o(0x00000013);
}

static void gen_bounds_epilog(void) { MCC_TRACE("enter\n");
	addr_t saved_ind;
	Sym *sym_data;
	int offset_modified;
	Sym label = {0};

	if (!gen_bounds_epilog_head(func_bound_offset, &sym_data, &offset_modified))
		return;

	label.type.t = VT_VOID | VT_STATIC;
	if (offset_modified) { MCC_TRACE("br\n");
		saved_ind = ind;
		ind = func_bound_ind;
		put_extern_sym(&label, cur_text_section, ind, 0);
		greloca(cur_text_section, sym_data, ind, R_RISCV_GOT_HI20, 0);
		o(0x17 | (10 << 7));
		greloca(cur_text_section, &label, ind, R_RISCV_PCREL_LO12_I, 0);
		EI(0x03, 3, 10, 10, 0);
		gen_bounds_call(TOK___bound_local_new);
		ind = saved_ind;
		label.c = 0;
	}

	o(0xe02a1101);
	o(0xa82ae42e);
	put_extern_sym(&label, cur_text_section, ind, 0);
	greloca(cur_text_section, sym_data, ind, R_RISCV_GOT_HI20, 0);
	o(0x17 | (10 << 7));
	greloca(cur_text_section, &label, ind, R_RISCV_PCREL_LO12_I, 0);
	EI(0x03, 3, 10, 10, 0);
	gen_bounds_call(TOK___bound_local_delete);
	o(0x65a26502);
	o(0x61052542);
}
#endif

static void reg_pass_rec(CType *type, int *rc, int *fieldofs, int ofs) { MCC_TRACE("enter\n");
	if ((type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
		Sym *f;
		if (type->ref->type.t == VT_UNION)
			rc[0] = -1;
		else
			for (f = type->ref->next; f; f = f->next)
				reg_pass_rec(&f->type, rc, fieldofs, ofs + f->c);
	} else if (type->t & VT_ARRAY) { MCC_TRACE("br\n");
		if (type->ref->c < 0 || type->ref->c > 2)
			rc[0] = -1;
		else { MCC_TRACE("br\n");
			int a, sz = type_size(&type->ref->type, &a);
			reg_pass_rec(&type->ref->type, rc, fieldofs, ofs);
			if (rc[0] > 2 || (rc[0] == 2 && type->ref->c > 1))
				rc[0] = -1;
			else if (type->ref->c == 2 && rc[0] && rc[1] == MCC_RC_FLOAT) { MCC_TRACE("br\n");
				rc[++rc[0]] = MCC_RC_FLOAT;
				fieldofs[rc[0]] = ((ofs + sz) << 4) | (type->ref->type.t & VT_BTYPE);
			} else if (type->ref->c == 2)
				rc[0] = -1;
		}
	} else if (rc[0] == 2 || rc[0] < 0 || (type->t & VT_BTYPE) == VT_LDOUBLE)
		rc[0] = -1;
	else if (!rc[0] || rc[1] == MCC_RC_FLOAT || is_float(type->t)) { MCC_TRACE("br\n");
		rc[++rc[0]] = is_float(type->t) ? MCC_RC_FLOAT : MCC_RC_INT;
		fieldofs[rc[0]] = (ofs << 4) | ((type->t & VT_BTYPE) == VT_PTR ? VT_LLONG : type->t & VT_BTYPE);
	} else
		rc[0] = -1;
}

static void reg_pass(CType *type, int *prc, int *fieldofs, int named) { MCC_TRACE("enter\n");
	prc[0] = 0;
	reg_pass_rec(type, prc, fieldofs, 0);
	if (prc[0] <= 0 || !named) { MCC_TRACE("br\n");
		int align, size = type_size(type, &align);
		prc[0] = (size + 7) >> 3;
		prc[1] = prc[2] = MCC_RC_INT;
		fieldofs[1] = (0 << 4) | (size <= 1
																	? VT_BYTE
															: size <= 2
																	? VT_SHORT
															: size <= 4
																	? VT_INT
																	: VT_LLONG);
		fieldofs[2] = (8 << 4) | (size <= 9
																	? VT_BYTE
															: size <= 10
																	? VT_SHORT
															: size <= 12
																	? VT_INT
																	: VT_LLONG);
	}
}

ST_FUNC void gfunc_call(int nb_args) { MCC_TRACE("enter\n");
	int align, size, areg[2];
	int *info = mcc_malloc((nb_args + 1) * sizeof(int));
	int stack_adj = 0, tempspace = 0, stack_add, splitofs = 0;
	int old = (vtop[-nb_args].type.ref->f.func_type == FUNC_OLD);
	SValue *sv;
	Sym *sa;

#if MCC_CONFIG_DIAG_RT >= 2
	int bc_save = mcc_state->do_bounds_check;
	if (mcc_state->do_bounds_check)
		gbound_args(nb_args);
#endif

	areg[0] = 0;
	areg[1] = 8;
	sa = vtop[-nb_args].type.ref->next;
	for (int i = 0; i < nb_args; i++) { MCC_TRACE("br\n");
		int nregs, byref = 0, tempofs;
		int prc[3], fieldofs[3];
		sv = &vtop[1 + i - nb_args];
		sv->type.t &= ~VT_ARRAY;
		size = type_size(&sv->type, &align);
		if (size > 16) { MCC_TRACE("br\n");
			if (align < XLEN)
				align = XLEN;
			tempspace = (tempspace + align - 1) & -align;
			tempofs = tempspace;
			tempspace += size;
			size = align = 8;
			byref = 64 | (tempofs << 7);
		}
		reg_pass(&sv->type, prc, fieldofs, old || sa != 0);
		if (!old && !sa && align == 2 * XLEN && size <= 2 * XLEN)
			areg[0] = (areg[0] + 1) & ~1;
		nregs = prc[0];
		if (size == 0)
			info[i] = 0;
		else if ((prc[1] == MCC_RC_INT && areg[0] >= 8) || (prc[1] == MCC_RC_FLOAT && areg[1] >= 16) || (nregs == 2 && prc[1] == MCC_RC_FLOAT && prc[2] == MCC_RC_FLOAT && areg[1] >= 15) || (nregs == 2 && prc[1] != prc[2] && (areg[1] >= 16 || areg[0] >= 8))) { MCC_TRACE("br\n");
			info[i] = 32;
			if (align < XLEN)
				align = XLEN;
			stack_adj += (size + align - 1) & -align;
			if (!old && !sa)
				areg[0] = 8, areg[1] = 16;
		} else { MCC_TRACE("br\n");
			info[i] = areg[prc[1] - 1]++;
			if (!byref)
				info[i] |= (fieldofs[1] & VT_BTYPE) << 12;
			assert(!(fieldofs[1] >> 4));
			if (nregs == 2) { MCC_TRACE("br\n");
				if (prc[2] == MCC_RC_FLOAT || areg[0] < 8)
					info[i] |= (1 + areg[prc[2] - 1]++) << 7;
				else { MCC_TRACE("br\n");
					info[i] |= 16;
					stack_adj += 8;
				}
				if (!byref) { MCC_TRACE("br\n");
					assert((fieldofs[2] >> 4) < 2048);
					info[i] |= fieldofs[2] << (12 + 4);
				}
			}
		}
		info[i] |= byref;
		if (sa)
			sa = sa->next;
	}
	stack_adj = (stack_adj + 15) & -16;
	tempspace = (tempspace + 15) & -16;
	stack_add = stack_adj + tempspace;

	if (stack_add) { MCC_TRACE("br\n");
		if (stack_add >= 0x800) { MCC_TRACE("br\n");
			o(0x37 | (5 << 7) | UPPER(-stack_add));
			EI(0x13, 0, 5, 5, SIGN11(-stack_add));
			ER(0x33, 0, 2, 2, 5, 0);
		} else
			EI(0x13, 0, 2, 2, -stack_add);
		for (int i = 0, ofs = 0; i < nb_args; i++) { MCC_TRACE("br\n");
			if (info[i] & (64 | 32)) { MCC_TRACE("br\n");
				vrotb(nb_args - i);
				size = type_size(&vtop->type, &align);
				if (info[i] & 64) { MCC_TRACE("br\n");
					vset(&char_pointer_type, MCC_TREG_SP, 0);
					vpushi(stack_adj + (info[i] >> 7));
					gen_op('+');
					vpushv(vtop);
					vrott(3);
					indir();
					vtop->type = vtop[-1].type;
					vswap();
					vstore();
					vpop();
					size = align = 8;
				}
				if (info[i] & 32) { MCC_TRACE("br\n");
					if (align < XLEN)
						align = XLEN;
					vset(&char_pointer_type, MCC_TREG_SP, 0);
					ofs = (ofs + align - 1) & -align;
					vpushi(ofs);
					gen_op('+');
					indir();
					vtop->type = vtop[-1].type;
					vswap();
					vstore();
					vtop->r = vtop->r2 = VT_CONST;
					ofs += size;
				}
				vrott(nb_args - i);
			} else if (info[i] & 16) { MCC_TRACE("br\n");
				assert(!splitofs);
				splitofs = ofs;
				ofs += 8;
			}
		}
	}
	for (int i = 0; i < nb_args; i++) { MCC_TRACE("br\n");
		int ii = info[nb_args - 1 - i], r = ii, r2 = r;
		if (!(r & 32)) { MCC_TRACE("br\n");
			CType origtype;
			int loadt;
			r &= 15;
			r2 = r2 & 64 ? 0 : (r2 >> 7) & 31;
			assert(r2 <= 16);
			vrotb(i + 1);
			origtype = vtop->type;
			size = type_size(&vtop->type, &align);
			if (size == 0)
				goto done;
			loadt = vtop->type.t & VT_BTYPE;
			if (loadt == VT_STRUCT) { MCC_TRACE("br\n");
				loadt = (ii >> 12) & VT_BTYPE;
			}
			if (info[nb_args - 1 - i] & 16) { MCC_TRACE("br\n");
				assert(!r2);
				r2 = 1 + MCC_TREG_RA;
			}
			if (loadt == VT_LDOUBLE) { MCC_TRACE("br\n");
				assert(r2);
				r2--;
			} else if (r2) { MCC_TRACE("br\n");
				test_lvalue();
				vpushv(vtop);
			}
			vtop->type.t = loadt | (vtop->type.t & VT_UNSIGNED);
			gv(r < 8 ? MCC_RC_R(r) : MCC_RC_F(r - 8));
			vtop->type = origtype;

			if (r2 && loadt != VT_LDOUBLE) { MCC_TRACE("br\n");
				r2--;
				assert(r2 < 16 || r2 == MCC_TREG_RA);
				vswap();
				gaddrof();
				vtop->type = char_pointer_type;
				vpushi(ii >> 20);
#if MCC_CONFIG_DIAG_RT >= 2
				if ((origtype.t & VT_BTYPE) == VT_STRUCT)
					mcc_state->do_bounds_check = 0;
#endif
				gen_op('+');
#if MCC_CONFIG_DIAG_RT >= 2
				mcc_state->do_bounds_check = bc_save;
#endif
				indir();
				vtop->type = origtype;
				loadt = vtop->type.t & VT_BTYPE;
				if (loadt == VT_STRUCT) { MCC_TRACE("br\n");
					loadt = (ii >> 16) & VT_BTYPE;
				}
				save_reg_upstack(r2, 1);
				vtop->type.t = loadt | (vtop->type.t & VT_UNSIGNED);
				load(r2, vtop);
				assert(r2 < VT_CONST);
				vtop--;
				vtop->r2 = r2;
			}
			if (info[nb_args - 1 - i] & 16) { MCC_TRACE("br\n");
				ES(0x23, 3, 2, ireg(vtop->r2), splitofs);
				vtop->r2 = VT_CONST;
			} else if (loadt == VT_LDOUBLE && vtop->r2 != r2) { MCC_TRACE("br\n");
				assert(vtop->r2 <= 7 && r2 <= 7);
				EI(0x13, 0, ireg(r2), ireg(vtop->r2), 0);
				vtop->r2 = r2;
			}
		done:
			vrott(i + 1);
		}
	}
	vrotb(nb_args + 1);
	save_regs(nb_args + 1);
	gcall_or_jmp(1);
	vtop -= nb_args + 1;
	if (stack_add) { MCC_TRACE("br\n");
		if (stack_add >= 0x800) { MCC_TRACE("br\n");
			o(0x37 | (5 << 7) | UPPER(stack_add));
			EI(0x13, 0, 5, 5, SIGN11(stack_add));
			ER(0x33, 0, 2, 2, 5, 0);
		} else
			EI(0x13, 0, 2, 2, stack_add);
	}
	mcc_free(info);
}

#define func_sub_sp_offset (mcc_state->cg_func_sub_sp_offset)
#define num_va_regs (mcc_state->cg_num_va_regs)
#define func_va_list_ofs (mcc_state->cg_func_va_list_ofs)

ST_FUNC void gfunc_prolog(Sym *func_sym) { MCC_TRACE("enter\n");
	CType *func_type = &func_sym->type;
	int addr, align, size;
	int param_addr = 0;
	int areg[2];
	Sym *sym;
	CType *type;

	sym = func_type->ref;
	loc = -16;
	func_sub_sp_offset = ind;
	ind += 5 * 4;

	areg[0] = 0, areg[1] = 0;
	addr = 0;
	size = type_size(&func_vt, &align);
	if (size > 2 * XLEN) { MCC_TRACE("br\n");
		loc -= 8;
		func_vc = loc;
		ES(0x23, 3, 8, 10 + areg[0]++, loc);
	}
	while ((sym = sym->next) != NULL) { MCC_TRACE("br\n");
		int byref = 0;
		int regcount;
		int prc[3], fieldofs[3];
		type = &sym->type;
		size = type_size(type, &align);
		if (size > 2 * XLEN) { MCC_TRACE("br\n");
			type = &char_pointer_type;
			size = align = byref = 8;
		}
		reg_pass(type, prc, fieldofs, 1);
		regcount = prc[0];
		if (areg[prc[1] - 1] >= 8 || (regcount == 2 && ((prc[1] == MCC_RC_FLOAT && prc[2] == MCC_RC_FLOAT && areg[1] >= 7) || (prc[1] != prc[2] && (areg[1] >= 8 || areg[0] >= 8))))) { MCC_TRACE("br\n");
			if (align < XLEN)
				align = XLEN;
			addr = (addr + align - 1) & -align;
			param_addr = addr;
			addr += size;
		} else { MCC_TRACE("br\n");
			loc -= regcount * 8;
			param_addr = loc;
			for (int i = 0; i < regcount; i++) { MCC_TRACE("br\n");
				if (areg[prc[1 + i] - 1] >= 8) { MCC_TRACE("br\n");
					assert(i == 1 && regcount == 2 && !(addr & 7));
					EI(0x03, 3, 5, 8, addr);
					addr += 8;
					ES(0x23, 3, 8, 5, loc + i * 8);
				} else if (prc[1 + i] == MCC_RC_FLOAT) { MCC_TRACE("br\n");
					ES(0x27, (size / regcount) == 4 ? 2 : 3, 8, 10 + areg[1]++, loc + (fieldofs[i + 1] >> 4));
				} else { MCC_TRACE("br\n");
					ES(0x23, 3, 8, 10 + areg[0]++, loc + i * 8);
				}
			}
		}
		gfunc_set_param(sym, param_addr, byref);
	}
	func_va_list_ofs = addr;
	num_va_regs = 0;
	if (func_var) { MCC_TRACE("br\n");
		for (; areg[0] < 8; areg[0]++) { MCC_TRACE("br\n");
			num_va_regs++;
			ES(0x23, 3, 8, 10 + areg[0], -8 + num_va_regs * 8);
		}
	}
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		gen_bounds_prolog();
#endif
}

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret,
											 int *ret_align, int *regsize) { MCC_TRACE("enter\n");
	int align, size = type_size(vt, &align), nregs;
	int prc[3], fieldofs[3];
	*ret_align = 1;
	*regsize = 8;
	if (size > 16)
		return 0;
	reg_pass(vt, prc, fieldofs, 1);
	nregs = prc[0];
	if (nregs == 2 && prc[1] != prc[2])
		return -1;
	if (prc[1] == MCC_RC_FLOAT) { MCC_TRACE("br\n");
		*regsize = size / nregs;
	}
	ret->t = fieldofs[1] & VT_BTYPE;
	ret->ref = NULL;
	return nregs;
}

ST_FUNC void arch_transfer_ret_regs(int aftercall) { MCC_TRACE("enter\n");
	int prc[3], fieldofs[3];
	reg_pass(&vtop->type, prc, fieldofs, 1);
	assert(prc[0] == 2 && prc[1] != prc[2] && !(fieldofs[1] >> 4));
	assert(vtop->r == (VT_LOCAL | VT_LVAL));
	vpushv(vtop);
	vtop->type.t = fieldofs[1] & VT_BTYPE;
	(aftercall ? store : load)(prc[1] == MCC_RC_INT ? REG_IRET : REG_FRET, vtop);
	vtop->c.i += fieldofs[2] >> 4;
	vtop->type.t = fieldofs[2] & VT_BTYPE;
	(aftercall ? store : load)(prc[2] == MCC_RC_INT ? REG_IRET : REG_FRET, vtop);
	vtop--;
}

ST_FUNC void gfunc_epilog(void) { MCC_TRACE("enter\n");
	int v, saved_ind, d, large_ofs_ind;

#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		gen_bounds_epilog();
#endif

	loc = (loc - num_va_regs * 8);
	d = v = (-loc + 15) & -16;

	EI(0x13, 0, 2, 8, num_va_regs * 8);
	EI(0x03, 3, 1, 8, -8);
	EI(0x03, 3, 8, 8, -16);
	EI(0x67, 0, 0, 1, 0);

	large_ofs_ind = ind;
	if (v >= (1 << 11)) { MCC_TRACE("br\n");
		d = 16;
		EI(0x13, 0, 8, 2, d - num_va_regs * 8);
		o(0x37 | (5 << 7) | UPPER(v - 16));
		EI(0x13, 0, 5, 5, SIGN11(v - 16));
		ER(0x33, 0, 2, 2, 5, 0x20);
		gjmp_addr(func_sub_sp_offset + 5 * 4);
	}
	saved_ind = ind;

	ind = func_sub_sp_offset;
	EI(0x13, 0, 2, 2, -d);
	ES(0x23, 3, 2, 1, d - 8 - num_va_regs * 8);
	ES(0x23, 3, 2, 8, d - 16 - num_va_regs * 8);
	if (v < (1 << 11))
		EI(0x13, 0, 8, 2, d - num_va_regs * 8);
	else
		gjmp_addr(large_ofs_ind);
	if ((ind - func_sub_sp_offset) != 5 * 4)
		EI(0x13, 0, 0, 0, 0);
	ind = saved_ind;
}

ST_FUNC void gen_va_start(void) { MCC_TRACE("enter\n");
	vtop--;
	vset(&char_pointer_type, VT_LOCAL, func_va_list_ofs);
}

ST_FUNC void gen_fill_nops(int bytes) { MCC_TRACE("enter\n");
	if ((bytes & 3))
		mcc_error("alignment of code section not multiple of 4");
	while (bytes > 0) { MCC_TRACE("br\n");
		EI(0x13, 0, 0, 0, 0);
		bytes -= 4;
	}
}

ST_FUNC int gjmp(int t) { MCC_TRACE("enter\n");
	if (nocode_wanted)
		return t;
	o(t);
	return ind - 4;
}

ST_FUNC void gjmp_addr(int a) { MCC_TRACE("enter\n");
	uint32_t r = a - ind, imm;
	if ((r + (1 << 21)) & ~((1U << 22) - 2)) { MCC_TRACE("br\n");
		o(0x17 | (5 << 7) | UPPER(r));
		r = SIGN11(r);
		EI(0x67, 0, 0, 5, r);
	} else { MCC_TRACE("br\n");
		imm = (((r >> 12) & 0xff) << 12) | (((r >> 11) & 1) << 20) | (((r >> 1) & 0x3ff) << 21) | (((r >> 20) & 1) << 31);
		o(0x6f | imm);
	}
}

ST_FUNC int gjmp_cond(int op, int t) { MCC_TRACE("enter\n");
	int tmp;
	int a = vtop->cmp_r & 0xff;
	int b = (vtop->cmp_r >> 8) & 0xff;
	switch (op) { MCC_TRACE("br\n");
	case TOK_ULT:
		op = 6;
		break;
	case TOK_UGE:
		op = 7;
		break;
	case TOK_ULE:
		op = 7;
		tmp = a;
		a = b;
		b = tmp;
		break;
	case TOK_UGT:
		op = 6;
		tmp = a;
		a = b;
		b = tmp;
		break;
	case TOK_LT:
		op = 4;
		break;
	case TOK_GE:
		op = 5;
		break;
	case TOK_LE:
		op = 5;
		tmp = a;
		a = b;
		b = tmp;
		break;
	case TOK_GT:
		op = 4;
		tmp = a;
		a = b;
		b = tmp;
		break;
	case TOK_NE:
		op = 1;
		break;
	case TOK_EQ:
		op = 0;
		break;
	}
	o(0x63 | (op ^ 1) << 12 | a << 15 | b << 20 | 8 << 7);
	return gjmp(t);
}

static void riscv64_ubsan_div0(int b) { MCC_TRACE("enter\n");
	if (!mcc_state->do_sanitize_undefined || nocode_wanted)
		return;
	o(0x63 | 1u << 12 | (uint32_t)b << 15 | 8u << 7);
	o(0x00100073);
}

void gen_ubsan_nullptr(void) { MCC_TRACE("enter\n");
	if (!mcc_state->do_sanitize_undefined || nocode_wanted)
		return;
	if ((vtop->r & VT_VALMASK) >= VT_CONST)
		return;
	o(0x63 | 1u << 12 | (uint32_t)ireg(vtop->r & VT_VALMASK) << 15 | 8u << 7);
	o(0x00100073);
}

static void riscv64_ubsan_shift(int cnt, uint32_t width) { MCC_TRACE("enter\n");
	if (!mcc_state->do_sanitize_undefined || nocode_wanted)
		return;
	o(0x13 | 3u << 12 | 5u << 7 | (uint32_t)cnt << 15 | width << 20);
	o(0x63 | 1u << 12 | 5u << 15 | 8u << 7);
	o(0x00100073);
}

static void riscv64_ubsan_addsub_pre(int a, int b) { MCC_TRACE("enter\n");
	o(0x13 | 5u << 7 | (uint32_t)a << 15);
	o(0x13 | 6u << 7 | (uint32_t)b << 15);
}

static void riscv64_ubsan_addsub_post(int op, int d) { MCC_TRACE("enter\n");
	o(0x33 | 4u << 12 | 7u << 7 | 5u << 15 | (uint32_t)d << 20);
	o(0x33 | 4u << 12 | 5u << 7 | 5u << 15 | 6u << 20);
	if (op == '+')
		o(0x13 | 4u << 12 | 5u << 7 | 5u << 15 | 0xfffu << 20);
	o(0x33 | 7u << 12 | 5u << 7 | 5u << 15 | 7u << 20);
	o(0x63 | 5u << 12 | 5u << 15 | 8u << 7);
	o(0x00100073);
}

static void riscv64_ubsan_mul_ovf(int ll, int d) { MCC_TRACE("enter\n");
	if (ll == 8) { MCC_TRACE("br\n");
		o(0x33 | 0u << 12 | 7u << 7 | 5u << 15 | 6u << 20 | 1u << 25);
		o(0x63 | 0u << 12 | 7u << 15 | (uint32_t)d << 20 | 8u << 7);
	} else { MCC_TRACE("br\n");
		o(0x33 | 1u << 12 | 7u << 7 | 5u << 15 | 6u << 20 | 1u << 25);
		o(0x13 | 5u << 12 | 5u << 7 | (uint32_t)d << 15 | 0x43fu << 20);
		o(0x63 | 0u << 12 | 7u << 15 | 5u << 20 | 8u << 7);
	}
	o(0x00100073);
}

static void gen_opil(int op, int ll) { MCC_TRACE("enter\n");
	int a, b, d;
	int func3 = 0;
	ll = ll ? 0 : 8;
	if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
		int fc = vtop->c.i;
		if (fc == vtop->c.i && !LOW_OVERFLOW(fc)) { MCC_TRACE("br\n");
			int cll = 0;
			int m = ll ? 31 : 63;
			vswap();
			gv(MCC_RC_INT);
			a = ireg(vtop[0].r);
			--vtop;
			d = get_reg(MCC_RC_INT);
			++vtop;
			vswap();
			switch (op) { MCC_TRACE("br\n");
			case '-':
				if (fc <= -(1 << 11))
					break;
				fc = -fc;
			case '+':
				func3 = 0;
				cll = ll;
			do_cop:
				EI(0x13 | cll, func3, ireg(d), a, fc);
				--vtop;
				if (op >= TOK_ULT && op <= TOK_GT) { MCC_TRACE("br\n");
					vset_VT_CMP(TOK_NE);
					vtop->cmp_r = ireg(d) | 0 << 8;
				} else
					vtop[0].r = d;
				return;
			case TOK_LE:
				if (fc >= (1 << 11) - 1)
					break;
				++fc;
			case TOK_LT:
				func3 = 2;
				goto do_cop;
			case TOK_ULE:
				if (fc >= (1 << 11) - 1 || fc == -1)
					break;
				++fc;
			case TOK_ULT:
				func3 = 3;
				goto do_cop;
			case '^':
				func3 = 4;
				goto do_cop;
			case '|':
				func3 = 6;
				goto do_cop;
			case '&':
				func3 = 7;
				goto do_cop;
			case TOK_SHL:
				func3 = 1;
				cll = ll;
				fc &= m;
				goto do_cop;
			case TOK_SHR:
				func3 = 5;
				cll = ll;
				fc &= m;
				goto do_cop;
			case TOK_SAR:
				func3 = 5;
				cll = ll;
				fc = 1024 | (fc & m);
				goto do_cop;

			case TOK_UGE:
			case TOK_UGT:
			case TOK_GE:
			case TOK_GT:
				gen_opil(op - 1, !ll);
				vtop->cmp_op ^= 1;
				return;

			case TOK_NE:
			case TOK_EQ:
				if (fc)
					gen_opil('-', !ll), a = ireg(vtop++->r);
				--vtop;
				vset_VT_CMP(op);
				vtop->cmp_r = a | 0 << 8;
				return;
			}
		}
	}
	gv2(MCC_RC_INT, MCC_RC_INT);
	a = ireg(vtop[-1].r);
	b = ireg(vtop[0].r);
	int uns = (vtop[-1].type.t & VT_UNSIGNED) != 0;
	vtop -= 2;
	d = get_reg(MCC_RC_INT);
	vtop++;
	vtop[0].r = d;
	d = ireg(d);
	switch (op) { MCC_TRACE("br\n");
	default:
		if (op >= TOK_ULT && op <= TOK_GT) { MCC_TRACE("br\n");
			vset_VT_CMP(op);
			vtop->cmp_r = a | b << 8;
			break;
		}
		mcc_error("implement me: %s(%s)", __FUNCTION__, get_tok_str(op, NULL));
		break;

	case '+': {
		int chk = !uns && mcc_state->do_sanitize_undefined && !nocode_wanted;
		if (chk)
			riscv64_ubsan_addsub_pre(a, b);
		ER(0x33 | ll, 0, d, a, b, 0);
		if (chk)
			riscv64_ubsan_addsub_post('+', d);
		break;
	}
	case '-': {
		int chk = !uns && mcc_state->do_sanitize_undefined && !nocode_wanted;
		if (chk)
			riscv64_ubsan_addsub_pre(a, b);
		ER(0x33 | ll, 0, d, a, b, 0x20);
		if (chk)
			riscv64_ubsan_addsub_post('-', d);
		break;
	}
	case TOK_SAR:
		riscv64_ubsan_shift(b, ll ? 32 : 64);
		ER(0x33 | ll | ll, 5, d, a, b, 0x20);
		break;
	case TOK_SHR:
		riscv64_ubsan_shift(b, ll ? 32 : 64);
		ER(0x33 | ll | ll, 5, d, a, b, 0);
		break;
	case TOK_SHL:
		riscv64_ubsan_shift(b, ll ? 32 : 64);
		ER(0x33 | ll, 1, d, a, b, 0);
		break;
	case '*': {
		int chk = !uns && mcc_state->do_sanitize_undefined && !nocode_wanted;
		if (chk)
			riscv64_ubsan_addsub_pre(a, b);
		ER(0x33 | ll, 0, d, a, b, 1);
		if (chk)
			riscv64_ubsan_mul_ovf(ll, d);
		break;
	}
	case '/':
	case TOK_PDIV:
		riscv64_ubsan_div0(b);
		ER(0x33 | ll, 4, d, a, b, 1);
		break;
	case '&':
		ER(0x33, 7, d, a, b, 0);
		break;
	case '^':
		ER(0x33, 4, d, a, b, 0);
		break;
	case '|':
		ER(0x33, 6, d, a, b, 0);
		break;
	case '%':
		riscv64_ubsan_div0(b);
		ER(ll ? 0x3b : 0x33, 6, d, a, b, 1);
		break;
	case TOK_UMOD:
		riscv64_ubsan_div0(b);
		ER(0x33 | ll, 7, d, a, b, 1);
		break;
	case TOK_UDIV:
		riscv64_ubsan_div0(b);
		ER(0x33 | ll, 5, d, a, b, 1);
		break;
	}
}

ST_FUNC void gen_opi(int op) { MCC_TRACE("enter\n");
	gen_opil(op, 0);
}

ST_FUNC void gen_opl(int op) { MCC_TRACE("enter\n");
	gen_opil(op, 1);
}

ST_FUNC void gen_opf(int op) { MCC_TRACE("enter\n");
	int rs1, rs2, rd, dbl, invert;
	if (vtop[0].type.t == VT_LDOUBLE) { MCC_TRACE("br\n");
		CType type = vtop[0].type;
		int func = 0;
		int cond = -1;
		switch (op) { MCC_TRACE("br\n");
		case '*':
			func = TOK___multf3;
			break;
		case '+':
			func = TOK___addtf3;
			break;
		case '-':
			func = TOK___subtf3;
			break;
		case '/':
			func = TOK___divtf3;
			break;
		case TOK_EQ:
			func = TOK___eqtf2;
			cond = 1;
			break;
		case TOK_NE:
			func = TOK___netf2;
			cond = 0;
			break;
		case TOK_LT:
			func = TOK___lttf2;
			cond = 10;
			break;
		case TOK_GE:
			func = TOK___getf2;
			cond = 11;
			break;
		case TOK_LE:
			func = TOK___letf2;
			cond = 12;
			break;
		case TOK_GT:
			func = TOK___gttf2;
			cond = 13;
			break;
		default:
			assert(0);
			break;
		}
		vpush_helper_func(func);
		vrott(3);
		gfunc_call(2);
		vpushi(0);
		vtop->r = REG_IRET;
		vtop->r2 = cond < 0 ? MCC_TREG_R(1) : VT_CONST;
		if (cond < 0)
			vtop->type = type;
		else { MCC_TRACE("br\n");
			vpushi(0);
			gen_opil(op, 1);
		}
		return;
	}

	gv2(MCC_RC_FLOAT, MCC_RC_FLOAT);
	assert(vtop->type.t == VT_DOUBLE || vtop->type.t == VT_FLOAT);
	dbl = vtop->type.t == VT_DOUBLE;
	rs1 = freg(vtop[-1].r);
	rs2 = freg(vtop->r);
	vtop--;
	invert = 0;
	switch (op) { MCC_TRACE("br\n");
	default:
		assert(0);
	case '+':
		op = 0;
	arithop:
		rd = get_reg(MCC_RC_FLOAT);
		vtop->r = rd;
		rd = freg(rd);
		ER(0x53, 7, rd, rs1, rs2, dbl | (op << 2));
		break;
	case '-':
		op = 1;
		goto arithop;
	case '*':
		op = 2;
		goto arithop;
	case '/':
		op = 3;
		goto arithop;
	case TOK_EQ:
		op = 2;
	cmpop:
		rd = get_reg(MCC_RC_INT);
		vtop->r = rd;
		rd = ireg(rd);
		ER(0x53, op, rd, rs1, rs2, dbl | 0x50);
		if (invert)
			EI(0x13, 4, rd, rd, 1);

		vset_VT_CMP(TOK_NE);
		vtop->cmp_r = rd | (0 << 8);
		break;
	case TOK_NE:
		invert = 1;
		op = 2;
		goto cmpop;
	case TOK_LT:
		op = 1;
		goto cmpop;
	case TOK_LE:
		op = 0;
		goto cmpop;
	case TOK_GT:
		op = 1;
		rd = rs1, rs1 = rs2, rs2 = rd;
		goto cmpop;
	case TOK_GE:
		op = 0;
		rd = rs1, rs1 = rs2, rs2 = rd;
		goto cmpop;
	}
}

ST_FUNC void gen_cvt_csti(int t) { MCC_TRACE("enter\n");
	int r = ireg(gv(MCC_RC_INT));
	if ((t & VT_BTYPE) == VT_SHORT) { MCC_TRACE("br\n");
		if (t & VT_UNSIGNED) { MCC_TRACE("br\n");
			EI(0x13, 1, r, r, 48);
			EI(0x13, 5, r, r, 48);
		} else { MCC_TRACE("br\n");
			EI(0x13, 1, r, r, 48);
			EIu(0x13, 5, r, r, 0x400 | 48);
		}
	} else { MCC_TRACE("br\n");
		if (t & VT_UNSIGNED) { MCC_TRACE("br\n");
			EI(0x13, 7, r, r, 0xff);
		} else { MCC_TRACE("br\n");
			EI(0x13, 1, r, r, 56);
			EIu(0x13, 5, r, r, 0x400 | 56);
		}
	}
}

ST_FUNC void gen_cvt_sxtw(void) { MCC_TRACE("enter\n");
	int r = ireg(gv(MCC_RC_INT));
	EI(0x1b, 0, r, r, 0);
}

ST_FUNC void gen_cvt_itof(int t) { MCC_TRACE("enter\n");
	int rr = ireg(gv(MCC_RC_INT)), dr;
	int u = vtop->type.t & VT_UNSIGNED;
	int l = (vtop->type.t & VT_BTYPE) == VT_LLONG;
	if (t == VT_LDOUBLE) { MCC_TRACE("br\n");
		int func = l ? (u ? TOK___floatunditf : TOK___floatditf) : (u ? TOK___floatunsitf : TOK___floatsitf);
		vpush_helper_func(func);
		vrott(2);
		gfunc_call(1);
		vpushi(0);
		vtop->type.t = t;
		vtop->r = REG_IRET;
		vtop->r2 = MCC_TREG_R(1);
	} else { MCC_TRACE("br\n");
		vtop--;
		dr = get_reg(MCC_RC_FLOAT);
		vtop++;
		vtop->r = dr;
		dr = freg(dr);
		EIu(0x53, 7, dr, rr, ((0x68 | (t == VT_DOUBLE ? 1 : 0)) << 5) | (u ? 1 : 0) | (l ? 2 : 0));
	}
}

ST_FUNC void gen_cvt_ftoi(int t) { MCC_TRACE("enter\n");
	int ft = vtop->type.t & VT_BTYPE;
	int l = (t & VT_BTYPE) == VT_LLONG;
	int u = t & VT_UNSIGNED;
	if (ft == VT_LDOUBLE) { MCC_TRACE("br\n");
		int func = l ? (u ? TOK___fixunstfdi : TOK___fixtfdi) : (u ? TOK___fixunstfsi : TOK___fixtfsi);
		vpush_helper_func(func);
		vrott(2);
		gfunc_call(1);
		vpushi(0);
		vtop->type.t = t;
		vtop->r = REG_IRET;
	} else { MCC_TRACE("br\n");
		int rr = freg(gv(MCC_RC_FLOAT)), dr;
		vtop--;
		dr = get_reg(MCC_RC_INT);
		vtop++;
		vtop->r = dr;
		dr = ireg(dr);
		EIu(0x53, 1, dr, rr, ((0x60 | (ft == VT_DOUBLE ? 1 : 0)) << 5) | (u ? 1 : 0) | (l ? 2 : 0));
	}
}

ST_FUNC void gen_cvt_ftof(int dt) { MCC_TRACE("enter\n");
	int st = vtop->type.t & VT_BTYPE, rs, rd;
	dt &= VT_BTYPE;
	if (st == dt)
		return;
	if (dt == VT_LDOUBLE || st == VT_LDOUBLE) { MCC_TRACE("br\n");
		int func = (dt == VT_LDOUBLE)
									 ? (st == VT_FLOAT ? TOK___extendsftf2 : TOK___extenddftf2)
									 : (dt == VT_FLOAT ? TOK___trunctfsf2 : TOK___trunctfdf2);
		save_regs(1);
		if (dt == VT_LDOUBLE)
			gv(MCC_RC_F(0));
		else { MCC_TRACE("br\n");
			gv(MCC_RC_R(0));
			assert(vtop->r2 < 7);
			if (vtop->r2 != 1 + vtop->r) { MCC_TRACE("br\n");
				EI(0x13, 0, ireg(vtop->r) + 1, ireg(vtop->r2), 0);
				vtop->r2 = 1 + vtop->r;
			}
		}
		vpush_helper_func(func);
		gcall_or_jmp(1);
		vtop -= 2;
		vpushi(0);
		vtop->type.t = dt;
		if (dt == VT_LDOUBLE)
			vtop->r = REG_IRET, vtop->r2 = REG_IRET + 1;
		else
			vtop->r = REG_FRET;
	} else { MCC_TRACE("br\n");
		assert(dt == VT_FLOAT || dt == VT_DOUBLE);
		assert(st == VT_FLOAT || st == VT_DOUBLE);
		rs = gv(MCC_RC_FLOAT);
		rd = get_reg(MCC_RC_FLOAT);
		if (dt == VT_DOUBLE)
			EI(0x53, 0, freg(rd), freg(rs), 0x21 << 5);
		else
			EI(0x53, 7, freg(rd), freg(rs), (0x20 << 5) | 1);
		vtop->r = rd;
	}
}

ST_FUNC void gen_increment_tcov(SValue *sv) { MCC_TRACE("enter\n");
	int r1, r2;
	Sym label = {0};
	label.type.t = VT_VOID | VT_STATIC;

	vpushv(sv);
	vtop->r = r1 = get_reg(MCC_RC_INT);
	r2 = get_reg(MCC_RC_INT);
	r1 = ireg(r1);
	r2 = ireg(r2);
	greloca(cur_text_section, sv->sym, ind, R_RISCV_PCREL_HI20, 0);
	put_extern_sym(&label, cur_text_section, ind, 0);
	o(0x17 | (r1 << 7));
	greloca(cur_text_section, &label, ind, R_RISCV_PCREL_LO12_I, 0);
	EI(0x03, 3, r2, r1, 0);
	EI(0x13, 0, r2, r2, 1);
	greloca(cur_text_section, sv->sym, ind, R_RISCV_PCREL_HI20, 0);
	label.c = 0;
	put_extern_sym(&label, cur_text_section, ind, 0);
	o(0x17 | (r1 << 7));
	greloca(cur_text_section, &label, ind, R_RISCV_PCREL_LO12_S, 0);
	ES(0x23, 3, r1, r2, 0);
	vpop();
}

ST_FUNC void ggoto(void) { MCC_TRACE("enter\n");
	gcall_or_jmp(0);
	vtop--;
}

ST_FUNC void gen_vla_sp_save(int addr) { MCC_TRACE("enter\n");
	if (LOW_OVERFLOW(addr)) { MCC_TRACE("br\n");
		o(0x37 | (5 << 7) | UPPER(addr));
		ER(0x33, 0, 5, 5, 8, 0);
		ES(0x23, 3, 5, 2, SIGN11(addr));
	} else
		ES(0x23, 3, 8, 2, addr);
}

ST_FUNC void gen_vla_sp_restore(int addr) { MCC_TRACE("enter\n");
	if (LOW_OVERFLOW(addr)) { MCC_TRACE("br\n");
		o(0x37 | (5 << 7) | UPPER(addr));
		ER(0x33, 0, 5, 5, 8, 0);
		EI(0x03, 3, 2, 5, SIGN11(addr));
	} else
		EI(0x03, 3, 2, 8, addr);
}

ST_FUNC void gen_vla_alloc(CType *type, int align) { MCC_TRACE("enter\n");
	int rr;
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		vpushv(vtop);
#endif
	rr = ireg(gv(MCC_RC_INT));
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		EI(0x13, 0, rr, rr, 15 + 1);
	else
#endif
		EI(0x13, 0, rr, rr, 15);
	EI(0x13, 7, rr, rr, -16);
	ER(0x33, 0, 2, 2, rr, 0x20);
	{
		int a = align < 16 ? 16 : align;
		if (a > 16) { MCC_TRACE("br\n");
			if (a <= 2048) { MCC_TRACE("br\n");
				EI(0x13, 7, 2, 2, -a);
			} else { MCC_TRACE("br\n");
				load_large_constant(rr, 0, -a);
				ER(0x33, 7, 2, 2, rr, 0);
			}
		}
	}
	vpop();
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check) { MCC_TRACE("br\n");
		vpushi(0);
		vtop->r = MCC_TREG_R(0);
		o(0x00010513);
		vswap();
		vpush_helper_func(TOK___bound_new_region);
		vrott(3);
		gfunc_call(2);
		func_bound_add_epilog = 1;
	}
#endif
}

ST_FUNC void gen_clear_cache(void) { MCC_TRACE("enter\n");
	o(0x0ff0000f);
	o(0x0000100f);
}
