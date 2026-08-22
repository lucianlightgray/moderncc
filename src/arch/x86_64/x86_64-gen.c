#define USING_GLOBALS
#include "mcc.h"
#include <assert.h>

ST_DATA const char *const target_machine_defs =
		"__x86_64__\0"
		"__x86_64\0"
		"__amd64__\0";

ST_DATA int reg_classes[MCC_NB_REGS] = {
		MCC_RC_INT | MCC_RC_RAX,
		MCC_RC_INT | MCC_RC_RCX,
		MCC_RC_INT | MCC_RC_RDX,
		0,
		0,
		0,
		MCC_RC_RSI,
		MCC_RC_RDI,
		MCC_RC_R8,
		MCC_RC_R9,
		MCC_RC_R10,
		MCC_RC_R11,
		0,
		0,
		0,
		0,
		MCC_RC_FLOAT | MCC_RC_XMM0,
		MCC_RC_FLOAT | MCC_RC_XMM1,
		MCC_RC_FLOAT | MCC_RC_XMM2,
		MCC_RC_FLOAT | MCC_RC_XMM3,
		MCC_RC_FLOAT | MCC_RC_XMM4,
		MCC_RC_FLOAT | MCC_RC_XMM5,
		MCC_RC_XMM6,
		MCC_RC_XMM7,
		MCC_RC_XMM8,
		MCC_RC_XMM9,
		MCC_RC_XMM10,
		MCC_RC_XMM11,
		MCC_RC_XMM12,
		MCC_RC_XMM13,
		MCC_RC_XMM14,
		MCC_RC_XMM15,
		MCC_RC_ST0};

#define func_sub_sp_offset (mcc_state->cg_func_sub_sp_offset)
#define func_ret_sub (mcc_state->cg_func_ret_sub)
#ifndef MCC_TARGET_PE
#define func_stack_chk_loc (mcc_state->cg_func_stack_chk_loc)
#endif
#define func_asan_offset (mcc_state->cg_func_asan_offset)
#define func_asan_ind (mcc_state->cg_func_asan_ind)

#define func_bound_offset (mcc_state->cg_func_bound_offset)
#define func_bound_ind (mcc_state->cg_func_bound_ind)
ST_DATA int func_bound_add_epilog;

#ifdef MCC_TARGET_PE
#define func_scratch (mcc_state->cg_func_scratch)
#define func_alloca (mcc_state->cg_func_alloca)
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

ST_FUNC void gen_le64(int64_t c) { MCC_TRACE("enter\n");
	g(c);
	g(c >> 8);
	g(c >> 16);
	g(c >> 24);
	g(c >> 32);
	g(c >> 40);
	g(c >> 48);
	g(c >> 56);
}

static void orex(int ll, int r, int r2, int b) { MCC_TRACE("enter\n");
	if ((r & VT_VALMASK) >= VT_CONST && !(r & MCC_TREG_MEM))
		{ MCC_TRACE("br\n"); r = 0; }
	if ((r2 & VT_VALMASK) >= VT_CONST && !(r2 & MCC_TREG_MEM))
		{ MCC_TRACE("br\n"); r2 = 0; }
	if (ll || REX_BASE(r) || REX_BASE(r2))
		{ MCC_TRACE("br\n"); o(0x40 | REX_BASE(r) | (REX_BASE(r2) << 2) | (ll << 3)); }
	o(b);
}

static void sse_rex(int reg, int rm) { MCC_TRACE("enter\n");
	if (REX_BASE(reg) || REX_BASE(rm))
		{ MCC_TRACE("br\n"); o(0x40 | REX_BASE(rm) | (REX_BASE(reg) << 2)); }
}

static void sse_unpin_src(void) { MCC_TRACE("enter\n");
	if (ast_pinned_regs & ((uint64_t)1 << (vtop->r & VT_VALMASK))) { MCC_TRACE("br\n");
		int nr = get_reg(MCC_RC_FLOAT);
		load(nr, vtop);
		vtop->r = nr;
	}
}

ST_FUNC void gsym_addr(int t, int a) { MCC_TRACE("enter\n");
	while (t) { MCC_TRACE("br\n");
		unsigned char *ptr = cur_text_section->data + t;
		uint32_t n = read32le(ptr);
		write32le(ptr, a < 0 ? -a : a - t - 4);
		t = n;
	}
}

static int is64_type(int t) { MCC_TRACE("enter\n");
	return ((t & VT_BTYPE) == VT_PTR ||
					(t & VT_BTYPE) == VT_FUNC ||
					(t & VT_BTYPE) == VT_INT128 ||
					(t & VT_BTYPE) == VT_LLONG);
}

#define gjmp2(instr, lbl) oad(instr, lbl)

ST_FUNC void gen_addr32(int r, Sym *sym, int c) { MCC_TRACE("enter\n");
	if (r & VT_SYM)
		{ MCC_TRACE("br\n"); greloca(cur_text_section, sym, ind, R_X86_64_32S, c), c = 0; }
	gen_le32(c);
}

ST_FUNC void gen_addrpc32(int r, Sym *sym, int c) { MCC_TRACE("enter\n");
	if (r & VT_SYM)
		{ MCC_TRACE("br\n"); greloca(cur_text_section, sym, ind, R_X86_64_PC32, c - 4), c = 4; }
	gen_le32(c - 4);
}

static void gen_gotpcrel(int r, Sym *sym, int c) { MCC_TRACE("enter\n");

#ifdef MCC_TARGET_PE
	mcc_error("internal error: no GOT on PE: %s %x %x | %02x %02x %02x\n",
						get_tok_str(sym->v, NULL), c, r,
						cur_text_section->data[ind - 3],
						cur_text_section->data[ind - 2],
						cur_text_section->data[ind - 1]);
#endif
	greloca(cur_text_section, sym, ind, R_X86_64_GOTPCREL, -4);
	gen_le32(0);
	if (c) { MCC_TRACE("br\n");
		orex(1, r, 0, 0x81);
		o(0xc0 + REG_VALUE(r));
		gen_le32(c);
	}
}

static void gen_modrm_impl(int op_reg, int r, Sym *sym, int c, int is_got) { MCC_TRACE("enter\n");
	op_reg = REG_VALUE(op_reg) << 3;
	if ((r & VT_VALMASK) == VT_CONST) { MCC_TRACE("br\n");
		if (!(r & VT_SYM)) { MCC_TRACE("br\n");
			o(0x04 | op_reg);
			oad(0x25, c);
		} else { MCC_TRACE("br\n");
			o(0x05 | op_reg);
			if (is_got) { MCC_TRACE("br\n");
				gen_gotpcrel(r, sym, c);
			} else { MCC_TRACE("br\n");
				gen_addrpc32(r, sym, c);
			}
		}
	} else if ((r & VT_VALMASK) == VT_LOCAL) { MCC_TRACE("br\n");
		if (c == (signed char)c) { MCC_TRACE("br\n");
			o(0x45 | op_reg);
			g(c);
		} else { MCC_TRACE("br\n");
			oad(0x85 | op_reg, c);
		}
	} else { MCC_TRACE("br\n");
		int rv = REG_VALUE(r);
		int indirect = (r & (MCC_TREG_MEM | VT_REGDISP)) != 0;
		int disp32 = indirect && c;
		if (disp32) { MCC_TRACE("br\n");
			g(0x80 | op_reg | rv);
			if (rv == 4)
				{ MCC_TRACE("br\n"); g(0x24); }
			gen_le32(c);
		} else if (rv == 5) { MCC_TRACE("br\n");
			g(0x40 | op_reg | rv);
			g(0x00);
		} else { MCC_TRACE("br\n");
			g(0x00 | op_reg | rv);
			if (rv == 4)
				{ MCC_TRACE("br\n"); g(0x24); }
		}
	}
}

static void gen_modrm(int op_reg, int r, Sym *sym, int c) { MCC_TRACE("enter\n");
	gen_modrm_impl(op_reg, r, sym, c, 0);
}

static void gen_modrm64(int opcode, int op_reg, int r, Sym *sym, int c) { MCC_TRACE("enter\n");
	int is_got;
	is_got = (op_reg & MCC_TREG_MEM) && !(sym->type.t & VT_STATIC);
	orex(1, r, op_reg, opcode);
	gen_modrm_impl(op_reg, r, sym, c, is_got);
}

ST_FUNC int signbit_inline_on(void) { MCC_TRACE("enter\n");
	static int on = -1;
	if (on < 0) { MCC_TRACE("br\n");
		const char *e = getenv("MCC_SIGNBIT_INLINE");
		on = e && e[0] ? (strcmp(e, "0") ? 1 : 0) : 1;
	}
	return on;
}

void gen_signbit(int isfloat) { MCC_TRACE("enter\n");
	int r, d;

	gv(MCC_RC_FLOAT);
	r = vtop->r & VT_VALMASK;
	d = get_reg(MCC_RC_INT);
	if (!isfloat)
		{ MCC_TRACE("br\n"); o(0x66); }
	sse_rex(d, r);
	o(0x0f);
	o(0x50);
	o(0xc0 + REG_VALUE(d) * 8 + REG_VALUE(r));
	orex(0, d, 0, 0x83);
	o(0xe0 + REG_VALUE(d));
	o(1);
	vtop->r = d;
	vtop->r2 = VT_CONST;
	vtop->type.t = VT_INT;
}

#ifdef MCC_TARGET_PE
static void gen_pe_tls_base(int dst) { MCC_TRACE("enter\n");
	int sc = (REG_VALUE(dst) == MCC_TREG_RAX) ? MCC_TREG_RCX : MCC_TREG_RAX;
	o(0x50 + sc);
	o(0x65);
	o(0x48 | (REX_BASE(dst) << 2));
	o(0x8b);
	o(0x04 | (REG_VALUE(dst) << 3));
	o(0x25);
	gen_le32(0x58);
	o(0x8b);
	o(0x05 | (sc << 3));
	gen_addrpc32(VT_SYM, pe_tls_index_sym(), 0);
	o(0x48 | (REX_BASE(dst) << 2) | REX_BASE(dst));
	o(0x8b);
	o(0x44 | (REG_VALUE(dst) << 3));
	o((3 << 6) | (sc << 3) | REG_VALUE(dst));
	g(0x00);
	o(0x58 + sc);
}
#endif

#ifdef MCC_TARGET_MACHO
static void gen_macho_tls_base(Sym *sym) { MCC_TRACE("enter\n");
	o(0x50);
	o(0x57);
	o(0x3d8d48);
	gen_addrpc32(VT_SYM, sym, 0);
	o(0x17ff);
	o(0xc38949);
	o(0x5f);
	o(0x58);
}
#endif

#ifndef MCC_TARGET_PE
static void x86_64_vec16_move(int xr, SValue *sv, int is_store);
#endif

void load(int r, SValue *sv) { MCC_TRACE("enter\n");
	mcc_stackref_note(sv->r);
	int v, t, ft, fc, fr;
	SValue v1;

	fr = sv->r;
	ft = sv->type.t & ~VT_DEFSIGN;
	fc = sv->c.i;
	if (fc != sv->c.i && (fr & VT_SYM))
		{ MCC_TRACE("br\n"); mcc_error("64 bit addend in load"); }

#ifndef MCC_TARGET_PE
	if ((ft & VT_BTYPE) == VT_FLOAT128 && (fr & VT_LVAL)) { MCC_TRACE("br\n");
		x86_64_vec16_move(r, sv, 0);
		return;
	}
#endif

	ft &= ~VT_QUALIFY;

#ifndef MCC_TARGET_PE
	if ((fr & VT_VALMASK) == VT_CONST && (fr & VT_SYM) &&
			(fr & VT_LVAL) && !(sv->sym->type.t & VT_STATIC) && !(sv->sym->type.t & VT_TLS)) { MCC_TRACE("br\n");
		int tr = r | MCC_TREG_MEM;
		if (is_float(ft)) { MCC_TRACE("br\n");
			tr = get_reg(MCC_RC_INT) | MCC_TREG_MEM;
		}
		gen_modrm64(0x8b, tr, fr, sv->sym, 0);

		fr = tr | VT_LVAL;
	}
#endif

	if ((fr & VT_VALMASK) == VT_CONST && (fr & VT_SYM) &&
			(fr & VT_LVAL) && (sv->sym->type.t & VT_TLS)) { MCC_TRACE("br\n");
		int tr = r | MCC_TREG_MEM;
		if (is_float(ft))
			{ MCC_TRACE("br\n"); tr = get_reg(MCC_RC_INT) | MCC_TREG_MEM; }
#if defined(MCC_TARGET_PE)
		gen_pe_tls_base(tr);
		o(0x48 | REX_BASE(tr));
		o(0x81);
		o(0xc0 | REG_VALUE(tr));
		greloca(cur_text_section, sv->sym, ind, R_X86_64_TPOFF32, 0);
		gen_le32(0);
#elif defined(MCC_TARGET_MACHO)
		gen_macho_tls_base(sv->sym);
		orex(1, tr, MCC_TREG_R11, 0x89);
		o(0xc0 + REG_VALUE(tr) + REG_VALUE(MCC_TREG_R11) * 8);
#else
		o(0x64);
		o(0x48 | (REX_BASE(tr) << 2));
		o(0x8b);
		o(0x04 | (REG_VALUE(tr) << 3));
		o(0x25);
		gen_le32(0);
		o(0x48 | REX_BASE(tr));
		o(0x81);
		o(0xc0 | REG_VALUE(tr));
		greloca(cur_text_section, sv->sym, ind, R_X86_64_TPOFF32, 0);
		gen_le32(0);
#endif
		fr = tr | VT_LVAL;
	}

	v = fr & VT_VALMASK;
	if (fr & VT_LVAL) { MCC_TRACE("br\n");
		int b, ll;
		if (v == VT_LLOCAL) { MCC_TRACE("br\n");
			v1.type.t = VT_PTR;
			v1.r = VT_LOCAL | VT_LVAL;
			v1.c.i = fc;
			v1.sym = NULL;
			fr = r;
			if (!(reg_classes[fr] & (MCC_RC_INT | MCC_RC_R11)))
				{ MCC_TRACE("br\n"); fr = get_reg(MCC_RC_INT); }
			load(fr, &v1);
		}
		if (fc != sv->c.i) { MCC_TRACE("br\n");
			v1.type.t = VT_LLONG;
			v1.r = VT_CONST;
			v1.c.i = sv->c.i;
			v1.sym = NULL;
			fr = r;
			if (!(reg_classes[fr] & (MCC_RC_INT | MCC_RC_R11)))
				{ MCC_TRACE("br\n"); fr = get_reg(MCC_RC_INT); }
			load(fr, &v1);
			fc = 0;
		}
		ll = 0;
		if ((ft & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			int align;
			switch (type_size(&sv->type, &align)) { MCC_TRACE("br\n");
			case 1:
				ft = VT_BYTE;
				break;
			case 2:
				ft = VT_SHORT;
				break;
			case 4:
				ft = VT_INT;
				break;
			case 8:
				ft = VT_LLONG;
				break;
			default:
				mcc_error("invalid aggregate type for register load");
				break;
			}
		}
		if ((ft & VT_BTYPE) == VT_FLOAT) { MCC_TRACE("br\n");
			o(0x66);
			b = 0x6e0f;
		} else if ((ft & VT_BTYPE) == VT_DOUBLE) { MCC_TRACE("br\n");
			o(0xf3);
			b = 0x7e0f;
		} else if ((ft & VT_BTYPE) == VT_LDOUBLE) { MCC_TRACE("br\n");
			b = 0xdb, r = 5;
		} else if (IS_HALF_BT(ft & VT_BTYPE)) { MCC_TRACE("br\n");
			b = 0xb70f;
		} else if ((ft & VT_TYPE) == VT_BYTE || (ft & VT_TYPE) == VT_BOOL) { MCC_TRACE("br\n");
			b = 0xbe0f;
		} else if ((ft & VT_TYPE) == (VT_BYTE | VT_UNSIGNED)) { MCC_TRACE("br\n");
			b = 0xb60f;
		} else if ((ft & VT_TYPE) == VT_SHORT) { MCC_TRACE("br\n");
			b = 0xbf0f;
		} else if ((ft & VT_TYPE) == (VT_SHORT | VT_UNSIGNED)) { MCC_TRACE("br\n");
			b = 0xb70f;
		} else if ((ft & VT_TYPE) == (VT_VOID)) { MCC_TRACE("br\n");
			return;
		} else { MCC_TRACE("br\n");
			assert(((ft & VT_BTYPE) == VT_INT) || ((ft & VT_BTYPE) == VT_LLONG) || ((ft & VT_BTYPE) == VT_INT128) || ((ft & VT_BTYPE) == VT_PTR) || ((ft & VT_BTYPE) == VT_FUNC));
			ll = is64_type(ft);
			b = 0x8b;
		}
		if (ll) { MCC_TRACE("br\n");
			gen_modrm64(b, r, fr, sv->sym, fc);
		} else { MCC_TRACE("br\n");
			orex(ll, fr, r, b);
			gen_modrm(r, fr, sv->sym, fc);
		}
	} else { MCC_TRACE("br\n");
		if (v == VT_CONST) { MCC_TRACE("br\n");
			if (fr & VT_SYM) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_PE
				if (sv->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
					gen_pe_tls_base(r);
					o(0x48 | REX_BASE(r));
					o(0x81);
					o(0xc0 | REG_VALUE(r));
					greloca(cur_text_section, sv->sym, ind, R_X86_64_TPOFF32, fc);
					gen_le32(0);
				} else { MCC_TRACE("br\n");
					orex(1, 0, r, 0x8d);
					o(0x05 + REG_VALUE(r) * 8);
					gen_addrpc32(fr, sv->sym, fc);
				}
#else
				if (sv->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_MACHO
					gen_macho_tls_base(sv->sym);
					o(0x48 | (REX_BASE(r) << 2) | REX_BASE(MCC_TREG_R11));
					o(0x8d);
					o(0x80 | (REG_VALUE(r) << 3) | REG_VALUE(MCC_TREG_R11));
					gen_le32(fc);
#else
					int dst = REG_VALUE(r);
					o(0x64);
					o(0x48 | (REX_BASE(r) << 2));
					o(0x8b);
					o(0x04 | (dst << 3));
					o(0x25);
					gen_le32(0);
					o(0x48 | REX_BASE(r));
					o(0x81);
					o(0xc0 | dst);
					greloca(cur_text_section, sv->sym, ind,
									R_X86_64_TPOFF32, fc);
					gen_le32(0);
#endif
				} else if (sv->sym->type.t & VT_STATIC) { MCC_TRACE("br\n");
					orex(1, 0, r, 0x8d);
					o(0x05 + REG_VALUE(r) * 8);
					gen_addrpc32(fr, sv->sym, fc);
				} else { MCC_TRACE("br\n");
					orex(1, 0, r, 0x8b);
					o(0x05 + REG_VALUE(r) * 8);
					gen_gotpcrel(r, sv->sym, fc);
				}
#endif
			} else if (is64_type(ft)) { MCC_TRACE("br\n");
				if (sv->c.i >> 32) { MCC_TRACE("br\n");
					orex(1, r, 0, 0xb8 + REG_VALUE(r));
					gen_le64(sv->c.i);
				} else if (sv->c.i > 0) { MCC_TRACE("br\n");
					orex(0, r, 0, 0xb8 + REG_VALUE(r));
					gen_le32(sv->c.i);
				} else { MCC_TRACE("br\n");
					orex(0, r, r, 0x31);
					o(0xc0 + REG_VALUE(r) * 9);
				}
			} else { MCC_TRACE("br\n");
				orex(0, r, 0, 0xb8 + REG_VALUE(r));
				gen_le32(fc);
			}
		} else if (v == VT_LOCAL) { MCC_TRACE("br\n");
			orex(1, 0, r, 0x8d);
			gen_modrm(r, VT_LOCAL, sv->sym, fc);
		} else if (v == VT_LLOCAL) { MCC_TRACE("br\n");
			orex(1, 0, r, 0x8b);
			gen_modrm(r, VT_LOCAL, sv->sym, fc);
		} else if (v == VT_CMP) { MCC_TRACE("br\n");
			if (fc & 0x100) { MCC_TRACE("br\n");
				v = vtop->cmp_r;
				fc &= ~0x100;
				orex(0, r, 0, 0xb0 + REG_VALUE(r));
				g(v ^ fc ^ (v == TOK_NE));
				o(0x037a + (REX_BASE(r) << 8));
			}
			orex(0, r, 0, 0x0f);
			o(fc);
			o(0xc0 + REG_VALUE(r));
			orex(0, r, r, 0x0f);
			o(0xc0b6 + REG_VALUE(r) * 0x900);
		} else if (v == VT_JMP || v == VT_JMPI) { MCC_TRACE("br\n");
			t = v & 1;
			orex(0, r, 0, 0);
			oad(0xb8 + REG_VALUE(r), t);
			o(0x05eb + (REX_BASE(r) << 8));
			gsym(fc);
			orex(0, r, 0, 0);
			oad(0xb8 + REG_VALUE(r), t ^ 1);
		} else if (v != r) { MCC_TRACE("br\n");
			if ((r >= MCC_TREG_XMM0) && (r <= MCC_TREG_XMM15)) { MCC_TRACE("br\n");
				if (v == MCC_TREG_ST0) { MCC_TRACE("br\n");
					o(0xf0245cdd);
					o(0xf2);
					sse_rex(r, MCC_TREG_RSP);
					o(0x100f);
					o(0x44 + REG_VALUE(r) * 8);
					o(0xf024);
				} else { MCC_TRACE("br\n");
					assert((v >= MCC_TREG_XMM0) && (v <= MCC_TREG_XMM15));
					if ((ft & VT_BTYPE) == VT_FLOAT) { MCC_TRACE("br\n");
						o(0xf3);
					} else { MCC_TRACE("br\n");
						assert((ft & VT_BTYPE) == VT_DOUBLE);
						o(0xf2);
					}
					sse_rex(r, v);
					o(0x100f);
					o(0xc0 + REG_VALUE(v) + REG_VALUE(r) * 8);
				}
			} else if (r == MCC_TREG_ST0) { MCC_TRACE("br\n");
				assert((v >= MCC_TREG_XMM0) && (v <= MCC_TREG_XMM15));
				o(0xf2);
				sse_rex(v, MCC_TREG_RSP);
				o(0x110f);
				o(0x44 + REG_VALUE(v) * 8);
				o(0xf024);
				o(0xf02444dd);
			} else { MCC_TRACE("br\n");
				orex(is64_type(ft), r, v, 0x89);
				o(0xc0 + REG_VALUE(r) + REG_VALUE(v) * 8);
			}
		}
	}
}

void store(int r, SValue *v) { MCC_TRACE("enter\n");
	mcc_stackref_note(v->r);
	int fr, bt, ft, fc;
	int op64 = 0;
	int pic = 0;

	fr = v->r & VT_VALMASK;
	ft = v->type.t;
	fc = v->c.i;
	if (fc != v->c.i && (fr & VT_SYM))
		{ MCC_TRACE("br\n"); mcc_error("64 bit addend in store"); }
	ft &= ~VT_QUALIFY;
	bt = ft & VT_BTYPE;

#ifndef MCC_TARGET_PE
	if (bt == VT_FLOAT128) { MCC_TRACE("br\n");
		x86_64_vec16_move(r, v, 1);
		return;
	}
#endif

	if ((v->r & VT_SYM) && v->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
#if defined(MCC_TARGET_PE)
		gen_pe_tls_base(MCC_TREG_R11);
		o(0x49);
		o(0x81);
		o(0xc3);
		greloca(cur_text_section, v->sym, ind, R_X86_64_TPOFF32, fc);
		gen_le32(0);
#elif defined(MCC_TARGET_MACHO)
		gen_macho_tls_base(v->sym);
		if (fc) { MCC_TRACE("br\n");
			o(0x49);
			o(0x81);
			o(0xc3);
			gen_le32(fc);
		}
#else
		o(0x64);
		o(0x4c);
		o(0x8b);
		o(0x1c);
		o(0x25);
		gen_le32(0);
		o(0x49);
		o(0x81);
		o(0xc3);
		greloca(cur_text_section, v->sym, ind, R_X86_64_TPOFF32, fc);
		gen_le32(0);
#endif
		pic = is64_type(bt) ? 0x49 : 0x41;
		fc = 0;
	}
#ifndef MCC_TARGET_PE
	else if (fr == VT_CONST && (v->r & VT_SYM) && !(v->sym->type.t & VT_STATIC)) { MCC_TRACE("br\n");
		o(0x1d8b4c);
		gen_gotpcrel(MCC_TREG_R11, v->sym, v->c.i);
		pic = is64_type(bt) ? 0x49 : 0x41;
	}
#endif

	if (bt == VT_FLOAT) { MCC_TRACE("br\n");
		o(0x66);
		if (pic)
			{ MCC_TRACE("br\n"); o(pic | (REX_BASE(r) << 2)); }
		else
			{ MCC_TRACE("br\n"); orex(0, v->r, r, 0); }
		o(0x7e0f);
		r = REG_VALUE(r);
	} else if (bt == VT_DOUBLE) { MCC_TRACE("br\n");
		o(0x66);
		if (pic)
			{ MCC_TRACE("br\n"); o(pic | (REX_BASE(r) << 2)); }
		else
			{ MCC_TRACE("br\n"); orex(0, v->r, r, 0); }
		o(0xd60f);
		r = REG_VALUE(r);
	} else if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
		o(0xc0d9);
		if (pic)
			{ MCC_TRACE("br\n"); o(pic); }
		else
			{ MCC_TRACE("br\n"); orex(0, v->r, 0, 0); }
		o(0xdb);
		r = 7;
	} else { MCC_TRACE("br\n");
		if (bt == VT_SHORT || bt == VT_FLOAT16 || bt == VT_BF16)
			{ MCC_TRACE("br\n"); o(0x66); }
		o(pic);
		if (bt == VT_BYTE || bt == VT_BOOL)
			{ MCC_TRACE("br\n"); orex(0, fr, r, 0x88); }
		else if (is64_type(bt))
			{ MCC_TRACE("br\n"); op64 = 0x89; }
		else
			{ MCC_TRACE("br\n"); orex(0, fr, r, 0x89); }
	}
	if (pic) { MCC_TRACE("br\n");
		if (op64)
			{ MCC_TRACE("br\n"); o(op64); }
		o(3 + (r << 3));
	} else if (op64) { MCC_TRACE("br\n");
		if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) { MCC_TRACE("br\n");
			gen_modrm64(op64, r, v->r, v->sym, fc);
		} else if (fr != r) { MCC_TRACE("br\n");
			orex(1, fr, r, op64);
			o(0xc0 + REG_VALUE(fr) + REG_VALUE(r) * 8);
		}
	} else { MCC_TRACE("br\n");
		if (fr == VT_CONST || fr == VT_LOCAL || (v->r & VT_LVAL)) { MCC_TRACE("br\n");
			gen_modrm(r, v->r, v->sym, fc);
		} else if (fr != r) { MCC_TRACE("br\n");
			o(0xc0 + REG_VALUE(fr) + REG_VALUE(r) * 8);
		}
	}
}

static void gcall_or_jmp(int is_jmp) { MCC_TRACE("enter\n");
	int r;
	if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST &&
			((vtop->r & VT_SYM) && (vtop->c.i - 4) == (int)(vtop->c.i - 4))) { MCC_TRACE("br\n");
		greloca(cur_text_section, vtop->sym, ind + 1, R_X86_64_PLT32, (int)(vtop->c.i - 4));
		oad(0xe8 + is_jmp, 0);
	} else { MCC_TRACE("br\n");
		r = MCC_TREG_R11;
		load(r, vtop);
		o(0x41);
		o(0xff);
		o(0xd0 + REG_VALUE(r) + (is_jmp << 4));
	}
}


static void gen_bounds_call(int v) { MCC_TRACE("enter\n");
	Sym *sym = external_helper_sym(v);
	oad(0xe8, 0);
	greloca(cur_text_section, sym, ind - 4, R_X86_64_PLT32, -4);
}

#ifdef MCC_TARGET_PE
#define MCC_TREG_FASTCALL_1 MCC_TREG_RCX
#else
#define MCC_TREG_FASTCALL_1 MCC_TREG_RDI
#endif

static void gen_bounds_prolog(void) { MCC_TRACE("enter\n");
	func_bound_offset = lbounds_section->data_offset;
	func_bound_ind = ind;
	func_bound_add_epilog = 0;
	o(0x0d8d48 + ((MCC_TREG_FASTCALL_1 == MCC_TREG_RDI) * 0x300000));
	gen_le32(0);
	oad(0xb8, 0);
}

static void gen_bounds_epilog(void) { MCC_TRACE("enter\n");
	addr_t saved_ind;
	Sym *sym_data;
	int offset_modified;

	if (!gen_bounds_epilog_head(func_bound_offset, &sym_data, &offset_modified))
		{ MCC_TRACE("br\n"); return; }

	if (offset_modified) { MCC_TRACE("br\n");
		saved_ind = ind;
		ind = func_bound_ind;
		greloca(cur_text_section, sym_data, ind + 3, R_X86_64_PC32, -4);
		ind = ind + 7;
		gen_bounds_call(TOK___bound_local_new);
		ind = saved_ind;
	}

	o(0x5250);
	o(0x20ec8348);
	o(0x290f);
	o(0x102444);
	o(0x240c290f);
	greloca(cur_text_section, sym_data, ind + 3, R_X86_64_PC32, -4);
	o(0x0d8d48 + ((MCC_TREG_FASTCALL_1 == MCC_TREG_RDI) * 0x300000));
	gen_le32(0);
	gen_bounds_call(TOK___bound_local_delete);
	o(0x280f);
	o(0x102444);
	o(0x240c280f);
	o(0x20c48348);
	o(0x585a);
}

static void gen_asan_stack_call(const char *name) { MCC_TRACE("enter\n");
	Sym *sym = external_helper_sym(tok_alloc_const(name));
	oad(0xe8, 0);
	greloca(cur_text_section, sym, ind - 4, R_X86_64_PLT32, -4);
}

#ifdef MCC_TARGET_PE
static void gen_asan_stack_prolog(void) { MCC_TRACE("enter\n");
	if (!asan_lstack_section)
		{ MCC_TRACE("br\n"); asan_lstack_section =
			new_section(mcc_state, ".asan_lstack", SHT_PROGBITS, SHF_ALLOC); }
	func_asan_offset = asan_lstack_section->data_offset;
	func_asan_ind = ind;
	o(0x20ec8348);
	o(0x0d8d48);
	gen_le32(0);
	o(0xea8948);
	oad(0xb8, 0);
	o(0x20c48348);
}

static void gen_asan_stack_epilog(void) { MCC_TRACE("enter\n");
	addr_t saved_ind;
	Sym *sym_data;

	if (!gen_asan_stack_epilog_head(func_asan_offset, &sym_data))
		{ MCC_TRACE("br\n"); return; }

	saved_ind = ind;
	ind = func_asan_ind + 4;
	greloca(cur_text_section, sym_data, ind + 3, R_X86_64_PC32, -4);
	ind = ind + 10;
	gen_asan_stack_call("__asan_stack_enter");
	ind = saved_ind;

	o(0x5050);
	o(0x20ec8348);
	greloca(cur_text_section, sym_data, ind + 3, R_X86_64_PC32, -4);
	o(0x0d8d48);
	gen_le32(0);
	o(0xea8948);
	gen_asan_stack_call("__asan_stack_leave");
	o(0x20c48348);
	o(0x5858);
}
#else
static void gen_asan_stack_prolog(void) { MCC_TRACE("enter\n");
	if (!asan_lstack_section)
		{ MCC_TRACE("br\n"); asan_lstack_section =
			new_section(mcc_state, ".asan_lstack", SHT_PROGBITS, SHF_ALLOC); }
	func_asan_offset = asan_lstack_section->data_offset;
	func_asan_ind = ind;
	o(0x3d8d48);
	gen_le32(0);
	o(0xee8948);
	oad(0xb8, 0);
}

static void gen_asan_stack_epilog(void) { MCC_TRACE("enter\n");
	addr_t saved_ind;
	Sym *sym_data;

	if (!gen_asan_stack_epilog_head(func_asan_offset, &sym_data))
		{ MCC_TRACE("br\n"); return; }

	saved_ind = ind;
	ind = func_asan_ind;
	greloca(cur_text_section, sym_data, ind + 3, R_X86_64_PC32, -4);
	ind = ind + 10;
	gen_asan_stack_call("__asan_stack_enter");
	ind = saved_ind;

	o(0x5250);
	o(0x20ec8348);
	o(0x290f);
	o(0x102444);
	o(0x240c290f);
	greloca(cur_text_section, sym_data, ind + 3, R_X86_64_PC32, -4);
	o(0x3d8d48);
	gen_le32(0);
	o(0xee8948);
	gen_asan_stack_call("__asan_stack_leave");
	o(0x280f);
	o(0x102444);
	o(0x240c280f);
	o(0x20c48348);
	o(0x585a);
}
#endif

#ifdef MCC_TARGET_PE

#define REGN 4
static const uint8_t arg_regs[REGN] = {
		MCC_TREG_RCX, MCC_TREG_RDX, MCC_TREG_R8, MCC_TREG_R9};

static int arg_prepare_reg(int idx) { MCC_TRACE("enter\n");
	if (idx == 0 || idx == 1)
		{ MCC_TRACE("br\n"); return idx + 10; }
	else
		{ MCC_TRACE("br\n"); return idx >= 0 && idx < REGN ? arg_regs[idx] : 0; }
}

static void gen_offs_sp(int b, int r, int d) { MCC_TRACE("enter\n");
	orex(1, 0, r & 0x100 ? 0 : r, b);
	if (d == (signed char)d) { MCC_TRACE("br\n");
		o(0x2444 | (REG_VALUE(r) << 3));
		g(d);
	} else { MCC_TRACE("br\n");
		o(0x2484 | (REG_VALUE(r) << 3));
		gen_le32(d);
	}
}

static int using_regs(int size) { MCC_TRACE("enter\n");
	return !(size > 8 || (size & (size - 1)));
}

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) { MCC_TRACE("enter\n");
	int size, align;
	*ret_align = 1;
	*regsize = 8;
	size = type_size(vt, &align);
	if (!using_regs(size))
		{ MCC_TRACE("br\n"); return 0; }
	if (size == 8)
		{ MCC_TRACE("br\n"); ret->t = VT_LLONG; }
	else if (size == 4)
		{ MCC_TRACE("br\n"); ret->t = VT_INT; }
	else if (size == 2)
		{ MCC_TRACE("br\n"); ret->t = VT_SHORT; }
	else
		{ MCC_TRACE("br\n"); ret->t = VT_BYTE; }
	ret->ref = NULL;
	return 1;
}

static int is_sse_float(int t) { MCC_TRACE("enter\n");
	int bt;
	bt = t & VT_BTYPE;
	return bt == VT_DOUBLE || bt == VT_FLOAT;
}

static int gfunc_arg_size(CType *type) { MCC_TRACE("enter\n");
	int align;
	if (type->t & (VT_ARRAY | VT_BITFIELD))
		{ MCC_TRACE("br\n"); return 8; }
	return type_size(type, &align);
}

void gfunc_call(int nb_args) { MCC_TRACE("enter\n");
	int size, r, args_size, d, bt, struct_size;
	int arg;

	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gbound_args(nb_args); }

	save_regs(nb_args);

	args_size = (nb_args < REGN ? REGN : nb_args) * MCC_PTR_SIZE;
	arg = nb_args;

	struct_size = args_size;
	for (int i = 0; i < nb_args; i++) { MCC_TRACE("br\n");
		SValue *sv;

		--arg;
		sv = &vtop[-i];
		bt = (sv->type.t & VT_BTYPE);
		size = gfunc_arg_size(&sv->type);

		if (using_regs(size))
			{ MCC_TRACE("br\n"); continue; }

		if (bt == VT_STRUCT) { MCC_TRACE("br\n");
			size = (size + 15) & ~15;
			r = get_reg(MCC_RC_INT);
			gen_offs_sp(0x8d, r, struct_size);
			struct_size += size;

			vset(&sv->type, r | VT_LVAL, 0);
			vpushv(sv);
			vstore();
			--vtop;
		} else if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
			gv(MCC_RC_ST0);
			gen_offs_sp(0xdb, 0x107, struct_size);
			struct_size += 16;
		}
	}

	if (func_scratch < struct_size)
		{ MCC_TRACE("br\n"); func_scratch = struct_size; }

	arg = nb_args;
	struct_size = args_size;

	for (int i = 0; i < nb_args; i++) { MCC_TRACE("br\n");
		--arg;
		bt = (vtop->type.t & VT_BTYPE);

		size = gfunc_arg_size(&vtop->type);
		if (!using_regs(size)) { MCC_TRACE("br\n");
			size = (size + 15) & ~15;
			if (arg >= REGN) { MCC_TRACE("br\n");
				d = get_reg(MCC_RC_INT);
				gen_offs_sp(0x8d, d, struct_size);
				gen_offs_sp(0x89, d, arg * 8);
			} else { MCC_TRACE("br\n");
				d = arg_prepare_reg(arg);
				gen_offs_sp(0x8d, d, struct_size);
			}
			struct_size += size;
		} else { MCC_TRACE("br\n");
			if (is_sse_float(vtop->type.t)) { MCC_TRACE("br\n");
				if (mcc_state->nosse)
					{ MCC_TRACE("br\n"); mcc_error("SSE disabled"); }
				if (arg >= REGN) { MCC_TRACE("br\n");
					gv(MCC_RC_XMM0);
					gen_offs_sp(0xd60f66, 0x100, arg * 8);
				} else { MCC_TRACE("br\n");
					gv(MCC_RC_XMM0 << arg);
					d = arg_prepare_reg(arg);
					o(0x66);
					orex(1, d, 0, 0x7e0f);
					o(0xc0 + arg * 8 + REG_VALUE(d));
				}
			} else { MCC_TRACE("br\n");
				if (bt == VT_STRUCT) { MCC_TRACE("br\n");
					vtop->type.ref = NULL;
					vtop->type.t = size > 4 ? VT_LLONG : size > 2 ? VT_INT
																					 : size > 1		? VT_SHORT
																												: VT_BYTE;
				}

				r = gv(MCC_RC_INT);
				if (arg >= REGN) { MCC_TRACE("br\n");
					gen_offs_sp(0x89, r, arg * 8);
				} else { MCC_TRACE("br\n");
					d = arg_prepare_reg(arg);
					orex(1, d, r, 0x89);
					o(0xc0 + REG_VALUE(r) * 8 + REG_VALUE(d));
				}
			}
		}
		vtop--;
	}

	if (nb_args > 0) { MCC_TRACE("br\n");
		o(0xd1894c);
		if (nb_args > 1) { MCC_TRACE("br\n");
			o(0xda894c);
		}
	}

	gcall_or_jmp(0);

	if ((vtop->r & VT_SYM) && vtop->sym->v == TOK_alloca) { MCC_TRACE("br\n");
		o(0x48);
		func_alloca = oad(0x05, func_alloca);
		if (mcc_state->do_bounds_check)
			{ MCC_TRACE("br\n"); gen_bounds_call(TOK___bound_alloca_nr); }
	}
	vtop--;
}

#define FUNC_PROLOG_SIZE 11

void gfunc_prolog(Sym *func_sym) { MCC_TRACE("enter\n");
	CType *func_type = &func_sym->type;
	int addr, reg_param_index, bt, size;
	if (func_naked)
		{ MCC_TRACE("br\n"); return; }
	Sym *sym;
	CType *type;

	func_ret_sub = 0;
	func_scratch = 32;
	func_alloca = 0;
	loc = 0;

	addr = MCC_PTR_SIZE * 2;
	ind += FUNC_PROLOG_SIZE;
	func_sub_sp_offset = ind;
	reg_param_index = 0;

	sym = func_type->ref;

	size = gfunc_arg_size(&func_vt);
	if (!using_regs(size)) { MCC_TRACE("br\n");
		gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
		func_vc = addr;
		reg_param_index++;
		addr += 8;
	}

	while ((sym = sym->next) != NULL) { MCC_TRACE("br\n");
		type = &sym->type;
		bt = type->t & VT_BTYPE;
		size = gfunc_arg_size(type);
		if (!using_regs(size)) { MCC_TRACE("br\n");
			if (reg_param_index < REGN) { MCC_TRACE("br\n");
				gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
			}
			gfunc_set_param(sym, addr, 1);
		} else { MCC_TRACE("br\n");
			if (reg_param_index < REGN) { MCC_TRACE("br\n");
				if ((bt == VT_FLOAT) || (bt == VT_DOUBLE)) { MCC_TRACE("br\n");
					if (mcc_state->nosse)
						{ MCC_TRACE("br\n"); mcc_error("SSE disabled"); }
					o(0xd60f66);
					gen_modrm(reg_param_index, VT_LOCAL, NULL, addr);
				} else { MCC_TRACE("br\n");
					gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
				}
			}
			gfunc_set_param(sym, addr, 0);
		}
		addr += 8;
		reg_param_index++;
	}

	while (reg_param_index < REGN) { MCC_TRACE("br\n");
		if (func_var) { MCC_TRACE("br\n");
			gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, addr);
			addr += 8;
		}
		reg_param_index++;
	}
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gen_bounds_prolog(); }
	if (mcc_state->do_asan_shadow)
		{ MCC_TRACE("br\n"); gen_asan_stack_prolog(); }
}

void gfunc_epilog(void) { MCC_TRACE("enter\n");
	int v, start;
	if (func_naked)
		{ MCC_TRACE("br\n"); return; }

	func_scratch = (func_scratch + 15) & -16;
	loc = (loc & -16) - func_scratch;

	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gen_bounds_epilog(); }
	if (mcc_state->do_asan_shadow)
		{ MCC_TRACE("br\n"); gen_asan_stack_epilog(); }

	o(0xc9);
	if (func_ret_sub == 0) { MCC_TRACE("br\n");
		o(0xc3);
	} else { MCC_TRACE("br\n");
		o(0xc2);
		g(func_ret_sub);
		g(func_ret_sub >> 8);
	}

	v = -loc;
	start = func_sub_sp_offset - FUNC_PROLOG_SIZE;
	cur_text_section->data_offset = ind;
	pe_add_unwind_data(start, ind, v);

	ind = start;
	if (v >= 4096) { MCC_TRACE("br\n");
		Sym *sym = external_helper_sym(TOK___chkstk);
		oad(0xb8, v);
		oad(0xe8, 0);
		greloca(cur_text_section, sym, ind - 4, R_X86_64_PLT32, -4);
		o(0x90);
	} else { MCC_TRACE("br\n");
		o(0xe5894855);
		o(0xec8148);
		gen_le32(v);
	}
	ind = cur_text_section->data_offset;

	gsym_addr(func_alloca, -func_scratch);
}

#else

static void gadd_sp(int val) { MCC_TRACE("enter\n");
	if (val == (signed char)val) { MCC_TRACE("br\n");
		o(0xc48348);
		g(val);
	} else { MCC_TRACE("br\n");
		oad(0xc48148, val);
	}
}

typedef enum X86_64_Mode {
	x86_64_mode_none,
	x86_64_mode_memory,
	x86_64_mode_integer,
	x86_64_mode_sse,
	x86_64_mode_sseup,
	x86_64_mode_x87
} X86_64_Mode;

static X86_64_Mode classify_x86_64_merge(X86_64_Mode a, X86_64_Mode b) { MCC_TRACE("enter\n");
	if (a == b)
		{ MCC_TRACE("br\n"); return a; }
	else if (a == x86_64_mode_none)
		{ MCC_TRACE("br\n"); return b; }
	else if (b == x86_64_mode_none)
		{ MCC_TRACE("br\n"); return a; }
	else if ((a == x86_64_mode_memory) || (b == x86_64_mode_memory))
		{ MCC_TRACE("br\n"); return x86_64_mode_memory; }
	else if ((a == x86_64_mode_integer) || (b == x86_64_mode_integer))
		{ MCC_TRACE("br\n"); return x86_64_mode_integer; }
	else if ((a == x86_64_mode_x87) || (b == x86_64_mode_x87))
		{ MCC_TRACE("br\n"); return x86_64_mode_memory; }
	else
		{ MCC_TRACE("br\n"); return x86_64_mode_sse; }
}

static X86_64_Mode classify_x86_64_inner(CType *ty) { MCC_TRACE("enter\n");
	X86_64_Mode mode;
	Sym *f;

	while ((ty->t & (VT_BTYPE | VT_ARRAY)) == (VT_PTR | VT_ARRAY))
		{ MCC_TRACE("br\n"); ty = &ty->ref->type; }

	switch (ty->t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_VOID:
		return x86_64_mode_none;

	case VT_INT:
	case VT_BYTE:
	case VT_SHORT:
	case VT_LLONG:
	case VT_INT128:
	case VT_BOOL:
	case VT_PTR:
	case VT_FUNC:
	case VT_FLOAT16:
	case VT_BF16:
	case VT_SFRACT:
	case VT_FRACT:
	case VT_LFRACT:
	case VT_SACCUM:
	case VT_ACCUM:
	case VT_LACCUM:
		return x86_64_mode_integer;

	case VT_FLOAT:
	case VT_DOUBLE:
	case VT_FLOAT128:
	case VT_DEC32:
	case VT_DEC64:
	case VT_DEC128:
		return x86_64_mode_sse;

	case VT_LDOUBLE:
		return x86_64_mode_x87;

	case VT_STRUCT:
		f = ty->ref;

		if (f->a.is_vector) { MCC_TRACE("br\n");
			int align, sz = type_size(ty, &align);
			if (sz == 8 && (f->next->type.t & VT_BTYPE) == VT_DOUBLE)
				{ MCC_TRACE("br\n"); return x86_64_mode_memory; }
			return x86_64_mode_sse;
		}

		mode = x86_64_mode_none;
		for (f = f->next; f; f = f->next) { MCC_TRACE("br\n");
			if ((f->type.t & VT_ARRAY) && f->type.ref->c == 0)
				{ MCC_TRACE("br\n"); continue; }
			mode = classify_x86_64_merge(mode, classify_x86_64_inner(&f->type));
		}

		return mode;
	}
	mcc_error("internal error: unsupported base type %d in ABI classification",
						ty->t & VT_BTYPE);
	return 0;
}

static int x86_64_has_unaligned_field(CType *ty, int base) { MCC_TRACE("enter\n");
	Sym *f;
	if ((ty->t & VT_BTYPE) != VT_STRUCT)
		{ MCC_TRACE("br\n"); return 0; }
	for (f = ty->ref->next; f; f = f->next) { MCC_TRACE("br\n");
		int align, off = base + f->c;
		type_size(&f->type, &align);
		if (align > 1 && (off & (align - 1)))
			{ MCC_TRACE("br\n"); return 1; }
		if ((f->type.t & VT_BTYPE) == VT_STRUCT && x86_64_has_unaligned_field(&f->type, off))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static void classify_x86_64_eb(CType *ty, int off, X86_64_Mode cls[2]) { MCC_TRACE("enter\n");
	if ((ty->t & VT_BTYPE) == VT_STRUCT && !(ty->t & VT_ARRAY) && !ty->ref->a.is_vector) { MCC_TRACE("br\n");
		Sym *f;
		for (f = ty->ref->next; f; f = f->next)
			{ MCC_TRACE("br\n"); classify_x86_64_eb(&f->type, off + f->c, cls); }
		return;
	}
	{
		X86_64_Mode m = classify_x86_64_inner(ty);
		int align, sz = type_size(ty, &align);
		int e, e1 = (off + sz - 1) >> 3;
		for (e = off >> 3; e <= e1 && e < 2; e++)
			{ MCC_TRACE("br\n"); cls[e] = classify_x86_64_merge(cls[e], m); }
	}
}

static int x86_64_mixed_class(CType *ty, X86_64_Mode cls[2]) { MCC_TRACE("enter\n");
	int align, sz;
	if ((ty->t & VT_BTYPE) != VT_STRUCT || (ty->t & VT_ARRAY))
		{ MCC_TRACE("br\n"); return 0; }
	sz = type_size(ty, &align);
	if (sz <= 8 || sz > 16 || x86_64_has_unaligned_field(ty, 0))
		{ MCC_TRACE("br\n"); return 0; }
	cls[0] = cls[1] = x86_64_mode_none;
	classify_x86_64_eb(ty, 0, cls);
	if ((cls[0] != x86_64_mode_integer && cls[0] != x86_64_mode_sse) ||
			(cls[1] != x86_64_mode_integer && cls[1] != x86_64_mode_sse))
		{ MCC_TRACE("br\n"); return 0; }
	return cls[0] != cls[1];
}

static int x86_64_is_vec16(CType *ty) { MCC_TRACE("enter\n");
	Sym *f;
	int align, sz;
	if ((ty->t & (VT_BTYPE | VT_ARRAY)) == VT_FLOAT128)
		{ MCC_TRACE("br\n"); return 1; }
	while ((ty->t & (VT_BTYPE | VT_ARRAY)) == (VT_PTR | VT_ARRAY)) { MCC_TRACE("br\n");
		if (type_size(ty, &align) != 16)
			{ MCC_TRACE("br\n"); return 0; }
		ty = &ty->ref->type;
	}
	if ((ty->t & VT_BTYPE) != VT_STRUCT || (ty->t & VT_ARRAY))
		{ MCC_TRACE("br\n"); return 0; }
	sz = type_size(ty, &align);
	if (sz != 16)
		{ MCC_TRACE("br\n"); return 0; }
	if (ty->ref->a.is_vector)
		{ MCC_TRACE("br\n"); return 1; }
	f = ty->ref->next;
	if (!f || f->next || f->c)
		{ MCC_TRACE("br\n"); return 0; }
	return x86_64_is_vec16(&f->type);
}

static void x86_64_vec16_move(int xr, SValue *sv, int is_store) { MCC_TRACE("enter\n");
	int fr = sv->r;
	int fc = sv->c.i;
	Sym *sym = sv->sym;
	int v = fr & VT_VALMASK;

	if (!(fr & VT_LVAL) && v < VT_CONST && (reg_classes[v] & MCC_RC_FLOAT)) { MCC_TRACE("br\n");
		int sr = is_store ? xr : v;
		int dr = is_store ? v : xr;
		if (sr != dr) { MCC_TRACE("br\n");
			sse_rex(dr, sr);
			o(0x280f);
			o(0xc0 + REG_VALUE(dr) * 8 + REG_VALUE(sr));
		}
		return;
	}

	if (v == VT_LLOCAL ||
			(v == VT_CONST && (fr & VT_SYM) &&
			 (!(sym->type.t & VT_STATIC) || (sym->type.t & VT_TLS)))) { MCC_TRACE("br\n");
		SValue v1;
		int tr = get_reg(MCC_RC_INT);
		v1.type.t = VT_PTR;
		v1.type.ref = NULL;
		v1.r2 = VT_CONST;
		v1.c.i = fc;
		if (v == VT_LLOCAL) { MCC_TRACE("br\n");
			v1.r = VT_LOCAL | VT_LVAL;
			v1.sym = NULL;
		} else { MCC_TRACE("br\n");
			v1.r = fr & ~VT_LVAL;
			v1.sym = sym;
		}
		load(tr, &v1);
		fr = tr | VT_LVAL;
		fc = 0;
		sym = NULL;
	}
	sse_rex(xr, fr);
	o(is_store ? 0x110f : 0x100f);
	gen_modrm(xr, fr, sym, fc);
}

void x86_64_vec16_packed_op(SValue *res, SValue *lhs, SValue *rhs, int op, int is_double) { MCC_TRACE("enter\n");
	int a, r1, r2;

	switch (op) { MCC_TRACE("br\n");
	case '+': a = 0; break;
	case '-': a = 4; break;
	case '*': a = 1; break;
	case '/': a = 6; break;
	default: return;
	}

	r1 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r1;
	r2 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r2;

	x86_64_vec16_move(r1, lhs, 0);
	x86_64_vec16_move(r2, rhs, 0);

	if (is_double)
		{ MCC_TRACE("br\n"); o(0x66); }
	sse_rex(r1, r2);
	o(0x0f);
	o(0x58 + a);
	o(0xc0 + REG_VALUE(r1) * 8 + REG_VALUE(r2));

	x86_64_vec16_move(r1, res, 1);

	ast_pinned_regs &= ~((uint64_t)1 << r1);
	ast_pinned_regs &= ~((uint64_t)1 << r2);
}

void x86_64_vec16_packed_iop(SValue *res, SValue *lhs, SValue *rhs, int op, int esz) { MCC_TRACE("enter\n");
	int opc, r1, r2;

	switch (op) { MCC_TRACE("br\n");
	case '&': opc = 0xdb; break;
	case '|': opc = 0xeb; break;
	case '^': opc = 0xef; break;
	case '*': opc = 0xd5; break;
	case '+':
		opc = esz == 1 ? 0xfc : esz == 2 ? 0xfd : esz == 8 ? 0xd4 : 0xfe;
		break;
	case '-':
		opc = esz == 1 ? 0xf8 : esz == 2 ? 0xf9 : esz == 8 ? 0xfb : 0xfa;
		break;
	default: return;
	}

	r1 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r1;
	r2 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r2;

	x86_64_vec16_move(r1, lhs, 0);
	x86_64_vec16_move(r2, rhs, 0);

	o(0x66);
	sse_rex(r1, r2);
	o(0x0f);
	o(opc);
	o(0xc0 + REG_VALUE(r1) * 8 + REG_VALUE(r2));

	x86_64_vec16_move(r1, res, 1);

	ast_pinned_regs &= ~((uint64_t)1 << r1);
	ast_pinned_regs &= ~((uint64_t)1 << r2);
}

void x86_64_vec16_imul32(SValue *res, SValue *op1, SValue *op2) { MCC_TRACE("enter\n");
	int r1, r2, r3, a, b, c;

	r1 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r1;
	r2 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r2;
	r3 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r3;

	x86_64_vec16_move(r1, op1, 0);
	x86_64_vec16_move(r2, op2, 0);
	a = REG_VALUE(r1);
	b = REG_VALUE(r2);
	c = REG_VALUE(r3);

	sse_rex(r3, r1);
	o(0x280f);
	o(0xc0 | (c << 3) | a);
	o(0x66); sse_rex(0, r1); o(0x730f); o(0xd0 | a); g(32);
	o(0x66); sse_rex(r3, r2); o(0xf40f); o(0xc0 | (c << 3) | b);
	o(0x66); sse_rex(0, r2); o(0x730f); o(0xd0 | b); g(32);
	o(0x66); sse_rex(r1, r2); o(0xf40f); o(0xc0 | (a << 3) | b);
	o(0x66); sse_rex(r3, r3); o(0x700f); o(0xc0 | (c << 3) | c); g(8);
	o(0x66); sse_rex(r1, r1); o(0x700f); o(0xc0 | (a << 3) | a); g(8);
	o(0x66); sse_rex(r3, r1); o(0x620f); o(0xc0 | (c << 3) | a);

	x86_64_vec16_move(r3, res, 1);

	ast_pinned_regs &= ~((uint64_t)1 << r1);
	ast_pinned_regs &= ~((uint64_t)1 << r2);
	ast_pinned_regs &= ~((uint64_t)1 << r3);
}

void x86_64_vec16_packed_fcmp(SValue *res, SValue *op1, SValue *op2, int imm, int is_double) { MCC_TRACE("enter\n");
	int r1, r2;

	r1 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r1;
	r2 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r2;

	x86_64_vec16_move(r1, op1, 0);
	x86_64_vec16_move(r2, op2, 0);

	if (is_double)
		{ MCC_TRACE("br\n"); o(0x66); }
	sse_rex(r1, r2);
	o(0x0f);
	o(0xc2);
	o(0xc0 + REG_VALUE(r1) * 8 + REG_VALUE(r2));
	g(imm);

	x86_64_vec16_move(r1, res, 1);

	ast_pinned_regs &= ~((uint64_t)1 << r1);
	ast_pinned_regs &= ~((uint64_t)1 << r2);
}

void x86_64_vec16_packed_icmp(SValue *res, SValue *op1, SValue *op2, int opc, int negate) { MCC_TRACE("enter\n");
	int r1, r2, r3;

	r1 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r1;
	r2 = get_reg(MCC_RC_FLOAT);
	ast_pinned_regs |= (uint64_t)1 << r2;

	x86_64_vec16_move(r1, op1, 0);
	x86_64_vec16_move(r2, op2, 0);

	o(0x66);
	sse_rex(r1, r2);
	o(0x0f);
	o(opc);
	o(0xc0 + REG_VALUE(r1) * 8 + REG_VALUE(r2));

	if (negate) { MCC_TRACE("br\n");
		r3 = get_reg(MCC_RC_FLOAT);
		ast_pinned_regs |= (uint64_t)1 << r3;
		o(0x66);
		sse_rex(r3, r3);
		o(0x0f);
		o(0x76);
		o(0xc0 + REG_VALUE(r3) * 8 + REG_VALUE(r3));
		o(0x66);
		sse_rex(r1, r3);
		o(0x0f);
		o(0xef);
		o(0xc0 + REG_VALUE(r1) * 8 + REG_VALUE(r3));
		ast_pinned_regs &= ~((uint64_t)1 << r3);
	}

	x86_64_vec16_move(r1, res, 1);

	ast_pinned_regs &= ~((uint64_t)1 << r1);
	ast_pinned_regs &= ~((uint64_t)1 << r2);
}

static X86_64_Mode classify_x86_64_arg(CType *ty, CType *ret, int *psize, int *palign, int *reg_count) { MCC_TRACE("enter\n");
	X86_64_Mode mode;
	int size, align, ret_t = 0;

	if (ty->t & (VT_BITFIELD | VT_ARRAY)) { MCC_TRACE("br\n");
		*psize = 8;
		*palign = 8;
		*reg_count = 1;
		ret_t = ty->t;
		mode = x86_64_mode_integer;
	} else { MCC_TRACE("br\n");
		size = type_size(ty, &align);
		*psize = (size + 7) & ~7;
		*palign = (align + 7) & ~7;
		*reg_count = 0;

		if (size > 16) { MCC_TRACE("br\n");
			mode = x86_64_mode_memory;
		} else if (x86_64_is_vec16(ty)) { MCC_TRACE("br\n");
			mode = x86_64_mode_sseup;
			*reg_count = 1;
		} else if ((ty->t & VT_BTYPE) == VT_STRUCT && x86_64_has_unaligned_field(ty, 0)) { MCC_TRACE("br\n");
			mode = x86_64_mode_memory;
		} else { MCC_TRACE("br\n");
			mode = classify_x86_64_inner(ty);
			switch (mode) { MCC_TRACE("br\n");
			case x86_64_mode_integer:
				if (size > 8) { MCC_TRACE("br\n");
					*reg_count = 2;
					ret_t = VT_QLONG;
				} else { MCC_TRACE("br\n");
					*reg_count = 1;
					if (size > 4)
						{ MCC_TRACE("br\n"); ret_t = VT_LLONG; }
					else if (size > 2)
						{ MCC_TRACE("br\n"); ret_t = VT_INT; }
					else if (size > 1)
						{ MCC_TRACE("br\n"); ret_t = VT_SHORT; }
					else
						{ MCC_TRACE("br\n"); ret_t = VT_BYTE; }
					if ((ty->t & VT_BTYPE) == VT_STRUCT || (ty->t & VT_UNSIGNED))
						{ MCC_TRACE("br\n"); ret_t |= VT_UNSIGNED; }
				}
				break;

			case x86_64_mode_x87:
				*reg_count = 1;
				ret_t = VT_LDOUBLE;
				break;

			case x86_64_mode_sse:
				if (size > 8) { MCC_TRACE("br\n");
					*reg_count = 2;
					ret_t = VT_QFLOAT;
				} else { MCC_TRACE("br\n");
					*reg_count = 1;
					ret_t = (size > 4) ? VT_DOUBLE : VT_FLOAT;
				}
				break;
			default:
				break;
			}
		}
	}

	if (ret) { MCC_TRACE("br\n");
		ret->ref = NULL;
		ret->t = ret_t;
		ret->bp = (ret_t & VT_BITFIELD) ? ty->bp : 0;
		ret->bs = (ret_t & VT_BITFIELD) ? ty->bs : 0;
	}

	return mode;
}

ST_FUNC int classify_x86_64_va_arg(CType *ty) { MCC_TRACE("enter\n");
	enum __va_arg_type {
		__va_gen_reg,
		__va_float_reg,
		__va_stack,
		__va_gen_sse,
		__va_sse_gen,
		__va_sse_up
	};
	int size, align, reg_count;
	X86_64_Mode cls[2];
	X86_64_Mode mode = classify_x86_64_arg(ty, NULL, &size, &align, &reg_count);
	if (mode != x86_64_mode_memory && x86_64_mixed_class(ty, cls)) { MCC_TRACE("br\n");
		return cls[0] == x86_64_mode_integer ? __va_gen_sse : __va_sse_gen;
	}
	switch (mode) { MCC_TRACE("br\n");
	default:
		return __va_stack;
	case x86_64_mode_integer:
		return __va_gen_reg;
	case x86_64_mode_sse:
		return __va_float_reg;
	case x86_64_mode_sseup:
		return __va_sse_up;
	}
}

static int x86_64_complex_ldouble(CType *vt) { MCC_TRACE("enter\n");
	return (vt->t & VT_BTYPE) == VT_STRUCT && vt->ref->a.is_complex && (vt->ref->next->type.t & VT_BTYPE) == VT_LDOUBLE;
}

#define X86_64_ARG_BASE (MCC_PTR_SIZE * 2)

static int x86_64_stack_arg_align(int addr, int align) { MCC_TRACE("enter\n");
	if (align <= 8)
		{ MCC_TRACE("br\n"); return addr; }
	return X86_64_ARG_BASE + ((addr - X86_64_ARG_BASE + align - 1) & -align);
}

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret, int *ret_align, int *regsize) { MCC_TRACE("enter\n");
	int size, align, reg_count;
	if (x86_64_complex_ldouble(vt)) { MCC_TRACE("br\n");
		*ret_align = 1;
		*regsize = 16;
		ret->t = VT_LDOUBLE;
		ret->ref = NULL;
		return -1;
	}
	if ((vt->t & VT_BTYPE) == VT_FLOAT128) { MCC_TRACE("br\n");
		*ret_align = 16;
		*regsize = 16;
		ret->t = VT_FLOAT128;
		ret->ref = NULL;
		return 1;
	}
	if (x86_64_is_vec16(vt)) { MCC_TRACE("br\n");
		*ret_align = 16;
		*regsize = 16;
		ret->t = 0;
		ret->ref = NULL;
		return -1;
	}
	{
		X86_64_Mode cls[2];
		if (x86_64_mixed_class(vt, cls)) { MCC_TRACE("br\n");
			*ret_align = 1;
			*regsize = 8;
			ret->t = 0;
			ret->ref = NULL;
			return -1;
		}
	}
	if (classify_x86_64_arg(vt, ret, &size, &align, &reg_count) == x86_64_mode_memory)
		{ MCC_TRACE("br\n"); return 0; }
	*ret_align = 1;
	*regsize = 8 * reg_count;
	return 1;
}

ST_FUNC void arch_transfer_ret_regs(int aftercall) { MCC_TRACE("enter\n");
	SValue *sv = vtop;
	Sym *re;
	int fr = sv->r & VT_VALMASK;
	int fc = sv->c.i;
	X86_64_Mode cls[2];
	if (x86_64_is_vec16(&sv->type)) { MCC_TRACE("br\n");
		x86_64_vec16_move(MCC_TREG_XMM0, sv, aftercall);
		return;
	}
	if (x86_64_mixed_class(&sv->type, cls)) { MCC_TRACE("br\n");
		int i, br = sv->r;
		Sym *bsym = sv->sym;
		if (fr == VT_LLOCAL ||
				(fr == VT_CONST && (br & VT_SYM) &&
				 (!(bsym->type.t & VT_STATIC) || (bsym->type.t & VT_TLS)))) { MCC_TRACE("br\n");
			SValue v1;
			int tr = get_reg(MCC_RC_INT);
			v1.type.t = VT_PTR;
			v1.type.ref = NULL;
			v1.r2 = VT_CONST;
			v1.c.i = fc;
			if (fr == VT_LLOCAL) { MCC_TRACE("br\n");
				v1.r = VT_LOCAL | VT_LVAL;
				v1.sym = NULL;
			} else { MCC_TRACE("br\n");
				v1.r = br & ~VT_LVAL;
				v1.sym = bsym;
			}
			load(tr, &v1);
			br = tr | VT_LVAL | VT_REGDISP;
			fc = 0;
			bsym = NULL;
		} else if (fr < VT_CONST && !(br & VT_REGDISP)) { MCC_TRACE("br\n");
			br |= VT_REGDISP;
			fc = 0;
		}
		for (i = 0; i < 2; i++) { MCC_TRACE("br\n");
			SValue s;
			int e = (cls[0] == x86_64_mode_integer) ? 1 - i : i;
			int reg = (cls[e] == x86_64_mode_sse) ? MCC_TREG_XMM0 : MCC_TREG_RAX;
			s.type.ref = NULL;
			s.type.t = (cls[e] == x86_64_mode_sse) ? VT_DOUBLE : VT_LLONG;
			s.r = br;
			s.r2 = VT_CONST;
			s.c.i = fc + e * 8;
			s.sym = bsym;
			if (aftercall)
				{ MCC_TRACE("br\n"); store(reg, &s); }
			else
				{ MCC_TRACE("br\n"); load(reg, &s); }
		}
		return;
	}
	re = sv->type.ref->next;
	if (aftercall) { MCC_TRACE("br\n");
		o(0xdb);
		gen_modrm(7, fr, sv->sym, fc + re->c);
		o(0xdb);
		gen_modrm(7, fr, sv->sym, fc + re->next->c);
	} else { MCC_TRACE("br\n");
		o(0xdb);
		gen_modrm(5, fr, sv->sym, fc + re->next->c);
		o(0xdb);
		gen_modrm(5, fr, sv->sym, fc + re->c);
	}
}

#define REGN 6
static const uint8_t arg_regs[REGN] = {
		MCC_TREG_RDI, MCC_TREG_RSI, MCC_TREG_RDX, MCC_TREG_RCX, MCC_TREG_R8, MCC_TREG_R9};

static int arg_prepare_reg(int idx) { MCC_TRACE("enter\n");
	if (idx == 2 || idx == 3)
		{ MCC_TRACE("br\n"); return idx + 8; }
	else
		{ MCC_TRACE("br\n"); return idx >= 0 && idx < REGN ? arg_regs[idx] : 0; }
}

static int alloca_inline_on(void) { MCC_TRACE("enter\n");
	static int on = -1;
	if (on < 0) { MCC_TRACE("br\n");
		const char *e = getenv("MCC_ALLOCA_INLINE");
		on = e && e[0] ? (strcmp(e, "0") ? 1 : 0) : 1;
	}
	return on;
}

static int ovf_inline_on(void) { MCC_TRACE("enter\n");
	static int on = -1;
	if (on < 0) { MCC_TRACE("br\n");
		const char *e = getenv("MCC_OVERFLOW_INLINE");
		on = e && e[0] ? (strcmp(e, "0") ? 1 : 0) : 1;
	}
	return on;
}

static int gen_ovf_addsub(int nb_args) { MCC_TRACE("enter\n");
	const char *nm;
	int sub, uns, size, align, ra, rb, rr, sc;
	CType *pt;

	if (!ovf_inline_on() || nb_args != 3)
		{ MCC_TRACE("br\n"); return 0; }
	if (!(vtop[-3].r & VT_SYM) || !vtop[-3].sym)
		{ MCC_TRACE("br\n"); return 0; }
	nm = get_tok_str(vtop[-3].sym->v, NULL);
	if (!nm || strncmp(nm, "__mcc_", 6))
		{ MCC_TRACE("br\n"); return 0; }
	if (!strncmp(nm + 6, "addo_", 5))
		{ MCC_TRACE("br\n"); sub = 0; }
	else if (!strncmp(nm + 6, "subo_", 5))
		{ MCC_TRACE("br\n"); sub = 1; }
	else if (!strncmp(nm + 6, "mulo_", 5))
		{ MCC_TRACE("br\n"); sub = 2; }
	else
		{ MCC_TRACE("br\n"); return 0; }
	if ((vtop->type.t & VT_BTYPE) != VT_PTR)
		{ MCC_TRACE("br\n"); return 0; }
	pt = pointed_type(&vtop->type);
	size = type_size(pt, &align);
	if (size != 1 && size != 2 && size != 4 && size != 8)
		{ MCC_TRACE("br\n"); return 0; }

	if ((pt->t & VT_BTYPE) == VT_STRUCT || is_float(pt->t))
		{ MCC_TRACE("br\n"); return 0; }
	uns = (pt->t & VT_UNSIGNED) != 0;

	{
		int umul = sub == 2 && uns;
		gv(MCC_RC_RDI);
		rr = vtop->r & VT_VALMASK;
		vswap();
		gv(MCC_RC_RCX);
		rb = vtop->r & VT_VALMASK;
		vswap();
		vrotb(3);
		gv(umul ? MCC_RC_RAX : MCC_RC_RSI);
		ra = vtop->r & VT_VALMASK;
		vrott(3);

		save_reg(MCC_TREG_RDX);
		save_reg(MCC_TREG_RAX);
		sc = umul ? MCC_TREG_RSI : MCC_TREG_RDX;

		if (umul) { MCC_TRACE("br\n");
			orex(1, rb, 0, 0xf7);
			o(0xe0 + REG_VALUE(rb));
		} else if (sub == 2) { MCC_TRACE("br\n");
			orex(1, rb, ra, 0x0f);
			o(0xaf);
			o(0xc0 + REG_VALUE(ra) * 8 + REG_VALUE(rb));
		} else { MCC_TRACE("br\n");
			orex(1, rb, ra, sub ? 0x2b : 0x03);
			o(0xc0 + REG_VALUE(ra) * 8 + REG_VALUE(rb));
		}

		if (REG_VALUE(sc) >= 4 || REX_BASE(sc))
			{ MCC_TRACE("br\n"); o(0x40 | REX_BASE(sc)); }
		o(0x0f);
		o(uns ? 0x92 : 0x90);
		o(0xc0 + REG_VALUE(sc));

		if (size < 8) { MCC_TRACE("br\n");
			int rt = umul ? MCC_TREG_RDX : MCC_TREG_RAX;

			if (size == 4 && uns) { MCC_TRACE("br\n");
				orex(0, ra, rt, 0x8b);
				o(0xc0 + REG_VALUE(rt) * 8 + REG_VALUE(ra));
			} else if (size == 4) { MCC_TRACE("br\n");
				orex(1, ra, rt, 0x63);
				o(0xc0 + REG_VALUE(rt) * 8 + REG_VALUE(ra));
			} else { MCC_TRACE("br\n");
				orex(1, ra, rt, 0x0f);
				o(uns ? (size == 2 ? 0xb7 : 0xb6) : (size == 2 ? 0xbf : 0xbe));
				o(0xc0 + REG_VALUE(rt) * 8 + REG_VALUE(ra));
			}
			orex(1, rt, ra, 0x3b);
			o(0xc0 + REG_VALUE(ra) * 8 + REG_VALUE(rt));
			if (REG_VALUE(rb) >= 4 || REX_BASE(rb))
				{ MCC_TRACE("br\n"); o(0x40 | REX_BASE(rb)); }
			o(0x0f);
			o(0x95);
			o(0xc0 + REG_VALUE(rb));
			if (REG_VALUE(rb) >= 4 || REG_VALUE(sc) >= 4 || REX_BASE(rb) ||
					REX_BASE(sc))
				{ MCC_TRACE("br\n"); o(0x40 | REX_BASE(sc) | (REX_BASE(rb) << 2)); }
			o(0x08);
			o(0xc0 + REG_VALUE(rb) * 8 + REG_VALUE(sc));
		}

		if (size == 1) { MCC_TRACE("br\n");
			o(0x40 | REX_BASE(rr) | (REX_BASE(ra) << 2));
			o(0x88);
			gen_modrm(ra, rr, NULL, 0);
		} else { MCC_TRACE("br\n");
			if (size == 2)
				{ MCC_TRACE("br\n"); o(0x66); }
			orex(size == 8, rr, ra, 0x89);
			gen_modrm(ra, rr, NULL, 0);
		}
	}

	if (REG_VALUE(sc) >= 4 || REX_BASE(sc))
		{ MCC_TRACE("br\n"); o(0x40 | REX_BASE(sc)); }
	o(0x0f);
	o(0xb6);
	o(0xc0 + REG_VALUE(MCC_TREG_RAX) * 8 + REG_VALUE(sc));
	vtop -= 4;
	return 1;
}

static int gen_alloca_inline(int nb_args) { MCC_TRACE("enter\n");
	int r;

	if (!alloca_inline_on() || nb_args != 1)
		{ MCC_TRACE("br\n"); return 0; }
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); return 0; }
	if (!(vtop[-1].r & VT_SYM) || !vtop[-1].sym ||
			(vtop[-1].sym->v != TOK_alloca && vtop[-1].sym->asm_label != TOK_alloca))
		{ MCC_TRACE("br\n"); return 0; }
	if ((vtop->type.t & VT_BTYPE) == VT_STRUCT || is_float(vtop->type.t))
		{ MCC_TRACE("br\n"); return 0; }

	gen_cast_s(VT_SIZE_T);
	vpushi(15);
	gen_op('+');
	vpushi(-16);
	gen_op('&');
	r = gv(MCC_RC_RAX);
	o(0x2b48);
	o(0xe0 | REG_VALUE(r));
	o(0x8948);
	o(0xe0 | REG_VALUE(r));
	vtop -= 2;
	return 1;
}

void gfunc_call(int nb_args) { MCC_TRACE("enter\n");
	X86_64_Mode mode;
	X86_64_Mode cls[2];
	CType type;
	int size, align, r, args_size, stack_adjust, reg_count;
	int nb_reg_args = 0;
	int nb_sse_args = 0;
	int sse_reg, gen_reg;
	int *onstack;

	if (gen_alloca_inline(nb_args))
		{ MCC_TRACE("br\n"); return; }
	if (gen_ovf_addsub(nb_args))
		{ MCC_TRACE("br\n"); return; }
	onstack = mcc_malloc((nb_args + 1) * sizeof(int));

	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gbound_args(nb_args); }

	save_regs(nb_args);

	stack_adjust = 0;
	for (int i = nb_args - 1; i >= 0; i--) { MCC_TRACE("br\n");
		mode = classify_x86_64_arg(&vtop[-i].type, NULL, &size, &align, &reg_count);
		if (size == 0)
			{ MCC_TRACE("br\n"); continue; }
		if (x86_64_mixed_class(&vtop[-i].type, cls)) { MCC_TRACE("br\n");
			if (nb_reg_args + 1 <= REGN && nb_sse_args + 1 <= 8) { MCC_TRACE("br\n");
				nb_reg_args++;
				nb_sse_args++;
				onstack[i] = 0;
			} else { MCC_TRACE("br\n");
				int pad = ((stack_adjust + align - 1) & -align) - stack_adjust;
				onstack[i] = 1 + pad / 8;
				stack_adjust += pad + size;
			}
		} else if ((mode == x86_64_mode_sse || mode == x86_64_mode_sseup) &&
				nb_sse_args + reg_count <= 8) { MCC_TRACE("br\n");
			nb_sse_args += reg_count;
			onstack[i] = 0;
		} else if (mode == x86_64_mode_integer && nb_reg_args + reg_count <= REGN) { MCC_TRACE("br\n");
			nb_reg_args += reg_count;
			onstack[i] = 0;
		} else if (mode == x86_64_mode_none) { MCC_TRACE("br\n");
			onstack[i] = 0;
		} else { MCC_TRACE("br\n");
			int pad = ((stack_adjust + align - 1) & -align) - stack_adjust;
			onstack[i] = 1 + pad / 8;
			stack_adjust += pad + size;
		}
	}

	if (nb_sse_args && mcc_state->nosse)
		{ MCC_TRACE("br\n"); mcc_error("SSE disabled but floating point arguments passed"); }

	gen_reg = nb_reg_args;
	sse_reg = nb_sse_args;
	args_size = 0;
	stack_adjust = -stack_adjust & 15;
	for (int i = 0, k = 0; i < nb_args;) { MCC_TRACE("br\n");
		mode = classify_x86_64_arg(&vtop[-i].type, NULL, &size, &align, &reg_count);
		if (size) { MCC_TRACE("br\n");
			if (!onstack[i + k]) { MCC_TRACE("br\n");
				++i;
				continue;
			}
			while (stack_adjust > 0) { MCC_TRACE("br\n");
				o(0x50);
				args_size += 8;
				stack_adjust -= 8;
			}
			stack_adjust = (onstack[i + k] - 1) * 8;
		}

		vrotb(i + 1);

		switch (vtop->type.t & VT_BTYPE) { MCC_TRACE("br\n");
		case VT_STRUCT:
			o(0x48);
			oad(0xec81, size);
			r = get_reg(MCC_RC_INT);
			orex(1, r, 0, 0x89);
			o(0xe0 + REG_VALUE(r));
			vset(&vtop->type, r | VT_LVAL, 0);
			vswap();
			o(0x10ec8348);
			o(0xf0e48348);
			orex(0, r, 0, 0x50 + REG_VALUE(r));
			o(0x08ec8348);
			vstore();
			o(0x08c48348);
			o(0x5c);
			break;

		case VT_LDOUBLE:
			gv(MCC_RC_ST0);
			oad(0xec8148, size);
			o(0x7cdb);
			g(0x24);
			g(0x00);

			vtop->r = VT_CONST;
			break;

		case VT_INT128:
			r = gv(MCC_RC_INT);
			orex(0, vtop->r2, 0, 0x50 + REG_VALUE(vtop->r2));
			orex(0, r, 0, 0x50 + REG_VALUE(r));
			break;

		case VT_FLOAT:
		case VT_DOUBLE:
			assert(mode == x86_64_mode_sse);
			r = gv(MCC_RC_FLOAT);
			o(0x50);
			o(0x66);
			sse_rex(r, MCC_TREG_RSP);
			o(0xd60f);
			o(0x04 + REG_VALUE(r) * 8);
			o(0x24);
			break;

		default:
			assert(mode == x86_64_mode_integer);
			r = gv(MCC_RC_INT);
			orex(0, r, 0, 0x50 + REG_VALUE(r));
			break;
		}
		args_size += size;

		vpop();
		--nb_args;
		k++;
	}

	mcc_free(onstack);

	assert(gen_reg <= REGN);
	assert(sse_reg <= 8);
	for (int i = 0; i < nb_args; i++) { MCC_TRACE("br\n");
		mode = classify_x86_64_arg(&vtop->type, &type, &size, &align, &reg_count);
		if (size == 0)
			{ MCC_TRACE("br\n"); continue; }
		if (x86_64_mixed_class(&vtop->type, cls)) { MCC_TRACE("br\n");
			int gp_first = cls[0] == x86_64_mode_integer;
			int lo, hi, gr, sr, d, x;
			vtop->type.t = VT_QLONG;
			vtop->type.ref = NULL;
			lo = gv(MCC_RC_INT);
			hi = vtop->r2;
			gr = gp_first ? lo : hi;
			sr = gp_first ? hi : lo;
			--gen_reg;
			d = arg_prepare_reg(gen_reg);
			orex(1, d, gr, 0x89);
			o(0xc0 + REG_VALUE(gr) * 8 + REG_VALUE(d));
			--sse_reg;
			x = MCC_TREG_XMM0 + sse_reg;
			o(0x66);
			orex(1, sr, x, 0x0f);
			o(0x6e);
			o(0xc0 + REG_VALUE(x) * 8 + REG_VALUE(sr));
			vtop--;
			continue;
		}
		if (mode == x86_64_mode_sseup) { MCC_TRACE("br\n");
			--sse_reg;
			x86_64_vec16_move(MCC_TREG_XMM0 + sse_reg, vtop, 0);
			vtop--;
			continue;
		}
		vtop->type = type;
		if (mode == x86_64_mode_sse) { MCC_TRACE("br\n");
			if (reg_count == 2) { MCC_TRACE("br\n");
				sse_reg -= 2;
				gv(MCC_RC_FRET);
				if (sse_reg) { MCC_TRACE("br\n");
					o(0x280f);
					o(0xc1 + ((sse_reg + 1) << 3));
					o(0x280f);
					o(0xc0 + (sse_reg << 3));
				}
			} else { MCC_TRACE("br\n");
				assert(reg_count == 1);
				--sse_reg;
				gv(MCC_RC_XMM0 << sse_reg);
			}
		} else if (mode == x86_64_mode_integer) { MCC_TRACE("br\n");
			int d;
			gen_reg -= reg_count;
			r = gv(MCC_RC_INT);
			d = arg_prepare_reg(gen_reg);
			orex(1, d, r, 0x89);
			o(0xc0 + REG_VALUE(r) * 8 + REG_VALUE(d));
			if (reg_count == 2) { MCC_TRACE("br\n");
				d = arg_prepare_reg(gen_reg + 1);
				orex(1, d, vtop->r2, 0x89);
				o(0xc0 + REG_VALUE(vtop->r2) * 8 + REG_VALUE(d));
			}
		}
		vtop--;
	}
	assert(gen_reg == 0);
	assert(sse_reg == 0);

	if (nb_reg_args > 2) { MCC_TRACE("br\n");
		o(0xd2894c);
		if (nb_reg_args > 3) { MCC_TRACE("br\n");
			o(0xd9894c);
		}
	}

	if (vtop->type.ref->f.func_type != FUNC_NEW)
		{ MCC_TRACE("br\n"); oad(0xb8, nb_sse_args < 8 ? nb_sse_args : 8); }
	gcall_or_jmp(0);
	if (args_size)
		{ MCC_TRACE("br\n"); gadd_sp(args_size); }
	vtop--;
}

#define FUNC_PROLOG_SIZE 11

static void push_arg_reg(int i) { MCC_TRACE("enter\n");
	loc -= 8;
	gen_modrm64(0x89, arg_regs[i], VT_LOCAL, NULL, loc);
}

#if defined(MCC_TARGET_MACHO)

static void gen_stack_chk_prolog(void) { MCC_TRACE("enter\n");
	Sym *guard = external_helper_sym(TOK___stack_chk_guard);
	func_stack_chk_loc = (loc -= 8);
	o(0x058b48);
	gen_gotpcrel(MCC_TREG_RAX, guard, 0);
	g(0x48);
	g(0x8b);
	g(0x00);
	g(0x48);
	g(0x89);
	g(0x85);
	gen_le32(func_stack_chk_loc);
}

static void gen_stack_chk_epilog(void) { MCC_TRACE("enter\n");
	Sym *guard = external_helper_sym(TOK___stack_chk_guard);

	g(0x48);
	g(0x8b);
	g(0x8d);
	gen_le32(func_stack_chk_loc);
	o(0x158b48);
	gen_gotpcrel(MCC_TREG_RDX, guard, 0);
	g(0x48);
	g(0x33);
	g(0x0a);
	g(0x74);
	g(0x05);
	oad(0xe8, 0);
	greloca(cur_text_section, external_helper_sym(TOK___stack_chk_fail),
					ind - 4, R_X86_64_PLT32, -4);
}
#elif !defined(MCC_TARGET_PE)
static void gen_stack_chk_prolog(void) { MCC_TRACE("enter\n");
	func_stack_chk_loc = (loc -= 8);
	g(0x64);
	g(0x48);
	g(0x8b);
	g(0x04);
	g(0x25);
	gen_le32(0x28);
	g(0x48);
	g(0x89);
	g(0x85);
	gen_le32(func_stack_chk_loc);
}

static void gen_stack_chk_epilog(void) { MCC_TRACE("enter\n");
	g(0x48);
	g(0x8b);
	g(0x8d);
	gen_le32(func_stack_chk_loc);
	g(0x64);
	g(0x48);
	g(0x33);
	g(0x0c);
	g(0x25);
	gen_le32(0x28);
	g(0x74);
	g(0x05);
	oad(0xe8, 0);
	greloca(cur_text_section, external_helper_sym(TOK___stack_chk_fail),
					ind - 4, R_X86_64_PLT32, -4);
}
#endif

void gfunc_prolog(Sym *func_sym) { MCC_TRACE("enter\n");
	CType *func_type = &func_sym->type;
	X86_64_Mode mode, ret_mode;
	if (func_naked)
		{ MCC_TRACE("br\n"); return; }
	X86_64_Mode cls[2];
	int addr, align, size, reg_count;
	int param_addr = 0, reg_param_index, sse_param_index;
	Sym *sym;
	CType *type;

	sym = func_type->ref;
	addr = MCC_PTR_SIZE * 2;
	loc = 0;
	ind += FUNC_PROLOG_SIZE;
	func_sub_sp_offset = ind;
	func_ret_sub = 0;
	ret_mode = classify_x86_64_arg(&func_vt, NULL, &size, &align, &reg_count);

	if (func_var) { MCC_TRACE("br\n");
		int seen_reg_num, seen_sse_num, seen_stack_size;
		seen_reg_num = ret_mode == x86_64_mode_memory && !x86_64_complex_ldouble(&func_vt);
		seen_sse_num = 0;
		seen_stack_size = MCC_PTR_SIZE * 2;
		sym = func_type->ref;
		while ((sym = sym->next) != NULL) { MCC_TRACE("br\n");
			type = &sym->type;
			mode = classify_x86_64_arg(type, NULL, &size, &align, &reg_count);
			if (x86_64_mixed_class(type, cls)) { MCC_TRACE("br\n");
				if (seen_reg_num + 1 > REGN || seen_sse_num + 1 > 8)
					{ MCC_TRACE("br\n"); goto stack_arg; }
				seen_reg_num += 1;
				seen_sse_num += 1;
				continue;
			}
			switch (mode) { MCC_TRACE("br\n");
			default:
			stack_arg:
				seen_stack_size = x86_64_stack_arg_align(seen_stack_size, align) + size;
				break;

			case x86_64_mode_integer:
				if (seen_reg_num + reg_count > REGN)
					{ MCC_TRACE("br\n"); goto stack_arg; }
				seen_reg_num += reg_count;
				break;

			case x86_64_mode_sse:
			case x86_64_mode_sseup:
				if (seen_sse_num + reg_count > 8)
					{ MCC_TRACE("br\n"); goto stack_arg; }
				seen_sse_num += reg_count;
				break;
			}
		}

		loc -= 24;
		o(0xe845c7);
		gen_le32(seen_reg_num * 8);
		o(0xec45c7);
		gen_le32(seen_sse_num * 16 + 48);
		o(0x9d8d4c);
		gen_le32(seen_stack_size);
		o(0xf05d894c);
		o(0x9d8d4c);
		gen_le32(-176 - 24);
		o(0xf85d894c);

		for (int i = 0; i < 8; i++) { MCC_TRACE("br\n");
			loc -= 16;
			if (!mcc_state->nosse) { MCC_TRACE("br\n");
				o(0x110f);
				gen_modrm(7 - i, VT_LOCAL, NULL, loc);
			} else { MCC_TRACE("br\n");
				o(0x85c748);
				gen_le32(loc + 8);
				gen_le32(0);
			}
		}
		for (int i = 0; i < REGN; i++) { MCC_TRACE("br\n");
			push_arg_reg(REGN - 1 - i);
		}
	}

	sym = func_type->ref;
	reg_param_index = 0;
	sse_param_index = 0;

	if (ret_mode == x86_64_mode_memory && !x86_64_complex_ldouble(&func_vt)) { MCC_TRACE("br\n");
		push_arg_reg(reg_param_index);
		func_vc = loc;
		reg_param_index++;
	}
	while ((sym = sym->next) != NULL) { MCC_TRACE("br\n");
		type = &sym->type;
		mode = classify_x86_64_arg(type, NULL, &size, &align, &reg_count);
		if (x86_64_mixed_class(type, cls)) { MCC_TRACE("br\n");
			if (reg_param_index + 1 <= REGN && sse_param_index + 1 <= 8) { MCC_TRACE("br\n");
				int ebint = (cls[0] == x86_64_mode_integer) ? 0 : 1;
				loc -= 16;
				param_addr = loc;
				gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, param_addr + ebint * 8);
				++reg_param_index;
				o(0xd60f66);
				gen_modrm(sse_param_index, VT_LOCAL, NULL, param_addr + (1 - ebint) * 8);
				++sse_param_index;
			} else { MCC_TRACE("br\n");
				addr = x86_64_stack_arg_align(addr, align);
				param_addr = addr;
				addr += size;
			}
			gfunc_set_param(sym, param_addr, 0);
			continue;
		}
		switch (mode) { MCC_TRACE("br\n");
		case x86_64_mode_sseup:
			if (mcc_state->nosse)
				{ MCC_TRACE("br\n"); mcc_error("SSE disabled but floating point arguments used"); }
			if (sse_param_index + 1 <= 8) { MCC_TRACE("br\n");
				loc = (loc - 16) & -16;
				param_addr = loc;
				o(0x110f);
				gen_modrm(sse_param_index, VT_LOCAL, NULL, param_addr);
				++sse_param_index;
			} else { MCC_TRACE("br\n");
				addr = x86_64_stack_arg_align(addr, align);
				param_addr = addr;
				addr += size;
			}
			break;

		case x86_64_mode_sse:
			if (mcc_state->nosse)
				{ MCC_TRACE("br\n"); mcc_error("SSE disabled but floating point arguments used"); }
			if (sse_param_index + reg_count <= 8) { MCC_TRACE("br\n");
				loc -= reg_count * 8;
				param_addr = loc;
				for (int i = 0; i < reg_count; ++i) { MCC_TRACE("br\n");
					o(0xd60f66);
					gen_modrm(sse_param_index, VT_LOCAL, NULL, param_addr + i * 8);
					++sse_param_index;
				}
			} else { MCC_TRACE("br\n");
				addr = x86_64_stack_arg_align(addr, align);
				param_addr = addr;
				addr += size;
			}
			break;

		case x86_64_mode_memory:
		case x86_64_mode_x87:
			addr = x86_64_stack_arg_align(addr, align);
			param_addr = addr;
			addr += size;
			break;

		case x86_64_mode_integer: {
			if (reg_param_index + reg_count <= REGN) { MCC_TRACE("br\n");
				loc -= reg_count * 8;
				param_addr = loc;
				for (int i = 0; i < reg_count; ++i) { MCC_TRACE("br\n");
					gen_modrm64(0x89, arg_regs[reg_param_index], VT_LOCAL, NULL, param_addr + i * 8);
					++reg_param_index;
				}
			} else { MCC_TRACE("br\n");
				addr = x86_64_stack_arg_align(addr, align);
				param_addr = addr;
				addr += size;
			}
			break;
		}
		default:
			break;
		}
		gfunc_set_param(sym, param_addr, 0);
	}

	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gen_bounds_prolog(); }
#ifndef MCC_TARGET_PE
	if (mcc_state->do_asan_shadow)
		{ MCC_TRACE("br\n"); gen_asan_stack_prolog(); }
	func_stack_chk_loc = 0;
	if (mcc_state->stack_protector)
		{ MCC_TRACE("br\n"); gen_stack_chk_prolog(); }
#endif
}

void gfunc_epilog(void) { MCC_TRACE("enter\n");
	int v, saved_ind;
	if (func_naked)
		{ MCC_TRACE("br\n"); return; }

	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gen_bounds_epilog(); }
#ifndef MCC_TARGET_PE
	if (mcc_state->do_asan_shadow)
		{ MCC_TRACE("br\n"); gen_asan_stack_epilog(); }
	if (func_stack_chk_loc)
		{ MCC_TRACE("br\n"); gen_stack_chk_epilog(); }
#endif
	o(0xc9);
	if (func_ret_sub == 0) { MCC_TRACE("br\n");
		o(0xc3);
	} else { MCC_TRACE("br\n");
		o(0xc2);
		g(func_ret_sub);
		g(func_ret_sub >> 8);
	}
	v = (-loc + 15) & -16;
	saved_ind = ind;
	ind = func_sub_sp_offset - FUNC_PROLOG_SIZE;
	o(0xe5894855);
	o(0xec8148);
	gen_le32(v);
	ind = saved_ind;
}

#endif

ST_FUNC void gen_fill_nops(int bytes) { MCC_TRACE("enter\n");
	while (bytes--)
		{ MCC_TRACE("br\n"); g(0x90); }
}

int gjmp(int t) { MCC_TRACE("enter\n");
	return gjmp2(0xe9, t);
}

void gjmp_addr(int a) { MCC_TRACE("enter\n");
	int r;
	r = a - ind - 2;
	if (r == (signed char)r) { MCC_TRACE("br\n");
		g(0xeb);
		g(r);
	} else { MCC_TRACE("br\n");
		oad(0xe9, a - ind - 5);
	}
}

ST_FUNC int gen_cmov(int rt, int rf, int rb, int ll) { MCC_TRACE("enter\n");
	orex(ll, rb, rb, 0x85);
	o(0xc0 + REG_VALUE(rb) + REG_VALUE(rb) * 8);
	orex(ll, rt, rf, 0x450f);
	o(0xc0 + REG_VALUE(rt) + REG_VALUE(rf) * 8);
	return rf;
}

ST_FUNC int gjmp_cond(int op, int t) { MCC_TRACE("enter\n");
	if (op & 0x100) { MCC_TRACE("br\n");
		int v = vtop->cmp_r;
		op &= ~0x100;
		if (op ^ v ^ (v != TOK_NE))
			{ MCC_TRACE("br\n"); o(0x067a); }
		else { MCC_TRACE("br\n");
			g(0x0f);
			t = gjmp2(0x8a, t);
		}
	}
	g(0x0f);
	t = gjmp2(op - 16, t);
	return t;
}

enum { UBK_OVERFLOW, UBK_DIVREM, UBK_SHIFT, UBK_NULLPTR, UBK_SUB, UBK_MUL };

static const char *ubsan_recover_sym(int kind) { MCC_TRACE("enter\n");
	switch (kind) { MCC_TRACE("br\n");
	case UBK_DIVREM:  { MCC_TRACE("br\n"); return "__ubsan_handle_divrem_overflow_minimal"; }
	case UBK_SHIFT:   { MCC_TRACE("br\n"); return "__ubsan_handle_shift_out_of_bounds_minimal"; }
	case UBK_NULLPTR: { MCC_TRACE("br\n"); return "__ubsan_handle_type_mismatch_v1_minimal"; }
	case UBK_SUB:     { MCC_TRACE("br\n"); return "__ubsan_handle_sub_overflow_minimal"; }
	case UBK_MUL:     { MCC_TRACE("br\n"); return "__ubsan_handle_mul_overflow_minimal"; }
	default:          { MCC_TRACE("br\n"); return "__ubsan_handle_add_overflow_minimal"; }
	}
}

static int ubsan_kind_recovers(int kind) { MCC_TRACE("enter\n");
	unsigned m = mcc_state->do_sanitize_recover;
	switch (kind) { MCC_TRACE("br\n");
	case UBK_SHIFT:   { MCC_TRACE("br\n"); return (m & MCC_SANR_SHIFT) != 0; }
	case UBK_DIVREM:  { MCC_TRACE("br\n"); return (m & MCC_SANR_DIVREM) != 0; }
	case UBK_NULLPTR: { MCC_TRACE("br\n"); return (m & MCC_SANR_NULLPTR) != 0; }
	default:          { MCC_TRACE("br\n"); return (m & MCC_SANR_OVERFLOW) != 0; }
	}
}

static void gen_ubsan_trap_or_call(int kind) { MCC_TRACE("enter\n");
	if (ubsan_kind_recovers(kind)) { MCC_TRACE("br\n");
		Sym *sym = external_helper_sym(tok_alloc_const(ubsan_recover_sym(kind)));
		g(0x50); g(0x51); g(0x52); g(0x56); g(0x57);
		g(0x41); g(0x50); g(0x41); g(0x51); g(0x41); g(0x52); g(0x41); g(0x53);
		g(0x53);
		g(0x48); g(0x89); g(0xe3);
		g(0x48); g(0x83); g(0xe4); g(0xf0);
		g(0x48); g(0x83); g(0xec); g(0x20);
		oad(0xe8, 0);
		greloca(cur_text_section, sym, ind - 4, R_X86_64_PLT32, -4);
		g(0x48); g(0x89); g(0xdc);
		g(0x5b);
		g(0x41); g(0x5b); g(0x41); g(0x5a); g(0x41); g(0x59); g(0x41); g(0x58);
		g(0x5f); g(0x5e); g(0x5a); g(0x59); g(0x58);
		return;
	}
	o(0x0b0f);
}

static void gen_ubsan_check_k(int cc, int kind) { MCC_TRACE("enter\n");
	int t;
	g(0x0f);
	t = gjmp2(cc, 0);
	gen_ubsan_trap_or_call(kind);
	gsym(t);
}

static void gen_blind_boundary_guard(void) { MCC_TRACE("enter\n");
	Sym *sym;
	int t;
	if (!ast_reemit_guard_op || nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	g(0x0f);
	t = gjmp2(0x81, 0);
	sym = external_helper_sym(tok_alloc_const("mccjit_boundary_hit"));
	o(0xc7); o(0x05);
	greloca(cur_text_section, sym, ind, R_X86_64_PC32, -8);
	gen_le32(0);
	gen_le32(1);
	gsym(t);
}

void gen_ubsan_nullptr(void) { MCC_TRACE("enter\n");
	int r;
	if (!mcc_state->do_sanitize_undefined || nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	if ((vtop->r & VT_VALMASK) >= VT_CONST)
		{ MCC_TRACE("br\n"); return; }
	r = vtop->r & VT_VALMASK;
	orex(1, r, r, 0x85);
	o(0xc0 + REG_VALUE(r) * 9);
	gen_ubsan_check_k(0x85, UBK_NULLPTR);
}

void gen_trap(void) { MCC_TRACE("enter\n");
	o(0x0b0f);
}

static Section *asan_type_sec;
static int asan_type_patch = -1;
static int asan_type_end;

void gen_asan_mark_write(void) { MCC_TRACE("enter\n");
	if (asan_type_patch < 0 || !cur_text_section)
		{ MCC_TRACE("br\n"); return; }
	if (cur_text_section != asan_type_sec || ind < asan_type_end) { MCC_TRACE("br\n");
		asan_type_patch = -1;
		return;
	}
	if (cur_text_section->data[asan_type_patch] != 0x40) { MCC_TRACE("br\n");
		asan_type_patch = -1;
		return;
	}
	cur_text_section->data[asan_type_patch] = 0xc0;
	asan_type_patch = -1;
}

void gen_asan_shadow_check(int sz) { MCC_TRACE("enter\n");
	int r, t = 0;
	asan_type_patch = -1;
	if (!mcc_state->do_asan_shadow || nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	if ((vtop->r & VT_VALMASK) >= VT_CONST || sz <= 0 || sz > 8)
		{ MCC_TRACE("br\n"); return; }
	r = vtop->r & VT_VALMASK;
	g(0x50);
	g(0x52);
	g(0x51);
	orex(1, 1, r, 0x89);
	o(0xc0 | REG_VALUE(r) << 3 | 1);
	orex(1, 2, r, 0x89);
	o(0xc0 | REG_VALUE(r) << 3 | 2);
	orex(1, 0, r, 0x89);
	o(0xc0 | REG_VALUE(r) << 3 | 0);
	g(0x48);
	g(0xc1);
	g(0xe8);
	g(0x03);
	g(0x0f);
	g(0xbe);
	g(0x80);
	gen_le32(0x7fff8000);
	g(0x85);
	g(0xc0);
	g(0x0f);
	t = gjmp2(0x84, t);
	g(0x83);
	g(0xe2);
	g(0x07);
	g(0x83);
	g(0xc2);
	g(sz - 1);
	g(0x39);
	g(0xc2);
	g(0x0f);
	t = gjmp2(0x8c, t);
	g(0x83);
	g(0xca);
	asan_type_sec = cur_text_section;
	asan_type_patch = ind;
	g(0x40);
	o(0x0b0f);
	gsym(t);
	g(0x59);
	g(0x5a);
	g(0x58);
	asan_type_end = ind;
}

void gen_opi(int op) { MCC_TRACE("enter\n");
	int r, fr, opc, c;
	int ll, uu, cc;

	ll = is64_type(vtop[-1].type.t);
	uu = (vtop[-1].type.t & VT_UNSIGNED) != 0;
	cc = (vtop->r & (VT_VALMASK | VT_LVAL | VT_SYM)) == VT_CONST;

	switch (op) { MCC_TRACE("br\n");
	case '+':
	case TOK_ADDC1:
		opc = 0;
	gen_op8:
		if (cc && (!ll || (int)vtop->c.i == vtop->c.i)) { MCC_TRACE("br\n");
			vswap();
			r = gv(MCC_RC_INT);
			vswap();
			c = vtop->c.i;
			if (c == (signed char)c) { MCC_TRACE("br\n");
				orex(ll, r, 0, 0x83);
				o(0xc0 | (opc << 3) | REG_VALUE(r));
				g(c);
			} else { MCC_TRACE("br\n");
				orex(ll, r, 0, 0x81);
				oad(0xc0 | (opc << 3) | REG_VALUE(r), c);
			}
		} else { MCC_TRACE("br\n");
			gv2(MCC_RC_INT, MCC_RC_INT);
			r = vtop[-1].r;
			fr = vtop[0].r;
			orex(ll, r, fr, (opc << 3) | 0x01);
			o(0xc0 + REG_VALUE(r) + REG_VALUE(fr) * 8);
		}
		if (mcc_state->do_sanitize_undefined && !nocode_wanted && !uu &&
				(op == '+' || op == '-'))
			{ MCC_TRACE("br\n"); gen_ubsan_check_k(0x81, op == '-' ? UBK_SUB : UBK_OVERFLOW); }
		if ((op == '+' || op == '-') && !ll)
			{ MCC_TRACE("br\n"); gen_blind_boundary_guard(); }
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
		if (cc && mcc_state && mcc_state->optimize >= 1 &&
				!mcc_state->do_sanitize_undefined && !ast_reemit_guard_op &&
				(!ll || (int64_t)(int32_t)vtop->c.i == (int64_t)vtop->c.i) &&
				(vtop->c.i == 3 || vtop->c.i == 5 || vtop->c.i == 9)) { MCC_TRACE("br\n");
			int sc = vtop->c.i == 3 ? 1 : (vtop->c.i == 5 ? 2 : 3);
			int rv, ext;
			vtop--;
			r = gv(MCC_RC_INT);
			rv = REG_VALUE(r);
			ext = REX_BASE(r);
			if (rv != 4 && rv != 5) { MCC_TRACE("br\n");
				if (ll || ext)
					{ MCC_TRACE("br\n"); o(0x40 | (ll << 3) | (ext ? 7 : 0)); }
				o(0x8d);
				o(0x04 | (rv << 3));
				o((sc << 6) | (rv << 3) | rv);
				break;
			}
			orex(ll, r, r, 0x69);
			oad(0xc0 | (REG_VALUE(r) << 3) | REG_VALUE(r),
					sc == 1 ? 3 : (sc == 2 ? 5 : 9));
			break;
		}
		gv2(MCC_RC_INT, MCC_RC_INT);
		r = vtop[-1].r;
		fr = vtop[0].r;
		orex(ll, fr, r, 0xaf0f);
		o(0xc0 + REG_VALUE(fr) + REG_VALUE(r) * 8);
		if (mcc_state->do_sanitize_undefined && !nocode_wanted && !uu)
			{ MCC_TRACE("br\n"); gen_ubsan_check_k(0x81, UBK_MUL); }
		if (!ll)
			{ MCC_TRACE("br\n"); gen_blind_boundary_guard(); }
		vtop--;
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
		if (cc) { MCC_TRACE("br\n");
			vswap();
			r = gv(MCC_RC_INT);
			vswap();
			orex(ll, r, 0, 0xc1);
			o(opc | REG_VALUE(r));
			g(vtop->c.i & (ll ? 63 : 31));
		} else { MCC_TRACE("br\n");
			gv2(MCC_RC_INT, MCC_RC_RCX);
			r = vtop[-1].r;
			if (mcc_state->do_sanitize_undefined && !nocode_wanted) { MCC_TRACE("br\n");
				orex(0, MCC_TREG_RCX, 0, 0x83);
				o((0xc0 | (7 << 3)) + REG_VALUE(MCC_TREG_RCX));
				g(ll ? 64 : 32);
				gen_ubsan_check_k(0x82, UBK_SHIFT);
			}
			orex(ll, r, 0, 0xd3);
			o(opc | REG_VALUE(r));
		}
		vtop--;
		break;
	case TOK_UDIV:
	case TOK_UMOD:
		uu = 1;
		goto divmod;
	case '/':
	case '%':
	case TOK_PDIV:
		uu = 0;
	divmod:
		gv2(MCC_RC_RAX, MCC_RC_RCX);
		r = vtop[-1].r;
		fr = vtop[0].r;
		vtop--;
		save_reg(MCC_TREG_RDX);
		if (mcc_state->do_sanitize_undefined && !nocode_wanted) { MCC_TRACE("br\n");
			orex(ll, fr, fr, 0x85);
			o(0xc0 + REG_VALUE(fr) * 9);
			gen_ubsan_check_k(0x85, UBK_DIVREM);
		}
		orex(ll, 0, 0, uu ? 0xd231 : 0x99);
		orex(ll, fr, 0, 0xf7);
		o((uu ? 0xf0 : 0xf8) + REG_VALUE(fr));
		if (op == '%' || op == TOK_UMOD)
			{ MCC_TRACE("br\n"); r = MCC_TREG_RDX; }
		else
			{ MCC_TRACE("br\n"); r = MCC_TREG_RAX; }
		vtop->r = r;
		break;
	default:
		opc = 7;
		goto gen_op8;
	}
}

void gen_opl(int op) { MCC_TRACE("enter\n");
	gen_opi(op);
}

void gen_mulh(int sign) { MCC_TRACE("enter\n");
	int fr, ll;
	ll = is64_type(vtop[-1].type.t);
	gv2(MCC_RC_RAX, MCC_RC_RCX);
	fr = vtop[0].r;
	vtop--;
	save_reg(MCC_TREG_RDX);
	orex(ll, fr, 0, 0xf7);
	o((sign ? 0xe8 : 0xe0) + REG_VALUE(fr));
	vtop->r = MCC_TREG_RDX;
}

void gen_mul_widen(void) { MCC_TRACE("enter\n");
	int fr, ll;
	ll = is64_type(vtop[-1].type.t);
	gv2(MCC_RC_RAX, MCC_RC_RCX);
	fr = vtop[0].r;
	vtop--;
	save_reg(MCC_TREG_RDX);
	orex(ll, fr, 0, 0xf7);
	o(0xe0 + REG_VALUE(fr));
	vtop->r = MCC_TREG_RAX;
	vtop->r2 = MCC_TREG_RDX;
}

void gen_reg_addi(int r, int64_t d) { MCC_TRACE("enter\n");
	int rv = REG_VALUE(r);
	orex(1, r, r, 0x8d);
	g(0x80 | (rv << 3) | rv);
	if (rv == 4)
		{ MCC_TRACE("br\n"); g(0x24); }
	gen_le32((int)d);
}

void gen_fabs(void) { MCC_TRACE("enter\n");
	int bt = vtop->type.t & VT_BTYPE;
	int dbl = bt == VT_DOUBLE;
	int r, mreg, m, pinned;
	if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
		gv(MCC_RC_ST0);
		o(0xe1d9);
		return;
	}
	gv(MCC_RC_FLOAT);
	r = vtop->r & VT_VALMASK;
	pinned = (ast_pinned_regs & ((uint64_t)1 << r)) != 0;
	mreg = get_reg(MCC_RC_FLOAT);
	m = REG_VALUE(mreg);
	o(0x66); sse_rex(mreg, mreg); o(0x760f); o(0xc0 | (m << 3) | m);
	o(0x66); sse_rex(0, mreg);
	o(dbl ? 0x730f : 0x720f); o(0xd0 | m); o(1);
	if (dbl)
		{ MCC_TRACE("br\n"); o(0x66); }
	if (pinned) { MCC_TRACE("br\n");
		sse_rex(mreg, r); o(0x540f); o(0xc0 | (m << 3) | REG_VALUE(r));
		vtop->r = mreg;
	} else { MCC_TRACE("br\n");
		sse_rex(r, mreg); o(0x540f); o(0xc0 | (REG_VALUE(r) << 3) | m);
	}
}

void gen_atomic_cmpxchg(int size) { MCC_TRACE("enter\n");
	int rp, re, rd, sc, t;
#ifdef MCC_TARGET_PE
	const int rc_re = MCC_RC_R8, rc_rp = MCC_RC_R9;
#else
	const int rc_re = MCC_RC_RSI, rc_rp = MCC_RC_RDI;
#endif

	gv(MCC_RC_RCX);
	rd = vtop->r & VT_VALMASK;
	vswap();
	gv(rc_re);
	re = vtop->r & VT_VALMASK;
	vswap();
	vrotb(3);
	gv(rc_rp);
	rp = vtop->r & VT_VALMASK;
	vrott(3);

	save_reg(MCC_TREG_RAX);
	save_reg(MCC_TREG_RDX);
	sc = MCC_TREG_RDX;

	orex(size == 8, re, MCC_TREG_RAX, 0x8b);
	gen_modrm(MCC_TREG_RAX, re, NULL, 0);

	o(0xf0);
	orex(size == 8, rp, rd, 0x0f);
	o(0xb1);
	gen_modrm(rd, rp, NULL, 0);

	if (REG_VALUE(sc) >= 4 || REX_BASE(sc))
		{ MCC_TRACE("br\n"); o(0x40 | REX_BASE(sc)); }
	o(0x0f);
	o(0x94);
	o(0xc0 + REG_VALUE(sc));

	t = gjmp_cond(TOK_EQ, 0);

	orex(size == 8, re, MCC_TREG_RAX, 0x89);
	gen_modrm(MCC_TREG_RAX, re, NULL, 0);

	gsym(t);

	if (REG_VALUE(sc) >= 4 || REX_BASE(sc))
		{ MCC_TRACE("br\n"); o(0x40 | REX_BASE(sc)); }
	o(0x0f);
	o(0xb6);
	o(0xc0 + REG_VALUE(sc) * 8 + REG_VALUE(sc));
	vtop->r = sc;
}

void gen_atomic_xchg(int size) { MCC_TRACE("enter\n");
	int rp, rv;
	gv2(MCC_RC_INT, MCC_RC_INT);
	rp = vtop[-1].r & VT_VALMASK;
	rv = vtop->r & VT_VALMASK;
	orex(size == 8, rp, rv, 0x87);
	gen_modrm(rv, rp, NULL, 0);
}

void gen_atomic_xadd(int size) { MCC_TRACE("enter\n");
	int rp, rv;
	gv2(MCC_RC_INT, MCC_RC_INT);
	rp = vtop[-1].r & VT_VALMASK;
	rv = vtop->r & VT_VALMASK;
	o(0xf0);
	orex(size == 8, rp, rv, 0x0f);
	o(0xc1);
	gen_modrm(rv, rp, NULL, 0);
}

void gen_ffs(int size) { MCC_TRACE("enter\n");
	int r, sc;
	gv(MCC_RC_INT);
	r = vtop->r & VT_VALMASK;
	sc = get_reg(MCC_RC_INT);
	orex(size == 8, r, r, 0x0f);
	o(0xbc);
	o(0xc0 + REG_VALUE(r) * 8 + REG_VALUE(r));
	orex(0, sc, 0, 0xb8 + REG_VALUE(sc));
	gen_le32(-1);
	orex(0, sc, r, 0x0f);
	o(0x44);
	o(0xc0 + REG_VALUE(r) * 8 + REG_VALUE(sc));
	orex(0, r, 0, 0xff);
	o(0xc0 + REG_VALUE(r));
	vtop->type.t = VT_INT;
}

void gen_bitscan(int ctz, int size) { MCC_TRACE("enter\n");
	int r, t, w = size * 8;
	gv(MCC_RC_INT);
	r = vtop->r & VT_VALMASK;
	t = get_reg(MCC_RC_INT);
	orex(0, t, 0, 0xb8 + REG_VALUE(t));
	gen_le32(ctz ? w : 2 * w - 1);
	orex(size == 8, r, r, 0x0f);
	o(ctz ? 0xbc : 0xbd);
	o(0xc0 + REG_VALUE(r) * 8 + REG_VALUE(r));
	orex(size == 8, t, r, 0x0f);
	o(0x44);
	o(0xc0 + REG_VALUE(r) * 8 + REG_VALUE(t));
	if (!ctz) { MCC_TRACE("br\n");
		orex(size == 8, r, 0, 0x83);
		o(0xf0 + REG_VALUE(r));
		o(w - 1);
	}
	vtop->type.t = VT_INT;
}

void gen_rotr_var(int size) { MCC_TRACE("enter\n");
	int r;
	gv2(MCC_RC_INT, MCC_RC_RCX);
	r = vtop[-1].r & VT_VALMASK;
	if (size == 2)
		{ MCC_TRACE("br\n"); o(0x66); }
	orex(size == 8, r, 0, 0xd3);
	o(0xc8 + REG_VALUE(r));
	vtop--;
}

void gen_rotl_var(int size) { MCC_TRACE("enter\n");
	int r;
	gv2(MCC_RC_INT, MCC_RC_RCX);
	r = vtop[-1].r & VT_VALMASK;
	if (size == 2)
		{ MCC_TRACE("br\n"); o(0x66); }
	orex(size == 8, r, 0, 0xd3);
	o(0xc0 + REG_VALUE(r));
	vtop--;
}

void gen_rotl(int size, int count) { MCC_TRACE("enter\n");
	int r;
	gv(MCC_RC_INT);
	r = vtop->r & VT_VALMASK;
	if (size == 2)
		{ MCC_TRACE("br\n"); o(0x66); }
	orex(size == 8, r, 0, 0xc1);
	o(0xc0 + REG_VALUE(r));
	o((unsigned)count & (size == 8 ? 63 : size == 2 ? 15 : 31));
}

void gen_shld(int size, int count) { MCC_TRACE("enter\n");
	int r, fr;
	gv2(MCC_RC_INT, MCC_RC_INT);
	r = vtop[-1].r & VT_VALMASK;
	fr = vtop[0].r & VT_VALMASK;
	orex(size == 8, r, fr, 0xa40f);
	o(0xc0 + REG_VALUE(r) + REG_VALUE(fr) * 8);
	o((unsigned)count & (size == 8 ? 63 : 31));
	vtop--;
}

void gen_shld_var(int size) { MCC_TRACE("enter\n");
	int r, fr;
	gv(MCC_RC_RCX);
	vswap();
	gv(MCC_RC_INT);
	vswap();
	vrotb(3);
	gv(MCC_RC_INT);
	vrott(3);
	r = vtop[-2].r & VT_VALMASK;
	fr = vtop[-1].r & VT_VALMASK;
	orex(size == 8, r, fr, 0xa50f);
	o(0xc0 + REG_VALUE(r) + REG_VALUE(fr) * 8);
	vtop -= 2;
}

void gen_shrd_var(int size) { MCC_TRACE("enter\n");
	int r, fr;
	gv(MCC_RC_RCX);
	vswap();
	gv(MCC_RC_INT);
	vswap();
	vrotb(3);
	gv(MCC_RC_INT);
	vrott(3);
	r = vtop[-2].r & VT_VALMASK;
	fr = vtop[-1].r & VT_VALMASK;
	orex(size == 8, r, fr, 0xad0f);
	o(0xc0 + REG_VALUE(r) + REG_VALUE(fr) * 8);
	vtop -= 2;
}

void gen_bswap(int size) { MCC_TRACE("enter\n");
	int r;
	gv(MCC_RC_INT);
	r = vtop->r & VT_VALMASK;
	if (size == 2) { MCC_TRACE("br\n");
		o(0x66);
		orex(0, r, 0, 0xc1);
		o(0xc0 + REG_VALUE(r));
		o(8);
		return;
	}
	orex(size == 8, r, 0, 0x0f);
	o(0xc8 + REG_VALUE(r));
}

void gen_sqrt(void) { MCC_TRACE("enter\n");
	int bt = vtop->type.t & VT_BTYPE;
	int r, d;
	gv(MCC_RC_FLOAT);
	r = vtop->r & VT_VALMASK;
	d = r;
	if (ast_pinned_regs & ((uint64_t)1 << (vtop->r & VT_VALMASK))) { MCC_TRACE("br\n");
		int nr = get_reg(MCC_RC_FLOAT);
		vtop->r = nr;
		d = nr;
	}
	o(bt == VT_DOUBLE ? 0xf2 : 0xf3);
	sse_rex(d, r);
	o(0x0f);
	o(0x51);
	o(0xc0 + REG_VALUE(r) + REG_VALUE(d) * 8);
}

void gen_round(int mode) { MCC_TRACE("enter\n");
	int bt = vtop->type.t & VT_BTYPE;
	int imm = mode == 0 ? 0x9 : mode == 1 ? 0xa : mode == 2 ? 0xb
					 : mode == 4 ? 0x4 : mode == 5 ? 0xc : 0xb;
	int r, d;
	gv(MCC_RC_FLOAT);
	r = vtop->r & VT_VALMASK;
	d = r;
	if (ast_pinned_regs & ((uint64_t)1 << (vtop->r & VT_VALMASK))) { MCC_TRACE("br\n");
		int nr = get_reg(MCC_RC_FLOAT);
		vtop->r = nr;
		d = nr;
	}
	o(0x66);
	sse_rex(d, r);
	o(0x0f);
	o(0x3a);
	o(bt == VT_DOUBLE ? 0x0b : 0x0a);
	o(0xc0 + REG_VALUE(d) * 8 + REG_VALUE(r));
	g(imm);
}

void gen_copysign(void) { MCC_TRACE("enter\n");
	int dbl = (vtop[-1].type.t & VT_BTYPE) == VT_DOUBLE;
	int x, y, mreg, m;
	gv2(MCC_RC_FLOAT, MCC_RC_FLOAT);
	int xr = vtop[-1].r & VT_VALMASK;
	int yr = vtop[0].r & VT_VALMASK;
	mreg = get_reg(MCC_RC_FLOAT);
	x = REG_VALUE(xr);
	y = REG_VALUE(yr);
	m = REG_VALUE(mreg);
	o(0x66); sse_rex(mreg, mreg); o(0x760f); o(0xc0 | (m << 3) | m);
	o(0x66); sse_rex(0, mreg);
	if (dbl) { MCC_TRACE("br\n"); o(0x730f); o(0xf0 | m); o(63); }
	else     { MCC_TRACE("br\n"); o(0x720f); o(0xf0 | m); o(31); }
	if (dbl) { MCC_TRACE("br\n"); o(0x66); }
	sse_rex(yr, mreg); o(0x540f); o(0xc0 | (y << 3) | m);
	if (dbl) { MCC_TRACE("br\n"); o(0x66); }
	sse_rex(mreg, xr); o(0x550f); o(0xc0 | (m << 3) | x);
	if (dbl) { MCC_TRACE("br\n"); o(0x66); }
	sse_rex(mreg, yr); o(0x560f); o(0xc0 | (m << 3) | y);
	vtop--;
	vtop->r = mreg;
}

void gen_opf(int op) { MCC_TRACE("enter\n");
	int a, ft, fc, swapped, r;
	int bt = vtop->type.t & VT_BTYPE;
	int float_type = bt == VT_LDOUBLE ? MCC_RC_ST0 : MCC_RC_FLOAT;

	if (op == TOK_NEG && IS_HALF_BT(bt)) { MCC_TRACE("br\n");
		int nr = gv(MCC_RC_INT);
		orex(0, nr, 0, 0x81);
		o(0xf0 + REG_VALUE(nr));
		gen_le32(0x8000);
		return;
	}
	if (op == TOK_NEG) { MCC_TRACE("br\n");
		gv(float_type);
		if (float_type == MCC_RC_ST0) { MCC_TRACE("br\n");
			o(0xe0d9);
		} else if (mcc_state && mcc_state->optimize >= 1) { MCC_TRACE("br\n");
			int dbl = bt == VT_DOUBLE;
			int rr = vtop->r & VT_VALMASK;
			int pinned = (ast_pinned_regs & ((uint64_t)1 << rr)) != 0;
			int mreg = get_reg(MCC_RC_FLOAT);
			int m = REG_VALUE(mreg);
			o(0x66); sse_rex(mreg, mreg); o(0x760f); o(0xc0 | (m << 3) | m);
			o(0x66); sse_rex(0, mreg);
			if (dbl) { MCC_TRACE("br\n"); o(0x730f); o(0xf0 | m); o(63); }
			else     { MCC_TRACE("br\n"); o(0x720f); o(0xf0 | m); o(31); }
			if (dbl) { MCC_TRACE("br\n"); o(0x66); }
			if (pinned) { MCC_TRACE("br\n");
				sse_rex(mreg, rr); o(0x570f); o(0xc0 | (m << 3) | REG_VALUE(rr));
				vtop->r = mreg;
			} else { MCC_TRACE("br\n");
				sse_rex(rr, mreg); o(0x570f); o(0xc0 | (REG_VALUE(rr) << 3) | m);
			}
		} else { MCC_TRACE("br\n");
			save_reg(vtop->r);
			o(0x80);
			gen_modrm(6, vtop->r, NULL, vtop->c.i + (bt == VT_DOUBLE ? 7 : 3));
			o(0x80);
			gv(float_type);
		}
		return;
	}

	if ((vtop[-1].r & (VT_VALMASK | VT_LVAL)) == VT_CONST) { MCC_TRACE("br\n");
		vswap();
		gv(float_type);
		vswap();
	}
	if ((vtop[0].r & (VT_VALMASK | VT_LVAL)) == VT_CONST)
		{ MCC_TRACE("br\n"); gv(float_type); }

	if (float_type == MCC_RC_FLOAT) { MCC_TRACE("br\n");
		if ((vtop[0].r & (VT_LVAL | VT_SYM)) == (VT_LVAL | VT_SYM) &&
				vtop[0].sym && (vtop[0].sym->type.t & VT_TLS))
			{ MCC_TRACE("br\n"); gv(float_type); }
		if ((vtop[-1].r & (VT_LVAL | VT_SYM)) == (VT_LVAL | VT_SYM) &&
				vtop[-1].sym && (vtop[-1].sym->type.t & VT_TLS)) { MCC_TRACE("br\n");
			vswap();
			gv(float_type);
			vswap();
		}
	}

	if ((vtop[-1].r & VT_LVAL) &&
			(vtop[0].r & VT_LVAL)) { MCC_TRACE("br\n");
		vswap();
		gv(float_type);
		vswap();
	}
	swapped = 0;
	if (vtop[-1].r & VT_LVAL) { MCC_TRACE("br\n");
		vswap();
		swapped = 1;
	}
	if ((vtop->type.t & VT_BTYPE) == VT_LDOUBLE) { MCC_TRACE("br\n");
		if (op >= TOK_ULT && op <= TOK_GT) { MCC_TRACE("br\n");
			load(MCC_TREG_ST0, vtop);
			save_reg(MCC_TREG_RAX);
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
			load(MCC_TREG_ST0, vtop);
			swapped = !swapped;

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
			o(0xde);
			o(0xc1 + (a << 3));
			vtop--;
		}
	} else { MCC_TRACE("br\n");
		if (op >= TOK_ULT && op <= TOK_GT) { MCC_TRACE("br\n");
			r = vtop->r;
			fc = vtop->c.i;
			if ((r & VT_VALMASK) == VT_LLOCAL) { MCC_TRACE("br\n");
				SValue v1;
				r = get_reg(MCC_RC_INT);
				v1.type.t = VT_PTR;
				v1.r = VT_LOCAL | VT_LVAL;
				v1.c.i = fc;
				v1.sym = NULL;
				load(r, &v1);
				fc = 0;
				vtop->r = r = r | VT_LVAL;
			}

			if (op == TOK_EQ || op == TOK_NE) { MCC_TRACE("br\n");
				swapped = 0;
			} else { MCC_TRACE("br\n");
				if (op == TOK_LE || op == TOK_LT)
					{ MCC_TRACE("br\n"); swapped = !swapped; }
				if (op == TOK_LE || op == TOK_GE) { MCC_TRACE("br\n");
					op = 0x93;
				} else { MCC_TRACE("br\n");
					op = 0x97;
				}
			}

			if (swapped) { MCC_TRACE("br\n");
				gv(MCC_RC_FLOAT);
				vswap();
			}
			assert(!(vtop[-1].r & VT_LVAL));

			if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE)
				{ MCC_TRACE("br\n"); o(0x66); }
			if (vtop->r & VT_LVAL)
				{ MCC_TRACE("br\n"); orex(0, r, vtop[-1].r, 0); }
			else
				{ MCC_TRACE("br\n"); sse_rex(vtop[-1].r, vtop[0].r); }
			if (op == TOK_EQ || op == TOK_NE)
				{ MCC_TRACE("br\n"); o(0x2e0f); }
			else
				{ MCC_TRACE("br\n"); o(0x2f0f); }

			if (vtop->r & VT_LVAL) { MCC_TRACE("br\n");
				gen_modrm(vtop[-1].r, r, vtop->sym, fc);
			} else { MCC_TRACE("br\n");
				o(0xc0 + REG_VALUE(vtop[0].r) + REG_VALUE(vtop[-1].r) * 8);
			}

			vtop--;
			vset_VT_CMP(op | 0x100);
			vtop->cmp_r = op;
		} else { MCC_TRACE("br\n");
			assert((vtop->type.t & VT_BTYPE) != VT_LDOUBLE);
			switch (op) { MCC_TRACE("br\n");
			default:
			case '+':
				a = 0;
				break;
			case '-':
				a = 4;
				break;
			case '*':
				a = 1;
				break;
			case '/':
				a = 6;
				break;
			}
			ft = vtop->type.t;
			fc = vtop->c.i;
			assert((ft & VT_BTYPE) != VT_LDOUBLE);

			r = vtop->r;
			if ((vtop->r & VT_VALMASK) == VT_LLOCAL) { MCC_TRACE("br\n");
				SValue v1;
				r = get_reg(MCC_RC_INT);
				v1.type.t = VT_PTR;
				v1.r = VT_LOCAL | VT_LVAL;
				v1.c.i = fc;
				v1.sym = NULL;
				load(r, &v1);
				fc = 0;
				vtop->r = r = r | VT_LVAL;
			}

			assert(!(vtop[-1].r & VT_LVAL));
			if (swapped) { MCC_TRACE("br\n");
				assert(vtop->r & VT_LVAL);
				gv(MCC_RC_FLOAT);
				vswap();
				fc = vtop->c.i;
				r = vtop->r;
			}

			{
				int dr = vtop[-1].r & VT_VALMASK;
				if (dr < VT_CONST && (ast_pinned_regs & ((uint64_t)1 << dr))) { MCC_TRACE("br\n");
					int sc = get_reg(MCC_RC_FLOAT);
					o((ft & VT_BTYPE) == VT_DOUBLE ? 0xf2 : 0xf3);
					sse_rex(sc, dr);
					o(0x100f);
					o(0xc0 + REG_VALUE(dr) + REG_VALUE(sc) * 8);
					vtop[-1].r = (vtop[-1].r & ~VT_VALMASK) | sc;
					fc = vtop->c.i;
					r = vtop->r;
				}
			}
			if ((ft & VT_BTYPE) == VT_DOUBLE) { MCC_TRACE("br\n");
				o(0xf2);
			} else { MCC_TRACE("br\n");
				o(0xf3);
			}
			if (vtop->r & VT_LVAL)
				{ MCC_TRACE("br\n"); orex(0, r, vtop[-1].r, 0); }
			else
				{ MCC_TRACE("br\n"); sse_rex(vtop[-1].r, vtop[0].r); }
			o(0x0f);
			o(0x58 + a);

			if (vtop->r & VT_LVAL) { MCC_TRACE("br\n");
				gen_modrm(vtop[-1].r, r, vtop->sym, fc);
			} else { MCC_TRACE("br\n");
				o(0xc0 + REG_VALUE(vtop[0].r) + REG_VALUE(vtop[-1].r) * 8);
			}

			vtop--;
		}
	}
}

void gen_cvt_itof(int t) { MCC_TRACE("enter\n");
	if ((t & VT_BTYPE) == VT_LDOUBLE) { MCC_TRACE("br\n");
		save_reg(MCC_TREG_ST0);
		gv(MCC_RC_INT);
		if ((vtop->type.t & VT_BTYPE) == VT_LLONG) { MCC_TRACE("br\n");
			o(0x50 + (vtop->r & VT_VALMASK));
			o(0x242cdf);
			o(0x08c48348);
		} else if ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
							 (VT_INT | VT_UNSIGNED)) { MCC_TRACE("br\n");
			o(0x6a);
			g(0x00);
			o(0x50 + (vtop->r & VT_VALMASK));
			o(0x242cdf);
			o(0x10c48348);
		} else { MCC_TRACE("br\n");
			o(0x50 + (vtop->r & VT_VALMASK));
			o(0x2404db);
			o(0x08c48348);
		}
		vtop->r = MCC_TREG_ST0;
	} else { MCC_TRACE("br\n");
		int r = get_reg(MCC_RC_FLOAT);
		gv(MCC_RC_INT);
		if (mcc_state && mcc_state->optimize >= 1 &&
				(vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
						(VT_LLONG | VT_UNSIGNED)) { MCC_TRACE("br\n");
			int pf = (t & VT_BTYPE) == VT_FLOAT ? 0xf3 : 0xf2;
			int s = vtop->r & VT_VALMASK;
			int t2, jhi, jend;
			uint64_t save_pin = ast_pinned_regs;
			ast_pinned_regs |= (uint64_t)1 << s;
			t2 = get_reg(MCC_RC_INT);
			ast_pinned_regs = save_pin;
			orex(1, s, s, 0x85);
			o(0xc0 | (REG_VALUE(s) << 3) | REG_VALUE(s));
			o(0x0f);
			jhi = gjmp2(0x88, 0);
			o(0x66); sse_rex(r, r); o(0xef0f);
			o(0xc0 | (REG_VALUE(r) << 3) | REG_VALUE(r));
			o(pf); o(0x48 | (REX_BASE(r) << 2) | REX_BASE(s)); o(0x2a0f);
			o(0xc0 | REG_VALUE(s) | (REG_VALUE(r) << 3));
			jend = gjmp2(0xe9, 0);
			gsym(jhi);
			orex(1, s, t2, 0x89);
			o(0xc0 | (REG_VALUE(s) << 3) | REG_VALUE(t2));
			orex(1, 0, t2, 0xd1); o(0xe8 | REG_VALUE(t2));
			orex(1, 0, s, 0x83); o(0xe0 | REG_VALUE(s)); g(1);
			orex(1, s, t2, 0x09);
			o(0xc0 | (REG_VALUE(s) << 3) | REG_VALUE(t2));
			o(0x66); sse_rex(r, r); o(0xef0f);
			o(0xc0 | (REG_VALUE(r) << 3) | REG_VALUE(r));
			o(pf); o(0x48 | (REX_BASE(r) << 2) | REX_BASE(t2)); o(0x2a0f);
			o(0xc0 | REG_VALUE(t2) | (REG_VALUE(r) << 3));
			o(pf);
			if (REX_BASE(r))
				{ MCC_TRACE("br\n"); o(0x40 | (REX_BASE(r) << 2) | REX_BASE(r)); }
			o(0x580f); o(0xc0 | (REG_VALUE(r) << 3) | REG_VALUE(r));
			gsym(jend);
			vtop->r = r;
			return;
		}
		int w = ((vtop->type.t & (VT_BTYPE | VT_UNSIGNED)) ==
										(VT_INT | VT_UNSIGNED) ||
								(vtop->type.t & VT_BTYPE) == VT_LLONG)
						? 8
						: 0;
		if (mcc_state && mcc_state->optimize >= 1) { MCC_TRACE("br\n");
			o(0x66);
			sse_rex(r, r);
			o(0xef0f);
			o(0xc0 | (REG_VALUE(r) << 3) | REG_VALUE(r));
		}
		o(0xf2 + ((t & VT_BTYPE) == VT_FLOAT ? 1 : 0));
		if (w || REX_BASE(r) || REX_BASE(vtop->r & VT_VALMASK)) { MCC_TRACE("br\n");
			o(0x40 | w | (REX_BASE(r) << 2) | REX_BASE(vtop->r & VT_VALMASK));
		}
		o(0x2a0f);
		o(0xc0 + (vtop->r & VT_VALMASK) + REG_VALUE(r) * 8);
		vtop->r = r;
	}
}

void gen_cvt_ftof(int t) { MCC_TRACE("enter\n");
	int ft, bt, tbt;

	ft = vtop->type.t;
	bt = ft & VT_BTYPE;
	tbt = t & VT_BTYPE;

	if (bt == VT_FLOAT) { MCC_TRACE("br\n");
		gv(MCC_RC_FLOAT);
		if (tbt == VT_DOUBLE) { MCC_TRACE("br\n");
			int v;
			sse_unpin_src();
			v = vtop->r & VT_VALMASK;
			sse_rex(v, v);
			o(0x140f);
			o(0xc0 + REG_VALUE(v) * 9);
			sse_rex(v, v);
			o(0x5a0f);
			o(0xc0 + REG_VALUE(v) * 9);
		} else if (tbt == VT_LDOUBLE) { MCC_TRACE("br\n");
			save_reg(MCC_RC_ST0);
			o(0xf3);
			sse_rex(vtop->r & VT_VALMASK, MCC_TREG_RSP);
			o(0x110f);
			o(0x44 + REG_VALUE(vtop->r) * 8);
			o(0xf024);
			o(0xf02444d9);
			vtop->r = MCC_TREG_ST0;
		}
	} else if (bt == VT_DOUBLE) { MCC_TRACE("br\n");
		gv(MCC_RC_FLOAT);
		if (tbt == VT_FLOAT) { MCC_TRACE("br\n");
			int v;
			sse_unpin_src();
			v = vtop->r & VT_VALMASK;
			o(0x66); sse_rex(v, v);
			o(0x140f);
			o(0xc0 + REG_VALUE(v) * 9);
			o(0x66); sse_rex(v, v);
			o(0x5a0f);
			o(0xc0 + REG_VALUE(v) * 9);
		} else if (tbt == VT_LDOUBLE) { MCC_TRACE("br\n");
			save_reg(MCC_RC_ST0);
			o(0xf2);
			sse_rex(vtop->r & VT_VALMASK, MCC_TREG_RSP);
			o(0x110f);
			o(0x44 + REG_VALUE(vtop->r) * 8);
			o(0xf024);
			o(0xf02444dd);
			vtop->r = MCC_TREG_ST0;
		}
	} else { MCC_TRACE("br\n");
		int r;
		gv(MCC_RC_ST0);
		r = get_reg(MCC_RC_FLOAT);
		if (tbt == VT_DOUBLE) { MCC_TRACE("br\n");
			o(0xf0245cdd);
			o(0xf2);
			sse_rex(r, MCC_TREG_RSP);
			o(0x100f);
			o(0x44 + REG_VALUE(r) * 8);
			o(0xf024);
			vtop->r = r;
		} else if (tbt == VT_FLOAT) { MCC_TRACE("br\n");
			o(0xf0245cd9);
			o(0xf3);
			sse_rex(r, MCC_TREG_RSP);
			o(0x100f);
			o(0x44 + REG_VALUE(r) * 8);
			o(0xf024);
			vtop->r = r;
		}
	}
}

void gen_cvt_ftoi(int t) { MCC_TRACE("enter\n");
	int ft, bt, size, r;
	ft = vtop->type.t;
	bt = ft & VT_BTYPE;
	if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
		if (t != VT_INT) { MCC_TRACE("br\n");
			vpush_helper_func(TOK___fixxfdi);
			vswap();
			gfunc_call(1);
			vpushi(0);
			vtop->r = REG_IRET;
			vtop->r2 = REG_IRE2;
			return;
		}
		{ MCC_TRACE("br\n");
			int slot;
			gv(MCC_RC_ST0);
			slot = (loc = ast_alloc_loc(8, 8));
			o(0xd9);
			gen_modrm(7, VT_LOCAL, NULL, slot + 4);
			o(0xd9);
			gen_modrm(7, VT_LOCAL, NULL, slot + 6);
			o(0x66);
			o(0x81);
			gen_modrm(1, VT_LOCAL, NULL, slot + 6);
			gen_le16(0x0c00);
			o(0xd9);
			gen_modrm(5, VT_LOCAL, NULL, slot + 6);
			o(0xdb);
			gen_modrm(3, VT_LOCAL, NULL, slot);
			o(0xd9);
			gen_modrm(5, VT_LOCAL, NULL, slot + 4);
			r = get_reg(MCC_RC_INT);
			orex(0, VT_LOCAL, r, 0x8b);
			gen_modrm(r, VT_LOCAL, NULL, slot);
			vtop->r = r;
			return;
		}
	}

	gv(MCC_RC_FLOAT);
	if (t != VT_INT)
		{ MCC_TRACE("br\n"); size = 8; }
	else
		{ MCC_TRACE("br\n"); size = 4; }

	r = get_reg(MCC_RC_INT);
	if (bt == VT_FLOAT) { MCC_TRACE("br\n");
		o(0xf3);
	} else if (bt == VT_DOUBLE) { MCC_TRACE("br\n");
		o(0xf2);
	} else { MCC_TRACE("br\n");
		assert(0);
	}
	orex(size == 8, vtop->r & VT_VALMASK, r, 0x2c0f);
	o(0xc0 + REG_VALUE(vtop->r) + REG_VALUE(r) * 8);
	vtop->r = r;
}

ST_FUNC void gen_cvt_sxtw(void) { MCC_TRACE("enter\n");
	int r = gv(MCC_RC_INT);
	o(0x6348);
	o(0xc0 + (REG_VALUE(r) << 3) + REG_VALUE(r));
}

ST_FUNC void gen_cvt_trunc32(void) { MCC_TRACE("enter\n");
	int r = gv(MCC_RC_INT);
	orex(0, r, r, 0x89);
	o(0xc0 + (REG_VALUE(r) << 3) + REG_VALUE(r));
}

ST_FUNC void gen_cvt_csti(int t) { MCC_TRACE("enter\n");
	int r, sz, xl, ll;
	r = gv(MCC_RC_INT);
	sz = !(t & VT_UNSIGNED);
	xl = (t & VT_BTYPE) == VT_SHORT;
	ll = (vtop->type.t & VT_BTYPE) == VT_LLONG;
	orex(ll, r, 0, 0xc0b60f | (sz << 3 | xl) << 8 | (REG_VALUE(r) << 3 | REG_VALUE(r)) << 16);
}

ST_FUNC void gen_increment_tcov(SValue *sv) { MCC_TRACE("enter\n");
	o(0x058348);
	greloca(cur_text_section, sv->sym, ind, R_X86_64_PC32, -5);
	gen_le32(0);
	o(1);
}

ST_FUNC void ggoto(void) { MCC_TRACE("enter\n");
	gcall_or_jmp(1);
	vtop--;
}

ST_FUNC void gen_x87_pop(void) { MCC_TRACE("enter\n");
	o(0xd8dd);
}

ST_FUNC void gen_vla_sp_save(int addr) { MCC_TRACE("enter\n");
	gen_modrm64(0x89, MCC_TREG_RSP, VT_LOCAL, NULL, addr);
}

ST_FUNC void gen_vla_sp_restore(int addr) { MCC_TRACE("enter\n");
	gen_modrm64(0x8b, MCC_TREG_RSP, VT_LOCAL, NULL, addr);
}

#ifdef MCC_TARGET_PE
ST_FUNC void gen_vla_result(int addr) { MCC_TRACE("enter\n");
	gen_modrm64(0x89, MCC_TREG_RAX, VT_LOCAL, NULL, addr);
}
#endif

ST_FUNC void gen_vla_alloc(CType *type, int align) { MCC_TRACE("enter\n");
	int use_call = 0;

	use_call = mcc_state->do_bounds_check;
#ifdef MCC_TARGET_PE
	use_call = 1;
#endif
	if (use_call) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_PE
		if (align > 16) { MCC_TRACE("br\n");
			vpushi(align);
			gen_op('+');
		}
#endif
		vpush_helper_func(TOK_alloca);
		vswap();
		gfunc_call(1);
#ifdef MCC_TARGET_PE
		if (align > 16) { MCC_TRACE("br\n");
			o(0x0548);
			gen_le32(align - 1);
			o(0x2548);
			gen_le32(-align);
		}
#endif
	} else { MCC_TRACE("br\n");
		int r;
		int a = align < 16 ? 16 : align;
		r = gv(MCC_RC_INT);
		o(0x2b48);
		o(0xe0 | REG_VALUE(r));
		if (a > 16) { MCC_TRACE("br\n");
			o(0xe48148);
			gen_le32(-a);
		} else { MCC_TRACE("br\n");
			o(0xf0e48348);
		}
		vpop();
	}
}

ST_FUNC void gen_struct_copy(int size) { MCC_TRACE("enter\n");
	int n = size / MCC_PTR_SIZE;
#ifdef MCC_TARGET_PE
	o(0x5756);
#endif
	gv2(MCC_RC_RDI, MCC_RC_RSI);
	if (n <= 4) { MCC_TRACE("br\n");
		while (n)
			{ MCC_TRACE("br\n"); o(0xa548), --n; }
	} else { MCC_TRACE("br\n");
		vpushi(n);
		gv(MCC_RC_RCX);
		o(0xa548f3);
		vpop();
	}
	if (size & 0x04)
		{ MCC_TRACE("br\n"); o(0xa5); }
	if (size & 0x02)
		{ MCC_TRACE("br\n"); o(0xa566); }
	if (size & 0x01)
		{ MCC_TRACE("br\n"); o(0xa4); }
#ifdef MCC_TARGET_PE
	o(0x5e5f);
#endif
	vpop();
	vpop();
}
