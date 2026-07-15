#define USING_GLOBALS
#include "mcc.h"

ST_DATA const char *const target_machine_defs =
		"__i386__\0"
		"__i386\0";

#define USE_EBX 2

ST_DATA const int reg_classes[MCC_NB_REGS] = {
		MCC_RC_INT | MCC_RC_EAX,
		MCC_RC_INT | MCC_RC_ECX,
		MCC_RC_INT | MCC_RC_EDX,
		(MCC_RC_INT | MCC_RC_EBX) * (USE_EBX == 1),
		MCC_RC_FLOAT | MCC_RC_ST0,
};

#define func_sub_sp_offset (mcc_state->cg_func_sub_sp_offset)
#define func_ret_sub (mcc_state->cg_func_ret_sub)
#if MCC_CONFIG_DIAG_RT >= 2
#define func_bound_offset (mcc_state->cg_func_bound_offset)
#define func_bound_ind (mcc_state->cg_func_bound_ind)
ST_DATA int func_bound_add_epilog;
static void gen_bounds_prolog(void);
static void gen_bounds_epilog(void);
#endif

ST_FUNC void o(unsigned int c) { MCC_TRACE("enter\n");
	while (c) { MCC_TRACE("br\n");
		g(c);
		c = c >> 8;
	}
}

ST_FUNC void gen_le32(int c) { MCC_TRACE("enter\n");
	g(c);
	g(c >> 8);
	g(c >> 16);
	g(c >> 24);
}

ST_FUNC void gsym_addr(int t, int a) { MCC_TRACE("enter\n");
	while (t) { MCC_TRACE("br\n");
		unsigned char *ptr = cur_text_section->data + t;
		uint32_t n = read32le(ptr);
		write32le(ptr, a - t - 4);
		t = n;
	}
}

ST_FUNC void gen_fill_nops(int bytes) { MCC_TRACE("enter\n");
	while (bytes--)
		g(0x90);
}

static void gen_static_call(int v);

static void get_pc_thunk(int r, int add) { MCC_TRACE("enter\n");
	static const char *const pc_thunk_name[] = {
			"__x86.get_pc_thunk.ax",
			"__x86.get_pc_thunk.cx",
			"__x86.get_pc_thunk.dx",
			"__x86.get_pc_thunk.bx"};
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	gen_static_call(tok_alloc_const(pc_thunk_name[r]));
	if (add) { MCC_TRACE("br\n");
		Sym label = {0};
		label.type.t = VT_VOID | VT_STATIC;
		put_extern_sym(&label, cur_text_section, ind, 0);
		r = REG_VALUE(r);
		if (r == 0)
			{ MCC_TRACE("br\n"); oad(0x05, 1); }
		else
			{ MCC_TRACE("br\n"); oad(0xc081 + r * 0x100, 2); }
		greloc(cur_text_section, &label, ind - 4, R_386_GOTPC);
	}
}

ST_FUNC void gen_gotpcrel(int r, Sym *sym, int c) { MCC_TRACE("enter\n");
	greloc(cur_text_section, sym, ind, R_386_GOT32X);
	gen_le32(0);
	if (c) { MCC_TRACE("br\n");
		r = REG_VALUE(r);
		if (r == 0)
			{ MCC_TRACE("br\n"); oad(0x05, c); }
		else
			{ MCC_TRACE("br\n"); oad(0xc081 + r * 0x100, c); }
	}
}

#define gjmp2(instr, lbl) oad(instr, lbl)

ST_FUNC void gen_addr32(int r, Sym *sym, int c) { MCC_TRACE("enter\n");
	if (r & VT_SYM)
		{ MCC_TRACE("br\n"); greloc(cur_text_section, sym, ind, R_386_32); }
	gen_le32(c);
}

ST_FUNC void gen_addrpc32(int r, Sym *sym, int c) { MCC_TRACE("enter\n");
	if (r & VT_SYM)
		{ MCC_TRACE("br\n"); greloc(cur_text_section, sym, ind, R_386_PC32); }
	gen_le32(c - 4);
}

static void gen_modrm(int opc, int op_r2, int r, Sym *sym, int c) { MCC_TRACE("enter\n");
	int op_reg = REG_VALUE(op_r2) << 3;

	if (mcc_state->pic && (r & (VT_VALMASK | VT_SYM)) == (VT_CONST | VT_SYM)) { MCC_TRACE("br\n");
		int is_got = (op_r2 & MCC_TREG_MEM) && !(sym->type.t & VT_STATIC);
		int here = ind;
		get_pc_thunk(MCC_TREG_EBX, is_got);
		o(opc);
		o(0x83 | op_reg);
		if (is_got) { MCC_TRACE("br\n");
			gen_gotpcrel(r, sym, c);
		} else { MCC_TRACE("br\n");
			gen_addrpc32(r, sym, c + (ind - here - 1));
		}
	} else if (mcc_state->pic && (r & VT_VALMASK) < VT_CONST && (r & MCC_TREG_MEM)) { MCC_TRACE("br\n");
		o(opc);
		if (c) { MCC_TRACE("br\n");
			g(0x80 | op_reg | REG_VALUE(r));
			gen_le32(c);
		} else { MCC_TRACE("br\n");
			g(0x00 | op_reg | REG_VALUE(r));
		}
	} else if ((r & VT_VALMASK) == VT_CONST) { MCC_TRACE("br\n");
		o(opc);
		o(0x05 | op_reg);
		gen_addr32(r, sym, c);
	} else if ((r & VT_VALMASK) == VT_LOCAL) { MCC_TRACE("br\n");
		o(opc);
		if (c == (signed char)c) { MCC_TRACE("br\n");
			o(0x45 | op_reg);
			g(c);
		} else { MCC_TRACE("br\n");
			oad(0x85 | op_reg, c);
		}
	} else { MCC_TRACE("br\n");
		o(opc);
		g(0x00 | op_reg | (r & VT_VALMASK));
	}
}

#ifdef MCC_TARGET_PE
static void gen_pe_tls_base(int dst) { MCC_TRACE("enter\n");
	int sc = (dst == MCC_TREG_EAX) ? MCC_TREG_ECX : MCC_TREG_EAX;
	o(0x50 + sc);
	o(0x64);
	o(0x8b);
	o(0x05 | (dst << 3));
	gen_le32(0x2c);
	o(0x8b);
	o(0x05 | (sc << 3));
	greloca(cur_text_section, pe_tls_index_sym(), ind, R_386_32, 0);
	gen_le32(0);
	o(0x8b);
	o(0x44 | (dst << 3));
	o((2 << 6) | (sc << 3) | dst);
	g(0x00);
	o(0x58 + sc);
}
#endif

ST_FUNC void load(int r, SValue *sv) { MCC_TRACE("enter\n");
	int v, t, ft, fc, fr, opc;
	SValue v1;

	fr = sv->r;
	ft = sv->type.t & ~VT_DEFSIGN;
	fc = sv->c.i;
	ft &= ~VT_QUALIFY;
	v = fr & VT_VALMASK;

	if (mcc_state->pic && (fr & (VT_VALMASK | VT_SYM | VT_LVAL)) == (VT_CONST | VT_SYM | VT_LVAL) && !(sv->sym->type.t & VT_STATIC)) { MCC_TRACE("br\n");
		int tr = r | MCC_TREG_MEM;
		if (is_float(ft)) { MCC_TRACE("br\n");
			tr = get_reg(MCC_RC_INT) | MCC_TREG_MEM;
		}
		gen_modrm(0x8b, tr, fr, sv->sym, 0);
		fr = tr | VT_LVAL;
	}

	if (fr & VT_LVAL) { MCC_TRACE("br\n");
		if ((fr & VT_SYM) && sv->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
			int dst_reg = REG_VALUE(r);
			int opc;
			if ((ft & VT_TYPE) == VT_BYTE || (ft & VT_TYPE) == VT_BOOL)
				{ MCC_TRACE("br\n"); opc = 0xbe0f; }
			else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED) ||
							 (ft & VT_TYPE) == (VT_BOOL | VT_UNSIGNED))
				{ MCC_TRACE("br\n"); opc = 0xb60f; }
			else if ((ft & VT_TYPE) == VT_SHORT)
				{ MCC_TRACE("br\n"); opc = 0xbf0f; }
			else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED))
				{ MCC_TRACE("br\n"); opc = 0xb70f; }
			else
				{ MCC_TRACE("br\n"); opc = 0x8b; }
#ifdef MCC_TARGET_PE
			gen_pe_tls_base(dst_reg);
			o(opc);
			o(0x80 | (dst_reg << 3) | dst_reg);
			greloca(cur_text_section, sv->sym, ind, R_386_TLS_LE, 0);
			gen_le32(fc);
#else
			o(0x65);
			o(opc);
			o(0x04 | (dst_reg << 3));
			o(0x25);
			greloca(cur_text_section, sv->sym, ind, R_386_TLS_LE, 0);
			gen_le32(fc);
#endif
			return;
		}
		if (v == VT_LLOCAL) { MCC_TRACE("br\n");
			v1.type.t = VT_INT;
			v1.r = VT_LOCAL | VT_LVAL;
			v1.c.i = fc;
			v1.sym = NULL;
			fr = r;
			if (!(reg_classes[fr] & MCC_RC_INT))
				{ MCC_TRACE("br\n"); fr = get_reg(MCC_RC_INT); }
			load(fr, &v1);
		}
		if ((ft & VT_BTYPE) == VT_FLOAT) { MCC_TRACE("br\n");
			opc = 0xd9;
			r = 0;
		} else if ((ft & VT_BTYPE) == VT_DOUBLE) { MCC_TRACE("br\n");
			opc = 0xdd;
			r = 0;
		} else if ((ft & VT_BTYPE) == VT_LDOUBLE) { MCC_TRACE("br\n");
			opc = 0xdb;
			r = 5;
		} else if ((ft & VT_TYPE) == VT_BYTE || (ft & VT_TYPE) == VT_BOOL) { MCC_TRACE("br\n");
			opc = 0xbe0f;
		} else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED) ||
							 (ft & VT_TYPE) == (VT_BOOL | VT_UNSIGNED)) { MCC_TRACE("br\n");
			opc = 0xb60f;
		} else if ((ft & VT_TYPE) == VT_SHORT) { MCC_TRACE("br\n");
			opc = 0xbf0f;
		} else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) { MCC_TRACE("br\n");
			opc = 0xb70f;
		} else { MCC_TRACE("br\n");
			opc = 0x8b;
		}
		gen_modrm(opc, r, fr, sv->sym, fc);
	} else { MCC_TRACE("br\n");
		if ((fr & (VT_VALMASK | VT_SYM)) == (VT_CONST | VT_SYM) && (sv->sym->type.t & VT_TLS)) { MCC_TRACE("br\n");
			int dst = REG_VALUE(r);
#ifdef MCC_TARGET_PE
			gen_pe_tls_base(dst);
			o(0x81);
			o(0xc0 | dst);
			greloca(cur_text_section, sv->sym, ind, R_386_TLS_LE, 0);
			gen_le32(fc);
#else
			o(0x65);
			o(0x8b);
			o(0x05 | (dst << 3));
			gen_le32(0);
			o(0x81);
			o(0xc0 | dst);
			greloca(cur_text_section, sv->sym, ind, R_386_TLS_LE, 0);
			gen_le32(fc);
#endif
		} else if (mcc_state->pic && (fr & (VT_VALMASK | VT_SYM)) == (VT_CONST | VT_SYM)) { MCC_TRACE("br\n");
			if (sv->sym->type.t & VT_STATIC) { MCC_TRACE("br\n");
				get_pc_thunk(r, 0);
				o(0x808d | REG_VALUE(r) * 0x900);
				gen_addrpc32(fr, sv->sym, fc + 6);
			} else { MCC_TRACE("br\n");
				get_pc_thunk(r, 1);
				o(0x808b | REG_VALUE(r) * 0x900);
				gen_gotpcrel(r, sv->sym, fc);
			}
		} else if (v == VT_CONST) { MCC_TRACE("br\n");
			o(0xb8 + r);
			gen_addr32(fr, sv->sym, fc);
		} else if (v == VT_LOCAL) { MCC_TRACE("br\n");
			if (fc) { MCC_TRACE("br\n");
				gen_modrm(0x8d, r, VT_LOCAL, sv->sym, fc);
			} else { MCC_TRACE("br\n");
				o(0x89);
				o(0xe8 + r);
			}
		} else if (v == VT_LLOCAL) { MCC_TRACE("br\n");
			gen_modrm(0x8b, r, VT_LOCAL, sv->sym, fc);
		} else if (v == VT_CMP) { MCC_TRACE("br\n");
			o(0x0f);
			o(fc);
			o(0xc0 + r);
			o(0xc0b60f + r * 0x90000);
		} else if (v == VT_JMP || v == VT_JMPI) { MCC_TRACE("br\n");
			t = v & 1;
			oad(0xb8 + r, t);
			o(0x05eb);
			gsym(fc);
			oad(0xb8 + r, t ^ 1);
		} else if (v != r) { MCC_TRACE("br\n");
			o(0x89);
			o(0xc0 + r + v * 8);
		}
	}
}

ST_FUNC void store(int r, SValue *v) { MCC_TRACE("enter\n");
	int fr, bt, fc, opc;

	bt = v->type.t & VT_BTYPE;

	if (bt == VT_FLOAT) { MCC_TRACE("br\n");
		opc = 0xd9;
		r = 2;
	} else if (bt == VT_DOUBLE) { MCC_TRACE("br\n");
		opc = 0xdd;
		r = 2;
	} else if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
		opc = 0xdbc0d9;
		r = 7;
	} else if (bt == VT_SHORT) { MCC_TRACE("br\n");
		opc = 0x8966;
	} else if (bt == VT_BYTE || bt == VT_BOOL) { MCC_TRACE("br\n");
		opc = 0x88;
	} else { MCC_TRACE("br\n");
		opc = 0x89;
	}

	fc = v->c.i;
	fr = v->r & VT_VALMASK;

	if (mcc_state->pic && (v->r & (VT_VALMASK | VT_SYM)) == (VT_CONST | VT_SYM) && !(v->sym->type.t & VT_STATIC)) { MCC_TRACE("br\n");
		get_pc_thunk(MCC_TREG_EBX, 1);
		o(0x9b8b);
		gen_gotpcrel(MCC_TREG_EBX, v->sym, v->c.i);
		o(opc);
		o(3 + (r << 3));
	} else if ((v->r & VT_SYM) && v->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_PE
		{
			int base = (REG_VALUE(r) == MCC_TREG_EAX) ? MCC_TREG_ECX : MCC_TREG_EAX;
			o(0x50 + base);
			gen_pe_tls_base(base);
			o(opc);
			o(0x80 | (REG_VALUE(r) << 3) | base);
			greloca(cur_text_section, v->sym, ind, R_386_TLS_LE, 0);
			gen_le32(fc);
			o(0x58 + base);
		}
#else
		o(0x65);
		o(opc);
		o(0x04 | (REG_VALUE(r) << 3));
		o(0x25);
		greloca(cur_text_section, v->sym, ind, R_386_TLS_LE, 0);
		gen_le32(fc);
#endif
		return;
	}

	if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) { MCC_TRACE("br\n");
		gen_modrm(opc, r, v->r, v->sym, fc);
	} else if (fr != r) { MCC_TRACE("br\n");
		o(opc);
		o(0xc0 + fr + r * 8);
	}
}

static void gadd_sp(int val) { MCC_TRACE("enter\n");
	if (val == (signed char)val) { MCC_TRACE("br\n");
		o(0xc483);
		g(val);
	} else { MCC_TRACE("br\n");
		oad(0xc481, val);
	}
}

static void gen_static_call(int v) { MCC_TRACE("enter\n");
	Sym *sym;

	sym = external_helper_sym(v);
	oad(0xe8, -4);
	greloc(cur_text_section, sym, ind - 4, R_386_PC32);
}

static void gcall_or_jmp(int is_jmp) { MCC_TRACE("enter\n");
	int r;
	if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == (VT_CONST | VT_SYM)) { MCC_TRACE("br\n");
		if (mcc_state->pic && !(vtop->sym->type.t & VT_STATIC)) { MCC_TRACE("br\n");
			get_pc_thunk(MCC_TREG_EBX, 1);
			oad(0xe8 + is_jmp, vtop->c.i - 4);
			greloc(cur_text_section, vtop->sym, ind - 4, R_386_PLT32);
			return;
		}
		oad(0xe8 + is_jmp, vtop->c.i - 4);
		greloc(cur_text_section, vtop->sym, ind - 4, R_386_PC32);
	} else { MCC_TRACE("br\n");
		r = gv(MCC_RC_INT);
		o(0xff);
		o(0xd0 + r + (is_jmp << 4));
	}
}

static const uint8_t fastcall_regs[3] = {MCC_TREG_EAX, MCC_TREG_EDX, MCC_TREG_ECX};
static const uint8_t fastcallw_regs[2] = {MCC_TREG_ECX, MCC_TREG_EDX};

static int fastcall_arg_inreg(CType *type) { MCC_TRACE("enter\n");
	int align;
	return !is_float(type->t) && (type->t & VT_BTYPE) != VT_STRUCT && type_size(type, &align) <= 4;
}

static int fastcall_arg_slots(CType *type) { MCC_TRACE("enter\n");
	int align, words;
	if (is_float(type->t))
		{ MCC_TRACE("br\n"); return 0; }
	words = (type_size(type, &align) + 3) >> 2;
	return words > 2 ? 2 : words;
}

#if !defined(MCC_TARGET_PE) && !MCC_TARGETOS_FreeBSD && !MCC_TARGETOS_OpenBSD
static int sysv_struct_ret_in_regs(CType *vt) { MCC_TRACE("enter\n");
	return (vt->t & VT_BTYPE) == VT_STRUCT && vt->ref->a.is_complex && (vt->ref->next->type.t & VT_BTYPE) == VT_FLOAT;
}
#endif

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) { MCC_TRACE("enter\n");
#if defined(MCC_TARGET_PE) || MCC_TARGETOS_FreeBSD || MCC_TARGETOS_OpenBSD
	int size, align, nregs;
	*ret_align = 1;
	*regsize = 4;
	size = type_size(vt, &align);
	if (size > 8 || (size & (size - 1)))
		{ MCC_TRACE("br\n"); return 0; }
	nregs = 1;
	if (size == 8)
		{ MCC_TRACE("br\n"); ret->t = VT_INT, nregs = 2; }
	else if (size == 4)
		{ MCC_TRACE("br\n"); ret->t = VT_INT; }
	else if (size == 2)
		{ MCC_TRACE("br\n"); ret->t = VT_SHORT; }
	else
		{ MCC_TRACE("br\n"); ret->t = VT_BYTE; }
	ret->ref = NULL;
	return nregs;
#else
	*ret_align = 1;
	if (sysv_struct_ret_in_regs(vt)) { MCC_TRACE("br\n");
		*regsize = 4;
		ret->t = VT_INT;
		ret->ref = NULL;
		return 2;
	}
	return 0;
#endif
}

ST_FUNC void gfunc_call(int nb_args) { MCC_TRACE("enter\n");
	int size, align, r, args_size, func_call;
	int fastcall_nb_regs, n_reg_pop;
	const uint8_t *fastcall_regs_ptr;
	Sym *func_sym;

#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gbound_args(nb_args); }
#endif

	func_sym = vtop[-nb_args].type.ref;
	func_call = func_sym->f.func_call;
	fastcall_regs_ptr = NULL;
	fastcall_nb_regs = 0;
	n_reg_pop = 0;
	if (func_call == FUNC_FASTCALLW) { MCC_TRACE("br\n");
		fastcall_regs_ptr = fastcallw_regs, fastcall_nb_regs = 2;
	} else if (func_call == FUNC_THISCALL) { MCC_TRACE("br\n");
		fastcall_regs_ptr = fastcallw_regs, fastcall_nb_regs = 1;
	} else if (func_call >= FUNC_FASTCALL1 && func_call <= FUNC_FASTCALL3) { MCC_TRACE("br\n");
		fastcall_regs_ptr = fastcall_regs, fastcall_nb_regs = func_call - FUNC_FASTCALL1 + 1;
	}
	if (fastcall_nb_regs) { MCC_TRACE("br\n");
		int slots = 0, spilled = 0;
		for (int s = 0; s < nb_args; s++) { MCC_TRACE("br\n");
			CType *t = &vtop[-nb_args + 1 + s].type;
			if (fastcall_arg_inreg(t) && slots < fastcall_nb_regs) { MCC_TRACE("br\n");
				if (spilled)
					{ MCC_TRACE("br\n"); mcc_error("fastcall with a non-register argument before an "
										"integer register argument is not supported"); }
				n_reg_pop++, slots++;
			} else { MCC_TRACE("br\n");
				spilled = 1;
				slots += fastcall_arg_slots(t);
				if (slots >= fastcall_nb_regs)
					{ MCC_TRACE("br\n"); break; }
			}
		}
	}

	save_regs(nb_args + 1);

	args_size = 0;
	for (int i = 0; i < nb_args; i++) { MCC_TRACE("br\n");
		if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			size = type_size(&vtop->type, &align);
			size = (size + 3) & ~3;
#ifdef MCC_TARGET_PE
			if (size >= 4096) { MCC_TRACE("br\n");
				save_reg(MCC_TREG_EDX);
				r = get_reg(MCC_RC_EAX);
				oad(0x68, size);
				gen_static_call(tok_alloc_const("__alloca"));
				gadd_sp(4);
			} else
#endif
			{ MCC_TRACE("br\n");
				oad(0xec81, size);
				r = get_reg(MCC_RC_INT);
				o(0xe089 + (r << 8));
			}
			vset(&vtop->type, r | VT_LVAL, 0);
			vswap();
			vstore();
			args_size += size;
		} else if (is_float(vtop->type.t)) { MCC_TRACE("br\n");
			gv(MCC_RC_FLOAT);
			if ((vtop->type.t & VT_BTYPE) == VT_FLOAT)
				{ MCC_TRACE("br\n"); size = 4; }
			else if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE)
				{ MCC_TRACE("br\n"); size = 8; }
			else
				{ MCC_TRACE("br\n"); size = 12; }
			oad(0xec81, size);
			if (size == 12)
				{ MCC_TRACE("br\n"); o(0x7cdb); }
			else
				{ MCC_TRACE("br\n"); o(0x5cd9 + size - 4); }
			g(0x24);
			g(0x00);
			args_size += size;
		} else { MCC_TRACE("br\n");
			r = gv(MCC_RC_INT);
			if ((vtop->type.t & VT_BTYPE) == VT_LLONG) { MCC_TRACE("br\n");
				size = 8;
				o(0x50 + vtop->r2);
			} else { MCC_TRACE("br\n");
				size = 4;
			}
			o(0x50 + r);
			args_size += size;
		}
		vtop--;
	}

	if (fastcall_nb_regs) { MCC_TRACE("br\n");
		for (int i = 0; i < n_reg_pop; i++) { MCC_TRACE("br\n");
			if (args_size <= 0)
				{ MCC_TRACE("br\n"); break; }
			o(0x58 + fastcall_regs_ptr[i]);
			args_size -= 4;
		}
	}
#if !defined(MCC_TARGET_PE) && !MCC_TARGETOS_FreeBSD || MCC_TARGETOS_OpenBSD
	else if ((vtop->type.ref->type.t & VT_BTYPE) == VT_STRUCT)
		{ MCC_TRACE("br\n"); args_size -= 4; }
#endif

	gcall_or_jmp(0);

	if (args_size && func_call != FUNC_STDCALL && func_call != FUNC_THISCALL && func_call != FUNC_FASTCALLW)
		{ MCC_TRACE("br\n"); gadd_sp(args_size); }
	vtop--;
}

#ifdef MCC_TARGET_PE
#define FUNC_PROLOG_SIZE (10 + !!USE_EBX)
#else
#define FUNC_PROLOG_SIZE (9 + !!USE_EBX)
#endif

ST_FUNC void gfunc_prolog(Sym *func_sym) { MCC_TRACE("enter\n");
	CType *func_type = &func_sym->type;
	int addr, align, size, func_call, fastcall_nb_regs;
	int param_addr, fastcall_used;
	const uint8_t *fastcall_regs_ptr;
	Sym *sym;
	CType *type;

	sym = func_type->ref;
	func_call = sym->f.func_call;
	addr = 8;
	loc = 0;
	func_vc = 0;

	if (func_call >= FUNC_FASTCALL1 && func_call <= FUNC_FASTCALL3) { MCC_TRACE("br\n");
		fastcall_nb_regs = func_call - FUNC_FASTCALL1 + 1;
		fastcall_regs_ptr = fastcall_regs;
	} else if (func_call == FUNC_FASTCALLW) { MCC_TRACE("br\n");
		fastcall_nb_regs = 2;
		fastcall_regs_ptr = fastcallw_regs;
	} else if (func_call == FUNC_THISCALL) { MCC_TRACE("br\n");
		fastcall_nb_regs = 1;
		fastcall_regs_ptr = fastcallw_regs;
	} else { MCC_TRACE("br\n");
		fastcall_nb_regs = 0;
		fastcall_regs_ptr = NULL;
	}
	fastcall_used = 0;

	ind += FUNC_PROLOG_SIZE;
	func_sub_sp_offset = ind;
#if defined(MCC_TARGET_PE) || MCC_TARGETOS_FreeBSD || MCC_TARGETOS_OpenBSD
	size = type_size(&func_vt, &align);
	if (((func_vt.t & VT_BTYPE) == VT_STRUCT) && (size > 8 || (size & (size - 1)))) { MCC_TRACE("br\n");
#else
	if ((func_vt.t & VT_BTYPE) == VT_STRUCT && !sysv_struct_ret_in_regs(&func_vt)) { MCC_TRACE("br\n");
#endif
		func_vc = addr;
		addr += 4;
		if (fastcall_nb_regs)
			{ MCC_TRACE("br\n"); fastcall_used++; }
	}
	while ((sym = sym->next) != NULL) { MCC_TRACE("br\n");
		type = &sym->type;
		size = type_size(type, &align);
		size = (size + 3) & ~3;
#ifdef MCC_FUNC_STRUCT_PARAM_AS_PTR
		if ((type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			size = 4;
		}
#endif
		if (fastcall_arg_inreg(type) && fastcall_used < fastcall_nb_regs) { MCC_TRACE("br\n");
			loc -= 4;
			gen_modrm(0x89, fastcall_regs_ptr[fastcall_used], VT_LOCAL, NULL, loc);
			param_addr = loc;
			fastcall_used++;
		} else { MCC_TRACE("br\n");
			param_addr = addr;
			addr += size;
			fastcall_used += fastcall_arg_slots(type);
			if (fastcall_used > fastcall_nb_regs)
				{ MCC_TRACE("br\n"); fastcall_used = fastcall_nb_regs; }
		}
		gfunc_set_param(sym, param_addr, 0);
	}
	func_ret_sub = 0;
	if (func_call == FUNC_STDCALL || func_call == FUNC_FASTCALLW || func_call == FUNC_THISCALL)
		{ MCC_TRACE("br\n"); func_ret_sub = addr - 8; }
#if !defined(MCC_TARGET_PE) && !MCC_TARGETOS_FreeBSD || MCC_TARGETOS_OpenBSD
	else if (func_vc)
		{ MCC_TRACE("br\n"); func_ret_sub = 4; }
#endif

#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gen_bounds_prolog(); }
#endif
}

ST_FUNC void gfunc_epilog(void) {
	addr_t v, saved_ind;

#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gen_bounds_epilog(); }
#endif

	v = (-loc + 3) & -4;

	if (mcc_state->pic)
		{ MCC_TRACE("br\n"); gen_modrm(0x8b, MCC_TREG_EBX, VT_LOCAL, NULL, -(v + 4)); }

	o(0xc9);
	if (func_ret_sub == 0) { MCC_TRACE("br\n");
		o(0xc3);
	} else { MCC_TRACE("br\n");
		o(0xc2);
		g(func_ret_sub);
		g(func_ret_sub >> 8);
	}
	saved_ind = ind;
	ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
#ifdef MCC_TARGET_PE
	if (v >= 4096) { MCC_TRACE("br\n");
		oad(0xb8, v);
		gen_static_call(TOK___chkstk);
	} else
#endif
	{ MCC_TRACE("br\n");
		o(0xe58955);
		o(0xec81);
		gen_le32(v);
#ifdef MCC_TARGET_PE
		o(0x90);
#endif
	}
	o(mcc_state->pic ? 0x53 : 0x90);
	ind = saved_ind;
}

ST_FUNC int gjmp(int t) {
	return gjmp2(0xe9, t);
}

ST_FUNC void gjmp_addr(int a) {
	int r;
	r = a - ind - 2;
	if (r == (signed char)r) { MCC_TRACE("br\n");
		g(0xeb);
		g(r);
	} else { MCC_TRACE("br\n");
		oad(0xe9, a - ind - 5);
	}
}

ST_FUNC int gjmp_cond(int op, int t) {
	g(0x0f);
	t = gjmp2(op - 16, t);
	return t;
}

ST_FUNC void gen_opi(int op) {
	int r, fr, opc, c;

	switch (op) { MCC_TRACE("br\n");
	case '+':
	case TOK_ADDC1:
		opc = 0;
	gen_op8:
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			vswap();
			r = gv(MCC_RC_INT);
			vswap();
			c = vtop->c.i;
			if (c == (signed char)c) { MCC_TRACE("br\n");
				if ((c == 1 || c == -1) && (op == '+' || op == '-')) { MCC_TRACE("br\n");
					opc = (c == 1) ^ (op == '+');
					o(0x40 | (opc << 3) | r);
				} else { MCC_TRACE("br\n");
					o(0x83);
					o(0xc0 | (opc << 3) | r);
					g(c);
				}
			} else { MCC_TRACE("br\n");
				o(0x81);
				oad(0xc0 | (opc << 3) | r, c);
			}
		} else { MCC_TRACE("br\n");
			gv2(MCC_RC_INT, MCC_RC_INT);
			r = vtop[-1].r;
			fr = vtop[0].r;
			o((opc << 3) | 0x01);
			o(0xc0 + r + fr * 8);
		}
		vtop--;
		if (op >= TOK_ULT && op <= TOK_GT)
			{ MCC_TRACE("br\n"); vset_VT_CMP(op); }
		break;
	case '-':
	case TOK_SUBC1:
		opc = 5;
		goto gen_op8;
	case TOK_ADDC2:
		opc = 2;
		goto gen_op8;
	case TOK_SUBC2:
		opc = 3;
		goto gen_op8;
	case '&':
		opc = 4;
		goto gen_op8;
	case '^':
		opc = 6;
		goto gen_op8;
	case '|':
		opc = 1;
		goto gen_op8;
	case '*':
		gv2(MCC_RC_INT, MCC_RC_INT);
		r = vtop[-1].r;
		fr = vtop[0].r;
		vtop--;
		o(0xaf0f);
		o(0xc0 + fr + r * 8);
		break;
	case TOK_SHL:
		opc = 4;
		goto gen_shift;
	case TOK_SHR:
		opc = 5;
		goto gen_shift;
	case TOK_SAR:
		opc = 7;
	gen_shift:
		opc = 0xc0 | (opc << 3);
		if ((vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST) { MCC_TRACE("br\n");
			vswap();
			r = gv(MCC_RC_INT);
			vswap();
			c = vtop->c.i & 0x1f;
			o(0xc1);
			o(opc | r);
			g(c);
		} else { MCC_TRACE("br\n");
			gv2(MCC_RC_INT, MCC_RC_ECX);
			r = vtop[-1].r;
			o(0xd3);
			o(opc | r);
		}
		vtop--;
		break;
	case '/':
	case TOK_UDIV:
	case TOK_PDIV:
	case '%':
	case TOK_UMOD:
	case TOK_UMULL:
		gv2(MCC_RC_EAX, MCC_RC_ECX);
		r = vtop[-1].r;
		fr = vtop[0].r;
		vtop--;
		save_reg(MCC_TREG_EDX);
		save_reg_upstack(MCC_TREG_EAX, 1);
		if (op == TOK_UMULL) { MCC_TRACE("br\n");
			o(0xf7);
			o(0xe0 + fr);
			vtop->r2 = MCC_TREG_EDX;
			r = MCC_TREG_EAX;
		} else { MCC_TRACE("br\n");
			if (op == TOK_UDIV || op == TOK_UMOD) { MCC_TRACE("br\n");
				o(0xf7d231);
				o(0xf0 + fr);
			} else { MCC_TRACE("br\n");
				o(0xf799);
				o(0xf8 + fr);
			}
			if (op == '%' || op == TOK_UMOD)
				{ MCC_TRACE("br\n"); r = MCC_TREG_EDX; }
			else
				{ MCC_TRACE("br\n"); r = MCC_TREG_EAX; }
		}
		vtop->r = r;
		break;
	default:
		opc = 7;
		goto gen_op8;
	}
}

ST_FUNC void gen_opf(int op) {
	int a, ft, fc, swapped, r;

	if (op == TOK_NEG) { MCC_TRACE("br\n");
		gv(MCC_RC_FLOAT);
		o(0xe0d9);
		return;
	}

	if ((vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_CONST) { MCC_TRACE("br\n");
		vswap();
		gv(MCC_RC_FLOAT);
		vswap();
	}
	if ((vtop[0].r & (VT_VALMASK | VT_LVAL)) == VT_CONST)
		{ MCC_TRACE("br\n"); gv(MCC_RC_FLOAT); }

	if ((vtop[-1].r & VT_LVAL) &&
			(vtop[0].r & VT_LVAL)) { MCC_TRACE("br\n");
		vswap();
		gv(MCC_RC_FLOAT);
		vswap();
	}
	swapped = 0;
	if (vtop[-1].r & VT_LVAL) { MCC_TRACE("br\n");
		vswap();
		swapped = 1;
	}
	if (op >= TOK_ULT && op <= TOK_GT) { MCC_TRACE("br\n");
		load(MCC_TREG_ST0, vtop);
		save_reg(MCC_TREG_EAX);
		if (op == TOK_GE || op == TOK_GT)
			{ MCC_TRACE("br\n"); swapped = !swapped; }
		else if (op == TOK_EQ || op == TOK_NE)
			{ MCC_TRACE("br\n"); swapped = 0; }
		if (swapped)
			{ MCC_TRACE("br\n"); o(0xc9d9); }
		if (op == TOK_EQ || op == TOK_NE)
			{ MCC_TRACE("br\n"); o(0xe9da); }
		else
			{ MCC_TRACE("br\n"); o(0xd9de); }
		o(0xe0df);
		if (op == TOK_EQ) { MCC_TRACE("br\n");
			o(0x45e480);
			o(0x40fC80);
		} else if (op == TOK_NE) { MCC_TRACE("br\n");
			o(0x45e480);
			o(0x40f480);
			op = TOK_NE;
		} else if (op == TOK_GE || op == TOK_LE) { MCC_TRACE("br\n");
			o(0x05c4f6);
			op = TOK_EQ;
		} else { MCC_TRACE("br\n");
			o(0x45c4f6);
			op = TOK_EQ;
		}
		vtop--;
		vset_VT_CMP(op);
	} else { MCC_TRACE("br\n");
		if ((vtop->type.t & VT_BTYPE) == VT_LDOUBLE) { MCC_TRACE("br\n");
			load(MCC_TREG_ST0, vtop);
			swapped = !swapped;
		}

		switch (op) { MCC_TRACE("br\n");
		default:
		case '+':
			a = 0;
			break;
		case '-':
			a = 4;
			if (swapped)
				{ MCC_TRACE("br\n"); a++; }
			break;
		case '*':
			a = 1;
			break;
		case '/':
			a = 6;
			if (swapped)
				{ MCC_TRACE("br\n"); a++; }
			break;
		}
		ft = vtop->type.t;
		fc = vtop->c.i;
		if ((ft & VT_BTYPE) == VT_LDOUBLE) { MCC_TRACE("br\n");
			o(0xde);
			o(0xc1 + (a << 3));
		} else { MCC_TRACE("br\n");
			r = vtop->r;
			if ((r & VT_VALMASK) == VT_LLOCAL) { MCC_TRACE("br\n");
				SValue v1;
				r = get_reg(MCC_RC_INT);
				v1.type.t = VT_INT;
				v1.r = VT_LOCAL | VT_LVAL;
				v1.c.i = fc;
				v1.sym = NULL;
				load(r, &v1);
				fc = 0;
			}

			gen_modrm((ft & VT_BTYPE) == VT_DOUBLE ? 0xdc : 0xd8,
								a, r, vtop->sym, fc);
		}
		vtop--;
	}
}

ST_FUNC void gen_cvt_itof(int t) {
	save_reg(MCC_TREG_ST0);
	gv(MCC_RC_INT);
	if ((vtop->type.t & VT_BTYPE) == VT_LLONG) { MCC_TRACE("br\n");
		o(0x50 + vtop->r2);
		o(0x50 + (vtop->r & VT_VALMASK));
		o(0x242cdf);
		o(0x08c483);
		vtop->r2 = VT_CONST;
	} else if ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
						 (VT_INT | VT_UNSIGNED)) { MCC_TRACE("br\n");
		o(0x6a);
		g(0x00);
		o(0x50 + (vtop->r & VT_VALMASK));
		o(0x242cdf);
		o(0x08c483);
	} else { MCC_TRACE("br\n");
		o(0x50 + (vtop->r & VT_VALMASK));
		o(0x2404db);
		o(0x04c483);
	}
	vtop->r2 = VT_CONST;
	vtop->r = MCC_TREG_ST0;
}

ST_FUNC void gen_cvt_ftoi(int t) {
	int bt = vtop->type.t & VT_BTYPE;
	if (bt == VT_FLOAT)
		{ MCC_TRACE("br\n"); vpush_helper_func(TOK___fixsfdi); }
	else if (bt == VT_LDOUBLE)
		{ MCC_TRACE("br\n"); vpush_helper_func(TOK___fixxfdi); }
	else
		{ MCC_TRACE("br\n"); vpush_helper_func(TOK___fixdfdi); }
	vswap();
	gfunc_call(1);
	vpushi(0);
	vtop->r = REG_IRET;
	if ((t & VT_BTYPE) == VT_LLONG)
		{ MCC_TRACE("br\n"); vtop->r2 = REG_IRE2; }
}

ST_FUNC void gen_cvt_ftof(int t) {
	gv(MCC_RC_FLOAT);
}

ST_FUNC void gen_cvt_csti(int t) {
	int r, sz, xl;
	r = gv(MCC_RC_INT);
	sz = !(t & VT_UNSIGNED);
	xl = (t & VT_BTYPE) == VT_SHORT;
	o(0xc0b60f | (sz << 3 | xl) << 8 | (r << 3 | r) << 16);
}

ST_FUNC void gen_increment_tcov(SValue *sv) {
	int indir, rel, add1, add2;
	if (mcc_state->pic) { MCC_TRACE("br\n");
		get_pc_thunk(MCC_TREG_EBX, 0);
		indir = 0x8300;
		rel = R_386_PC32;
		add1 = 2;
		add2 = 13;
	} else { MCC_TRACE("br\n");
		indir = 0x0500;
		rel = R_386_32;
		add1 = 0;
		add2 = 4;
	}
	o(0x0083 + indir);
	greloc(cur_text_section, sv->sym, ind, rel);
	gen_le32(add1);
	o(1);
	o(0x1083 + indir);
	greloc(cur_text_section, sv->sym, ind, rel);
	gen_le32(add2);
	g(0);
}

ST_FUNC void ggoto(void) {
	gcall_or_jmp(1);
	vtop--;
}

#if MCC_CONFIG_DIAG_RT >= 2

static void gen_bound_call(int v) {
	Sym *sym;

	sym = external_helper_sym(v);
	if (mcc_state->pic) { MCC_TRACE("br\n");
		get_pc_thunk(MCC_TREG_EBX, 1);
		oad(0xe8, -4);
		greloc(cur_text_section, sym, ind - 4, R_386_PLT32);
	} else { MCC_TRACE("br\n");
		oad(0xe8, -4);
		greloc(cur_text_section, sym, ind - 4, R_386_PC32);
	}
}

static void gen_bounds_prolog(void) {
	func_bound_offset = lbounds_section->data_offset;
	func_bound_ind = ind;
	func_bound_add_epilog = 0;
	if (mcc_state->pic) { MCC_TRACE("br\n");
		oad(0xb8, 0);
		oad(0x808d, 0);
		oad(0xb8, 0);
		oad(0xc381, 0);
		oad(0xb8, 0);
	} else { MCC_TRACE("br\n");
		oad(0xb8, 0);
		oad(0xb8, 0);
	}
}

static void gen_bounds_epilog(void) {
	addr_t saved_ind;
	addr_t *bounds_ptr;
	Sym *sym_data;
	int offset_modified = func_bound_offset != lbounds_section->data_offset;

	if (!offset_modified && !func_bound_add_epilog)
		{ MCC_TRACE("br\n"); return; }

	bounds_ptr = section_ptr_add(lbounds_section, sizeof(addr_t));
	*bounds_ptr = 0;

	sym_data = get_sym_ref(&char_pointer_type, lbounds_section,
												 func_bound_offset, MCC_PTR_SIZE);

	if (offset_modified) { MCC_TRACE("br\n");
		saved_ind = ind;
		ind = func_bound_ind;
		if (mcc_state->pic) { MCC_TRACE("br\n");
			get_pc_thunk(MCC_TREG_EAX, 0);
			o(0x808d | MCC_TREG_EAX * 0x900);
			greloc(cur_text_section, sym_data, ind, R_386_PC32);
			gen_le32(2);
		} else { MCC_TRACE("br\n");
			greloc(cur_text_section, sym_data, ind + 1, R_386_32);
			ind = ind + 5;
		}
		gen_bound_call(TOK___bound_local_new);
		ind = saved_ind;
	}

	o(0x5250);
	if (mcc_state->pic) { MCC_TRACE("br\n");
		get_pc_thunk(MCC_TREG_EAX, 0);
		o(0x808d | MCC_TREG_EAX * 0x900);
		greloc(cur_text_section, sym_data, ind, R_386_PC32);
		gen_le32(2);
	} else { MCC_TRACE("br\n");
		greloc(cur_text_section, sym_data, ind + 1, R_386_32);
		oad(0xb8, 0);
	}
	gen_bound_call(TOK___bound_local_delete);
	o(0x585a);
}
#endif

ST_FUNC void gen_vla_sp_save(int addr) {
	gen_modrm(0x89, MCC_TREG_ESP, VT_LOCAL, NULL, addr);
}

ST_FUNC void gen_vla_sp_restore(int addr) {
	gen_modrm(0x8b, MCC_TREG_ESP, VT_LOCAL, NULL, addr);
}

ST_FUNC void gen_vla_alloc(CType *type, int align) {
	int use_call = 0;

#if MCC_CONFIG_DIAG_RT >= 2
	use_call = mcc_state->do_bounds_check;
#endif
#ifdef MCC_TARGET_PE
	use_call = 1;
#endif
	if (use_call) { MCC_TRACE("br\n");
		vpush_helper_func(TOK_alloca);
		vswap();
		gfunc_call(1);
		if (align > 8) { MCC_TRACE("br\n");
			o(0x81);
			g(0xe4);
			gen_le32(-align);
		}
	} else { MCC_TRACE("br\n");
		int r;
		int a = align < 16 ? 16 : align;
		r = gv(MCC_RC_INT);
		o(0x2b);
		o(0xe0 | r);
		if (a > 16) { MCC_TRACE("br\n");
			o(0xe481);
			gen_le32(-a);
		} else { MCC_TRACE("br\n");
			o(0xf0e483);
		}
		vpop();
	}
}

