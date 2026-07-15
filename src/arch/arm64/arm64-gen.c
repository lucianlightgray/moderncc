#define USING_GLOBALS
#include "mcc.h"
#include <assert.h>

ST_DATA const char *const target_machine_defs =
		"__aarch64__\0"
#if defined(MCC_TARGET_MACHO)
		"__arm64__\0"
#endif
		"__AARCH64EL__\0";

ST_DATA const int reg_classes[MCC_NB_REGS] = {
		MCC_RC_INT | MCC_RC_R(0),
		MCC_RC_INT | MCC_RC_R(1),
		MCC_RC_INT | MCC_RC_R(2),
		MCC_RC_INT | MCC_RC_R(3),
		MCC_RC_INT | MCC_RC_R(4),
		MCC_RC_INT | MCC_RC_R(5),
		MCC_RC_INT | MCC_RC_R(6),
		MCC_RC_INT | MCC_RC_R(7),
		MCC_RC_INT | MCC_RC_R(8),
		MCC_RC_INT | MCC_RC_R(9),
		MCC_RC_INT | MCC_RC_R(10),
		MCC_RC_INT | MCC_RC_R(11),
		MCC_RC_INT | MCC_RC_R(12),
		MCC_RC_INT | MCC_RC_R(13),
		MCC_RC_INT | MCC_RC_R(14),
		MCC_RC_INT | MCC_RC_R(15),
		MCC_RC_INT | MCC_RC_R(16),
		MCC_RC_INT | MCC_RC_R(17),
#ifdef MCC_TARGET_PE
		MCC_RC_R(18),
#else
		MCC_RC_INT | MCC_RC_R(18),
#endif
		MCC_RC_R30,
		MCC_RC_FLOAT | MCC_RC_F(0),
		MCC_RC_FLOAT | MCC_RC_F(1),
		MCC_RC_FLOAT | MCC_RC_F(2),
		MCC_RC_FLOAT | MCC_RC_F(3),
		MCC_RC_FLOAT | MCC_RC_F(4),
		MCC_RC_FLOAT | MCC_RC_F(5),
		MCC_RC_FLOAT | MCC_RC_F(6),
		MCC_RC_FLOAT | MCC_RC_F(7),
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0,
		0};

#if MCC_CONFIG_DIAG_RT >= 2
#define func_bound_offset (mcc_state->cg_func_bound_offset)
#define func_bound_ind (mcc_state->cg_func_bound_ind)
ST_DATA int func_bound_add_epilog;
#endif

#ifdef MCC_TARGET_MACHO
#define func_stack_chk_loc (mcc_state->cg_func_stack_chk_loc)
#endif

#define IS_FREG(x) ((x) >= MCC_TREG_F(0) && (x) <= MCC_TREG_F(7))

static uint32_t intr(int r) { MCC_TRACE("enter\n");
	if (r >= MCC_TREG_SAVED(0) && r <= MCC_TREG_SAVED(MCC_NB_SAVED - 1))
		{ MCC_TRACE("br\n"); return (uint32_t)(r - MCC_TREG_SAVED(0) + 19); }
	assert(MCC_TREG_R(0) <= r && r <= MCC_TREG_R30);
	return r < MCC_TREG_R30 ? r : 30;
}

static uint32_t fltr(int r) { MCC_TRACE("enter\n");
	assert(MCC_TREG_F(0) <= r && r <= MCC_TREG_F(7));
	return r - MCC_TREG_F(0);
}

ST_FUNC void o(unsigned int c) { MCC_TRACE("enter\n");
	int ind1 = ind + 4;
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	if (ind1 > cur_text_section->data_allocated)
		{ MCC_TRACE("br\n"); section_realloc(cur_text_section, ind1); }
	write32le(cur_text_section->data + ind, c);
	ind = ind1;
}

static int arm64_encode_bimm64(uint64_t x) { MCC_TRACE("enter\n");
	int neg = x & 1;
	int rep, pos, len;

	if (neg)
		{ MCC_TRACE("br\n"); x = ~x; }
	if (!x)
		{ MCC_TRACE("br\n"); return -1; }

	if (x >> 2 == (x & (((uint64_t)1 << (64 - 2)) - 1)))
		{ MCC_TRACE("br\n"); rep = 2, x &= ((uint64_t)1 << 2) - 1; }
	else if (x >> 4 == (x & (((uint64_t)1 << (64 - 4)) - 1)))
		{ MCC_TRACE("br\n"); rep = 4, x &= ((uint64_t)1 << 4) - 1; }
	else if (x >> 8 == (x & (((uint64_t)1 << (64 - 8)) - 1)))
		{ MCC_TRACE("br\n"); rep = 8, x &= ((uint64_t)1 << 8) - 1; }
	else if (x >> 16 == (x & (((uint64_t)1 << (64 - 16)) - 1)))
		{ MCC_TRACE("br\n"); rep = 16, x &= ((uint64_t)1 << 16) - 1; }
	else if (x >> 32 == (x & (((uint64_t)1 << (64 - 32)) - 1)))
		{ MCC_TRACE("br\n"); rep = 32, x &= ((uint64_t)1 << 32) - 1; }
	else
		{ MCC_TRACE("br\n"); rep = 64; }

	pos = 0;
	if (!(x & (((uint64_t)1 << 32) - 1)))
		{ MCC_TRACE("br\n"); x >>= 32, pos += 32; }
	if (!(x & (((uint64_t)1 << 16) - 1)))
		{ MCC_TRACE("br\n"); x >>= 16, pos += 16; }
	if (!(x & (((uint64_t)1 << 8) - 1)))
		{ MCC_TRACE("br\n"); x >>= 8, pos += 8; }
	if (!(x & (((uint64_t)1 << 4) - 1)))
		{ MCC_TRACE("br\n"); x >>= 4, pos += 4; }
	if (!(x & (((uint64_t)1 << 2) - 1)))
		{ MCC_TRACE("br\n"); x >>= 2, pos += 2; }
	if (!(x & (((uint64_t)1 << 1) - 1)))
		{ MCC_TRACE("br\n"); x >>= 1, pos += 1; }

	len = 0;
	if (!(~x & (((uint64_t)1 << 32) - 1)))
		{ MCC_TRACE("br\n"); x >>= 32, len += 32; }
	if (!(~x & (((uint64_t)1 << 16) - 1)))
		{ MCC_TRACE("br\n"); x >>= 16, len += 16; }
	if (!(~x & (((uint64_t)1 << 8) - 1)))
		{ MCC_TRACE("br\n"); x >>= 8, len += 8; }
	if (!(~x & (((uint64_t)1 << 4) - 1)))
		{ MCC_TRACE("br\n"); x >>= 4, len += 4; }
	if (!(~x & (((uint64_t)1 << 2) - 1)))
		{ MCC_TRACE("br\n"); x >>= 2, len += 2; }
	if (!(~x & (((uint64_t)1 << 1) - 1)))
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

static uint32_t arm64_movi(int r, uint64_t x) { MCC_TRACE("enter\n");
	uint64_t m = 0xffff;
	int e;
	if (!(x & ~m))
		{ MCC_TRACE("br\n"); return ARM64_MOVZ | r | x << 5; }
	if (!(x & ~(m << 16)))
		{ MCC_TRACE("br\n"); return (ARM64_MOVZ | ARM64_HW(1) | r | x >> 11); }
	if (!(x & ~(m << 32)))
		{ MCC_TRACE("br\n"); return (ARM64_MOVZ64 | ARM64_HW(2) | r | x >> 27); }
	if (!(x & ~(m << 48)))
		{ MCC_TRACE("br\n"); return (ARM64_MOVZ64 | ARM64_HW(3) | r | x >> 43); }
	if ((x & ~m) == m << 16)
		{ MCC_TRACE("br\n"); return (ARM64_MOVN | r |
						(~x << 5 & 0x1fffe0)); }
	if ((x & ~(m << 16)) == m)
		{ MCC_TRACE("br\n"); return (ARM64_MOVN | ARM64_HW(1) | r |
						(~x >> 11 & 0x1fffe0)); }
	if (!~(x | m))
		{ MCC_TRACE("br\n"); return (ARM64_MOVN64 | r |
						(~x << 5 & 0x1fffe0)); }
	if (!~(x | m << 16))
		{ MCC_TRACE("br\n"); return (ARM64_MOVN64 | ARM64_HW(1) | r |
						(~x >> 11 & 0x1fffe0)); }
	if (!~(x | m << 32))
		{ MCC_TRACE("br\n"); return (ARM64_MOVN64 | ARM64_HW(2) | r |
						(~x >> 27 & 0x1fffe0)); }
	if (!~(x | m << 48))
		{ MCC_TRACE("br\n"); return (ARM64_MOVN64 | ARM64_HW(3) | r |
						(~x >> 43 & 0x1fffe0)); }
	if (!(x >> 32) && (e = arm64_encode_bimm64(x | x << 32)) >= 0)
		{ MCC_TRACE("br\n"); return (ARM64_ORR_IMM | r | (uint32_t)e << 10); }
	if ((e = arm64_encode_bimm64(x)) >= 0)
		{ MCC_TRACE("br\n"); return (ARM64_ORR_IMM | ARM64_SF(1) | r | (uint32_t)e << 10); }
	return 0;
}

static void arm64_movimm(int r, uint64_t x) { MCC_TRACE("enter\n");
	uint32_t i;
	if ((i = arm64_movi(r, x)))
		{ MCC_TRACE("br\n"); o(i); }
	else { MCC_TRACE("br\n");
		int z = 0, m = 0;
		uint32_t mov1 = ARM64_MOVZ64;
		uint64_t x1 = x;
		for (i = 0; i < 64; i += 16) { MCC_TRACE("br\n");
			z += !(x >> i & 0xffff);
			m += !(~x >> i & 0xffff);
		}
		if (m > z) { MCC_TRACE("br\n");
			x1 = ~x;
			mov1 = ARM64_MOVN64;
		}
		for (i = 0; i < 64; i += 16)
			if (x1 >> i & 0xffff) { MCC_TRACE("br\n");
				o(mov1 | r | (x1 >> i & 0xffff) << 5 | i << 17);
				break;
			}
		for (i += 16; i < 64; i += 16)
			if (x1 >> i & 0xffff)
				{ MCC_TRACE("br\n"); o(ARM64_MOVK | ARM64_SF(1) | r | (x >> i & 0xffff) << 5 | i << 17); }
	}
}

ST_FUNC void gsym_addr(int t_, int a_) { MCC_TRACE("enter\n");
	uint32_t t = t_;
	uint32_t a = a_;
	while (t) { MCC_TRACE("br\n");
		unsigned char *ptr = cur_text_section->data + t;
		uint32_t next = read32le(ptr);
		if (a - t + 0x8000000 >= 0x10000000)
			{ MCC_TRACE("br\n"); mcc_error("branch out of range"); }
		write32le(ptr, (a - t == 4 ? ARM64_NOP : ARM64_B | ((a - t) >> 2 & 0x3ffffff)));
		t = next;
	}
}

static int arm64_type_size(int t) { MCC_TRACE("enter\n");
	switch (t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_BYTE:
		return 0;
	case VT_SHORT:
		return 1;
	case VT_INT:
		return 2;
	case VT_LLONG:
		return 3;
	case VT_PTR:
		return 3;
	case VT_FUNC:
		return 3;
	case VT_STRUCT:
		return 3;
	case VT_FLOAT:
		return 2;
	case VT_DOUBLE:
		return 3;
	case VT_LDOUBLE:
		return 4;
	case VT_BOOL:
		return 0;
	}
	assert(0);
	return 0;
}

static void arm64_spoff(int reg, uint64_t off) { MCC_TRACE("enter\n");
	uint32_t sub = off >> 63;
	if (sub)
		{ MCC_TRACE("br\n"); off = -off; }
	if (off < 4096)
		{ MCC_TRACE("br\n"); o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RN(31) | ARM64_RD(reg) | ARM64_IMM12(off)); }
	else { MCC_TRACE("br\n");
		arm64_movimm(30, off);
		o(ARM64_ADD_REG | ARM64_SF(1) | ARM64_RM(30) | ARM64_RN(31) | ARM64_RD(reg) | (sub << 30));
	}
}

static uint64_t arm64_check_offset(int invert, int sz_, uint64_t off) { MCC_TRACE("enter\n");
	uint32_t sz = sz_;
	uint64_t scaled_mask = 0xffful << sz;

	if (!(off & ~scaled_mask) ||
			(off < 256 || -off <= 256))
		{ MCC_TRACE("br\n"); return invert ? off : 0ul; }
	else if (off & scaled_mask)
		{ MCC_TRACE("br\n"); return invert ? off & scaled_mask : off & ~scaled_mask; }
	else if (off & 0x1fful)
		{ MCC_TRACE("br\n"); return invert ? off & 0x1fful : off & ~(uint64_t)0x1ff; }
	else
		{ MCC_TRACE("br\n"); return invert ? 0ul : off; }
}

static void arm64_ldrx(int sg, int sz_, int dst, int bas, uint64_t off) { MCC_TRACE("enter\n");
	uint32_t sz = sz_;
	uint64_t scaled_mask = 0xffful << sz;
	if (sz >= 2)
		{ MCC_TRACE("br\n"); sg = 0; }
	if (!(off & ~scaled_mask))
		{ MCC_TRACE("br\n"); o(ARM64_LDR_B | dst | bas << 5 | off << (10 - sz) |
			(uint32_t)!!sg << 23 | sz << 30); }
	else if (off < 256 || -off <= 256)
		{ MCC_TRACE("br\n"); o(ARM64_LDUR_B | dst | bas << 5 | (off & 511) << 12 |
			(uint32_t)!!sg << 23 | sz << 30); }
	else { MCC_TRACE("br\n");
		arm64_movimm(30, off);
		o(ARM64_LDR_B_REG | dst | bas << 5 | (uint32_t)30 << 16 |
			(uint32_t)(!!sg + 1) << 22 | sz << 30);
	}
}

static void arm64_ldrv(int sz_, int dst, int bas, uint64_t off) { MCC_TRACE("enter\n");
	uint32_t sz = sz_;
	uint64_t scaled_mask = 0xffful << sz;

	if (!(off & ~scaled_mask))
		{ MCC_TRACE("br\n"); o(ARM64_LDR_SCALAR | dst | bas << 5 | off << (10 - sz) |
			(sz & 4) << 21 | (sz & 3) << 30); }
	else if (off < 256 || -off <= 256)
		{ MCC_TRACE("br\n"); o(ARM64_LDUR_Q_SIMD | dst | bas << 5 | (off & 511) << 12 |
			(sz & 4) << 21 | (sz & 3) << 30); }
	else { MCC_TRACE("br\n");
		arm64_movimm(30, off);
		o(ARM64_LDR_Q_REG | dst | bas << 5 | (uint32_t)30 << 16 |
			sz << 30 | (sz & 4) << 21);
	}
}

static void arm64_ldrs(int reg_, int size) { MCC_TRACE("enter\n");
	uint32_t reg = reg_;
	switch (size) { MCC_TRACE("br\n");
	default:
		assert(0);
		break;
	case 0:
		break;
	case 1:
		arm64_ldrx(0, 0, reg, reg, 0);
		break;
	case 2:
		arm64_ldrx(0, 1, reg, reg, 0);
		break;
	case 3:
		arm64_ldrx(0, 1, 30, reg, 0);
		arm64_ldrx(0, 0, reg, reg, 2);
		o(0x2a0043c0 | reg | reg << 16);
		break;
	case 4:
		arm64_ldrx(0, 2, reg, reg, 0);
		break;
	case 5:
		arm64_ldrx(0, 2, 30, reg, 0);
		arm64_ldrx(0, 0, reg, reg, 4);
		o(0xaa0083c0 | reg | reg << 16);
		break;
	case 6:
		arm64_ldrx(0, 2, 30, reg, 0);
		arm64_ldrx(0, 1, reg, reg, 4);
		o(0xaa0083c0 | reg | reg << 16);
		break;
	case 7:
		arm64_ldrx(0, 2, 30, reg, 0);
		arm64_ldrx(0, 2, reg, reg, 3);
		o(0x53087c00 | reg | reg << 5);
		o(0xaa0083c0 | reg | reg << 16);
		break;
	case 8:
		arm64_ldrx(0, 3, reg, reg, 0);
		break;
	case 9:
		arm64_ldrx(0, 0, reg + 1, reg, 8);
		arm64_ldrx(0, 3, reg, reg, 0);
		break;
	case 10:
		arm64_ldrx(0, 1, reg + 1, reg, 8);
		arm64_ldrx(0, 3, reg, reg, 0);
		break;
	case 11:
		arm64_ldrx(0, 2, reg + 1, reg, 7);
		o(0x53087c00 | (reg + 1) | (reg + 1) << 5);
		arm64_ldrx(0, 3, reg, reg, 0);
		break;
	case 12:
		arm64_ldrx(0, 2, reg + 1, reg, 8);
		arm64_ldrx(0, 3, reg, reg, 0);
		break;
	case 13:
		arm64_ldrx(0, 3, reg + 1, reg, 5);
		o(0xd358fc00 | (reg + 1) | (reg + 1) << 5);
		arm64_ldrx(0, 3, reg, reg, 0);
		break;
	case 14:
		arm64_ldrx(0, 3, reg + 1, reg, 6);
		o(0xd350fc00 | (reg + 1) | (reg + 1) << 5);
		arm64_ldrx(0, 3, reg, reg, 0);
		break;
	case 15:
		arm64_ldrx(0, 3, reg + 1, reg, 7);
		o(0xd348fc00 | (reg + 1) | (reg + 1) << 5);
		arm64_ldrx(0, 3, reg, reg, 0);
		break;
	case 16:
		o(0xa9400000 | reg | (reg + 1) << 10 | reg << 5);
		break;
	}
}

static void arm64_strx(int sz_, int dst, int bas, uint64_t off) { MCC_TRACE("enter\n");
	uint32_t sz = sz_;
	uint64_t scaled_mask = 0xffful << sz;

	if (!(off & ~scaled_mask))
		{ MCC_TRACE("br\n"); o(0x39000000 | dst | bas << 5 | off << (10 - sz) | sz << 30); }
	else if (off < 256 || -off <= 256)
		{ MCC_TRACE("br\n"); o(0x38000000 | dst | bas << 5 | (off & 511) << 12 | sz << 30); }
	else { MCC_TRACE("br\n");
		arm64_movimm(30, off);
		o(0x38206800 | dst | bas << 5 | (uint32_t)30 << 16 | sz << 30);
	}
}

static void arm64_strv(int sz_, int dst, int bas, uint64_t off) { MCC_TRACE("enter\n");
	uint32_t sz = sz_;
	uint64_t scaled_mask = 0xffful << sz;

	if (!(off & ~scaled_mask))
		{ MCC_TRACE("br\n"); o(0x3d000000 | dst | bas << 5 | off << (10 - sz) |
			(sz & 4) << 21 | (sz & 3) << 30); }
	else if (off < 256 || -off <= 256)
		{ MCC_TRACE("br\n"); o(0x3c000000 | dst | bas << 5 | (off & 511) << 12 |
			(sz & 4) << 21 | (sz & 3) << 30); }
	else { MCC_TRACE("br\n");
		arm64_movimm(30, off);
		o(0x3c206800 | dst | bas << 5 | (uint32_t)30 << 16 |
			sz << 30 | (sz & 4) << 21);
	}
}

static void arm64_sym(int r, Sym *sym, unsigned long addend) { MCC_TRACE("enter\n");
#ifdef MCC_TARGET_PE
	greloca(cur_text_section, sym, ind, R_AARCH64_ADR_PREL_PG_HI21, 0);
	o(ARM64_ADRP | r);
	greloca(cur_text_section, sym, ind, R_AARCH64_ADD_ABS_LO12_NC, 0);
	o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RN(r) | r);
#else
	greloca(cur_text_section, sym, ind, R_AARCH64_ADR_GOT_PAGE, 0);
	o(ARM64_ADRP | r);
	greloca(cur_text_section, sym, ind, R_AARCH64_LD64_GOT_LO12_NC, 0);
	o(ARM64_LDR_X | ARM64_RN(r) | r);
#endif
	if (addend) { MCC_TRACE("br\n");
		if (addend & 0xffful)
			{ MCC_TRACE("br\n"); o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RN(r) | r |
				(addend & 0xfff) << 10); }
		if (addend > 0xffful) { MCC_TRACE("br\n");
			if (addend & 0xfff000ul)
				{ MCC_TRACE("br\n"); o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
					ARM64_RN(r) | r | ((addend >> 12) & 0xfff) << 10); }
			if (addend > 0xfffffful) { MCC_TRACE("br\n");
				int t = r ? 0 : 1;
				o(ARM64_STR_X_PRE | 0x001F0FE0U | t);
				arm64_movimm(t, addend & ~0xfffffful);
				o(ARM64_ADD_REG | ARM64_SF(1) | ARM64_RM(t) | ARM64_RN(r) | r);
				o(ARM64_LDR_X_POST | 0x000107E0U | t);
			}
		}
	}
}

static void arm64_load_cmp(int r, SValue *sv);

#ifdef MCC_TARGET_MACHO
static void arm64_macho_tls_addr(Sym *sym, uint64_t addend) { MCC_TRACE("enter\n");
	o(ARM64_SUB_IMM | ARM64_SF(1) | ARM64_RN(31) | ARM64_RD(31) | ARM64_IMM12(32));
	arm64_strx(3, 0, 31, 0);
	arm64_strx(3, 16, 31, 8);
	arm64_strx(3, 17, 31, 16);
	greloca(cur_text_section, sym, ind, R_AARCH64_ADR_PREL_PG_HI21, 0);
	o(ARM64_ADRP | ARM64_RD(0));
	greloca(cur_text_section, sym, ind, R_AARCH64_ADD_ABS_LO12_NC, 0);
	o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RN(0) | ARM64_RD(0));
	o(ARM64_LDR_X | ARM64_RN(0) | ARM64_RT(16));
	o(ARM64_BLR | ARM64_RN(16));
	if (addend & 0xfff)
		{ MCC_TRACE("br\n"); o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RN(0) | ARM64_RD(0) |
			ARM64_IMM12(addend & 0xfff)); }
	if (addend > 0xfff)
		{ MCC_TRACE("br\n"); o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) | ARM64_RN(0) | ARM64_RD(0) |
			ARM64_IMM12((addend >> 12) & 0xfff)); }
	o(ARM64_MOV_REG | ARM64_SF(1) | ARM64_RM(0) | ARM64_RD(30));
	arm64_ldrx(0, 3, 0, 31, 0);
	arm64_ldrx(0, 3, 16, 31, 8);
	arm64_ldrx(0, 3, 17, 31, 16);
	o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RN(31) | ARM64_RD(31) | ARM64_IMM12(32));
}
#endif

#ifndef MCC_TARGET_MACHO

static void arm64_tls_base_x30(void) { MCC_TRACE("enter\n");
#ifdef MCC_TARGET_PE
	o(ARM64_STR_X_PRE | 0x001F0FE0U | 16);
	arm64_ldrx(0, 3, 30, 18, 0x58);
	arm64_sym(16, pe_tls_index_sym(), 0);
	arm64_ldrx(0, 2, 16, 16, 0);
	o(ARM64_ADD_REG | ARM64_SF(1) | ARM64_RM(16) |
		ARM64_SHIFT_LSL(3) | ARM64_RN(30) | ARM64_RD(30));
	arm64_ldrx(0, 3, 30, 30, 0);
	o(ARM64_LDR_X_POST | 0x000107E0U | 16);
#else
	o(0xd53bd05e);
#endif
}
#endif

ST_FUNC void load(int r, SValue *sv) { MCC_TRACE("enter\n");
	int svtt = sv->type.t;
	int svr = sv->r & ~(VT_BOUNDED | VT_NONCONST | VT_NONLVAL | VT_MUSTCAST);
	int svrv = svr & VT_VALMASK;
	uint64_t svcul = sv->c.i;
	uint64_t svcoff = (uint64_t)(int64_t)(int32_t)sv->c.i;

	if (svr == (VT_LOCAL | VT_LVAL)) { MCC_TRACE("br\n");
		if (IS_FREG(r))
			{ MCC_TRACE("br\n"); arm64_ldrv(arm64_type_size(svtt), fltr(r), 29, svcoff); }
		else
			{ MCC_TRACE("br\n"); arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
								 intr(r), 29, svcoff); }
		return;
	}

	if (svr == (VT_CONST | VT_LVAL)) { MCC_TRACE("br\n");
		uint64_t i = sv->c.i;

		if (sv->sym)
			{ MCC_TRACE("br\n"); arm64_sym(30, sv->sym,
								arm64_check_offset(0, arm64_type_size(svtt), i)); }
		else
			{ MCC_TRACE("br\n"); arm64_movimm(30, i), i = 0; }
		if (IS_FREG(r))
			{ MCC_TRACE("br\n"); arm64_ldrv(arm64_type_size(svtt), fltr(r), 30,
								 arm64_check_offset(1, arm64_type_size(svtt), i)); }
		else
			{ MCC_TRACE("br\n"); arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt), intr(r), 30,
								 arm64_check_offset(1, arm64_type_size(svtt), i)); }
		return;
	}

	if ((svr & ~VT_VALMASK) == VT_LVAL && svrv < VT_CONST) { MCC_TRACE("br\n");
		if ((svtt & VT_BTYPE) != VT_VOID) { MCC_TRACE("br\n");
			if (IS_FREG(r))
				{ MCC_TRACE("br\n"); arm64_ldrv(arm64_type_size(svtt), fltr(r), intr(svrv), 0); }
			else
				{ MCC_TRACE("br\n"); arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
									 intr(r), intr(svrv), 0); }
		}
		return;
	}

	if (svr == (VT_CONST | VT_LVAL | VT_SYM)) { MCC_TRACE("br\n");
		if (sv->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_MACHO
			arm64_macho_tls_addr(sv->sym, svcoff);
			if (IS_FREG(r))
				{ MCC_TRACE("br\n"); arm64_ldrv(arm64_type_size(svtt), fltr(r), 30, 0); }
			else
				{ MCC_TRACE("br\n"); arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
									 intr(r), 30, 0); }
#else
			arm64_tls_base_x30();
			greloca(cur_text_section, sv->sym, ind,
							R_AARCH64_TLSLE_ADD_TPREL_HI12, 0);
			o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
				ARM64_RN(30) | ARM64_RD(30));
			greloca(cur_text_section, sv->sym, ind,
							R_AARCH64_TLSLE_ADD_TPREL_LO12, 0);
			o(ARM64_ADD_IMM | ARM64_SF(1) |
				ARM64_RN(30) | ARM64_RD(30));
			if (IS_FREG(r))
				{ MCC_TRACE("br\n"); arm64_ldrv(arm64_type_size(svtt), fltr(r), 30, svcoff); }
			else
				{ MCC_TRACE("br\n"); arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
									 intr(r), 30, svcoff); }
#endif
			return;
		}
		arm64_sym(30, sv->sym,
							arm64_check_offset(0, arm64_type_size(svtt), svcoff));
		if (IS_FREG(r))
			{ MCC_TRACE("br\n"); arm64_ldrv(arm64_type_size(svtt), fltr(r), 30,
								 arm64_check_offset(1, arm64_type_size(svtt), svcoff)); }
		else
			{ MCC_TRACE("br\n"); arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt), intr(r), 30,
								 arm64_check_offset(1, arm64_type_size(svtt), svcoff)); }
		return;
	}

	if (svr == (VT_CONST | VT_SYM)) { MCC_TRACE("br\n");
		if (sv->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_MACHO
			arm64_macho_tls_addr(sv->sym, svcul);
			o(ARM64_MOV_REG | ARM64_SF(1) | ARM64_RM(30) | ARM64_RD(intr(r)));
#else
			arm64_tls_base_x30();
			greloca(cur_text_section, sv->sym, ind,
							R_AARCH64_TLSLE_ADD_TPREL_HI12, svcul);
			o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
				ARM64_RN(30) | ARM64_RD(30));
			greloca(cur_text_section, sv->sym, ind,
							R_AARCH64_TLSLE_ADD_TPREL_LO12, svcul);
			o(ARM64_ADD_IMM | ARM64_SF(1) |
				ARM64_RN(30) | ARM64_RD(intr(r)));
#endif
			return;
		}
		arm64_sym(intr(r), sv->sym, svcul);
		return;
	}

	if (svr == VT_CONST) { MCC_TRACE("br\n");
		if ((svtt & VT_BTYPE) != VT_VOID)
			{ MCC_TRACE("br\n"); arm64_movimm(intr(r), arm64_type_size(svtt) == 3 ? sv->c.i : (uint32_t)svcul); }
		return;
	}

	if (svr < VT_CONST) { MCC_TRACE("br\n");
		if (IS_FREG(r) && IS_FREG(svr))
			{ MCC_TRACE("br\n"); if (svtt == VT_LDOUBLE)
				{ MCC_TRACE("br\n"); o(ARM64_MOV_V16B | fltr(r) | fltr(svr) * 0x10020); }
			else
				{ MCC_TRACE("br\n"); o(ARM64_FMOV_SCALAR | fltr(r) | fltr(svr) << 5); } }
		else if (!IS_FREG(r) && !IS_FREG(svr))
			{ MCC_TRACE("br\n"); o(ARM64_MOV_REG | ARM64_SF(1) | intr(r) | intr(svr) << 16); }
		else
			{ MCC_TRACE("br\n"); assert(0); }
		return;
	}

	if (svr == VT_LOCAL) { MCC_TRACE("br\n");
		if (-svcoff < 0x1000)
			{ MCC_TRACE("br\n"); o(0xd10003a0 | intr(r) | -svcoff << 10); }
		else { MCC_TRACE("br\n");
			arm64_movimm(30, -svcoff);
			o(0xcb0003a0 | intr(r) | (uint32_t)30 << 16);
		}
		return;
	}

	if (svr == VT_LLOCAL) { MCC_TRACE("br\n");
		arm64_ldrx(0, 3, intr(r), 29, svcoff);
		return;
	}

	if (svr == VT_JMP || svr == VT_JMPI) { MCC_TRACE("br\n");
		int t = (svr == VT_JMPI);
		arm64_movimm(intr(r), t);
		o(ARM64_B | 2);
		gsym(svcul);
		arm64_movimm(intr(r), t ^ 1);
		return;
	}

	if (svr == (VT_LLOCAL | VT_LVAL)) { MCC_TRACE("br\n");
		arm64_ldrx(0, 3, 30, 29, svcoff);
		if (IS_FREG(r))
			{ MCC_TRACE("br\n"); arm64_ldrv(arm64_type_size(svtt), fltr(r), 30, 0); }
		else
			{ MCC_TRACE("br\n"); arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
								 intr(r), 30, 0); }
		return;
	}

	if (svr == VT_CMP) { MCC_TRACE("br\n");
		arm64_load_cmp(r, sv);
		return;
	}

	printf("load(%x, (%x, %x, %lx))\n", r, svtt, sv->r, (long)svcul);
	assert(0);
}

ST_FUNC void store(int r, SValue *sv) { MCC_TRACE("enter\n");
	int svtt = sv->type.t;
	int svr = sv->r & ~VT_BOUNDED;
	int svrv = svr & VT_VALMASK;
	uint64_t svcoff = (uint64_t)(int64_t)(int32_t)sv->c.i;

	if (svr == (VT_LOCAL | VT_LVAL)) { MCC_TRACE("br\n");
		if (IS_FREG(r))
			{ MCC_TRACE("br\n"); arm64_strv(arm64_type_size(svtt), fltr(r), 29, svcoff); }
		else
			{ MCC_TRACE("br\n"); arm64_strx(arm64_type_size(svtt), intr(r), 29, svcoff); }
		return;
	}

	if (svr == (VT_CONST | VT_LVAL)) { MCC_TRACE("br\n");
		uint64_t i = sv->c.i;

		if (sv->sym && (sv->sym->type.t & VT_TLS)) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_MACHO
			arm64_macho_tls_addr(sv->sym, i);
			if (IS_FREG(r))
				{ MCC_TRACE("br\n"); arm64_strv(arm64_type_size(svtt), fltr(r), 30, 0); }
			else
				{ MCC_TRACE("br\n"); arm64_strx(arm64_type_size(svtt), intr(r), 30, 0); }
#else
			arm64_tls_base_x30();
			greloca(cur_text_section, sv->sym, ind,
							R_AARCH64_TLSLE_ADD_TPREL_HI12, 0);
			o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
				ARM64_RN(30) | ARM64_RD(30));
			greloca(cur_text_section, sv->sym, ind,
							R_AARCH64_TLSLE_ADD_TPREL_LO12, 0);
			o(ARM64_ADD_IMM | ARM64_SF(1) |
				ARM64_RN(30) | ARM64_RD(30));
			if (IS_FREG(r))
				{ MCC_TRACE("br\n"); arm64_strv(arm64_type_size(svtt), fltr(r), 30, i); }
			else
				{ MCC_TRACE("br\n"); arm64_strx(arm64_type_size(svtt), intr(r), 30, i); }
#endif
			return;
		}
		if (sv->sym)
			{ MCC_TRACE("br\n"); arm64_sym(30, sv->sym,
								arm64_check_offset(0, arm64_type_size(svtt), i)); }
		else
			{ MCC_TRACE("br\n"); arm64_movimm(30, i), i = 0; }
		if (IS_FREG(r))
			{ MCC_TRACE("br\n"); arm64_strv(arm64_type_size(svtt), fltr(r), 30,
								 arm64_check_offset(1, arm64_type_size(svtt), i)); }
		else
			{ MCC_TRACE("br\n"); arm64_strx(arm64_type_size(svtt), intr(r), 30,
								 arm64_check_offset(1, arm64_type_size(svtt), i)); }
		return;
	}

	if ((svr & ~VT_VALMASK) == VT_LVAL && svrv < VT_CONST) { MCC_TRACE("br\n");
		if (IS_FREG(r))
			{ MCC_TRACE("br\n"); arm64_strv(arm64_type_size(svtt), fltr(r), intr(svrv), 0); }
		else
			{ MCC_TRACE("br\n"); arm64_strx(arm64_type_size(svtt), intr(r), intr(svrv), 0); }
		return;
	}

	if (svr == (VT_CONST | VT_LVAL | VT_SYM)) { MCC_TRACE("br\n");
		if (sv->sym->type.t & VT_TLS) { MCC_TRACE("br\n");
#ifdef MCC_TARGET_MACHO
			arm64_macho_tls_addr(sv->sym, svcoff);
			if (IS_FREG(r))
				{ MCC_TRACE("br\n"); arm64_strv(arm64_type_size(svtt), fltr(r), 30, 0); }
			else
				{ MCC_TRACE("br\n"); arm64_strx(arm64_type_size(svtt), intr(r), 30, 0); }
#else
			arm64_tls_base_x30();
			greloca(cur_text_section, sv->sym, ind,
							R_AARCH64_TLSLE_ADD_TPREL_HI12, 0);
			o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
				ARM64_RN(30) | ARM64_RD(30));
			greloca(cur_text_section, sv->sym, ind,
							R_AARCH64_TLSLE_ADD_TPREL_LO12, 0);
			o(ARM64_ADD_IMM | ARM64_SF(1) |
				ARM64_RN(30) | ARM64_RD(30));
			if (IS_FREG(r))
				{ MCC_TRACE("br\n"); arm64_strv(arm64_type_size(svtt), fltr(r), 30, svcoff); }
			else
				{ MCC_TRACE("br\n"); arm64_strx(arm64_type_size(svtt), intr(r), 30, svcoff); }
#endif
			return;
		}
		arm64_sym(30, sv->sym,
							arm64_check_offset(0, arm64_type_size(svtt), svcoff));
		if (IS_FREG(r))
			{ MCC_TRACE("br\n"); arm64_strv(arm64_type_size(svtt), fltr(r), 30,
								 arm64_check_offset(1, arm64_type_size(svtt), svcoff)); }
		else
			{ MCC_TRACE("br\n"); arm64_strx(arm64_type_size(svtt), intr(r), 30,
								 arm64_check_offset(1, arm64_type_size(svtt), svcoff)); }
		return;
	}

	printf("store(%x, (%x, %x, %lx))\n", r, svtt, sv->r, (long)svcoff);
	assert(0);
}

static void arm64_gen_bl_or_b(int b) { MCC_TRACE("enter\n");
	if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && (vtop->r & VT_SYM)) { MCC_TRACE("br\n");
		greloca(cur_text_section, vtop->sym, ind,
						b ? R_AARCH64_JUMP26 : R_AARCH64_CALL26, 0);
		o(b ? ARM64_B : ARM64_BL);
	} else { MCC_TRACE("br\n");
#if MCC_CONFIG_DIAG_RT >= 2
		vtop->r &= ~VT_MUSTBOUND;
#endif
		o((b ? ARM64_BR : ARM64_BLR) | intr(gv(MCC_RC_R30)) << 5);
	}
}

#if MCC_CONFIG_DIAG_RT >= 2

static void gen_bounds_call(int v) { MCC_TRACE("enter\n");
	Sym *sym = external_helper_sym(v);

	greloca(cur_text_section, sym, ind, R_AARCH64_CALL26, 0);
	o(ARM64_BL);
}

static void gen_bounds_prolog(void) { MCC_TRACE("enter\n");
	func_bound_offset = lbounds_section->data_offset;
	func_bound_ind = ind;
	func_bound_add_epilog = 0;
	o(ARM64_NOP);
	o(ARM64_NOP);
	o(ARM64_NOP);
	o(ARM64_NOP);
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
		arm64_sym(0, sym_data, 0);
		gen_bounds_call(TOK___bound_local_new);
		ind = saved_ind;
	}

	o(0xa9bf07e0);
	o(0x3c9f0fe0);
	arm64_sym(0, sym_data, 0);
	gen_bounds_call(TOK___bound_local_delete);
	o(0x3cc107e0);
	o(0xa8c107e0);
}
#endif

static int arm64_hfa_aux(CType *type, int *fsize, int num) { MCC_TRACE("enter\n");
	if (is_float(type->t)) { MCC_TRACE("br\n");
		int a, n = type_size(type, &a);
		if (num >= 4 || (*fsize && *fsize != n))
			{ MCC_TRACE("br\n"); return -1; }
		*fsize = n;
		return num + 1;
	} else if ((type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
		Sym *field;
		if (!IS_UNION(type->t)) { MCC_TRACE("br\n");
			int num0 = num;
			for (field = type->ref->next; field; field = field->next) { MCC_TRACE("br\n");
				if (field->c != (num - num0) * *fsize)
					{ MCC_TRACE("br\n"); return -1; }
				num = arm64_hfa_aux(&field->type, fsize, num);
				if (num == -1)
					{ MCC_TRACE("br\n"); return -1; }
			}
			if (type->ref->c != (num - num0) * *fsize)
				{ MCC_TRACE("br\n"); return -1; }
			return num;
		} else { MCC_TRACE("br\n");
			int num0 = num;
			for (field = type->ref->next; field; field = field->next) { MCC_TRACE("br\n");
				int num1 = arm64_hfa_aux(&field->type, fsize, num0);
				if (num1 == -1)
					{ MCC_TRACE("br\n"); return -1; }
				num = num1 < num ? num : num1;
			}
			if (type->ref->c != (num - num0) * *fsize)
				{ MCC_TRACE("br\n"); return -1; }
			return num;
		}
	} else if (type->t & VT_ARRAY) { MCC_TRACE("br\n");
		int num1;
		if (!type->ref->c)
			{ MCC_TRACE("br\n"); return num; }
		num1 = arm64_hfa_aux(&type->ref->type, fsize, num);
		if (num1 == -1 || (num1 != num && type->ref->c > 4))
			{ MCC_TRACE("br\n"); return -1; }
		num1 = num + type->ref->c * (num1 - num);
		if (num1 > 4)
			{ MCC_TRACE("br\n"); return -1; }
		return num1;
	}
	return -1;
}

static int arm64_hfa(CType *type, unsigned *fsize) { MCC_TRACE("enter\n");
	if ((type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
		int sz = 0;
		int n = arm64_hfa_aux(type, &sz, 0);
		if (0 < n && n <= 4) { MCC_TRACE("br\n");
			if (fsize)
				{ MCC_TRACE("br\n"); *fsize = sz; }
			return n;
		}
	}
	return 0;
}

static unsigned long arm64_pcs_aux(int variadic, int n, CType **type, unsigned long *a) { MCC_TRACE("enter\n");
	int nx = 0;
	int nv = 0;
	unsigned long ns = 32;
	for (int i = 0; i < n; i++) { MCC_TRACE("br\n");
		int hfa = arm64_hfa(type[i], 0);
		int size, align, bt;

		bt = type[i]->t & VT_BTYPE;
		if (bt == VT_PTR || bt == VT_FUNC)
			{ MCC_TRACE("br\n"); size = align = 8; }
		else
			{ MCC_TRACE("br\n"); size = type_size(type[i], &align); }

#if defined(MCC_TARGET_MACHO)
		if (variadic && i == variadic) { MCC_TRACE("br\n");
			nx = 8;
			nv = 8;
		}

#elif defined(MCC_TARGET_PE)
		if (variadic && i >= variadic) { MCC_TRACE("br\n");
			hfa = 0;
			if (is_float(bt))
				{ MCC_TRACE("br\n"); bt = VT_INT, size = align = 8; }
		}
#endif
		if (hfa)
			{ MCC_TRACE("br\n"); ; }
		else if (size > 16) { MCC_TRACE("br\n");
			if (nx < 8)
				{ MCC_TRACE("br\n"); a[i] = nx++ << 1 | 1; }
			else { MCC_TRACE("br\n");
				ns = (ns + 7) & ~7;
				a[i] = ns | 1;
				ns += 8;
			}
			continue;
		} else if (bt == VT_STRUCT)
			{ MCC_TRACE("br\n"); size = (size + 7) & ~7; }

		if (is_float(bt) && nv < 8) { MCC_TRACE("br\n");
			a[i] = 16 + (nv++ << 1);
			continue;
		}

		if (hfa && nv + hfa <= 8) { MCC_TRACE("br\n");
			a[i] = 16 + (nv << 1);
			nv += hfa;
			continue;
		}

		if (hfa) { MCC_TRACE("br\n");
			nv = 8;
			size = (size + 7) & ~7;
		}

		if (hfa || bt == VT_LDOUBLE) { MCC_TRACE("br\n");
			ns = (ns + 7) & ~7;
			ns = (ns + align - 1) & -align;
		}

		if (bt == VT_FLOAT)
			{ MCC_TRACE("br\n"); size = 8; }

		if (hfa || is_float(bt)) { MCC_TRACE("br\n");
			a[i] = ns;
			ns += size;
			continue;
		}

		if (bt != VT_STRUCT && size <= 8 && nx < 8) { MCC_TRACE("br\n");
			a[i] = nx++ << 1;
			continue;
		}

		if (align == 16)
			{ MCC_TRACE("br\n"); nx = (nx + 1) & ~1; }

		if (bt != VT_STRUCT && size == 16 && nx < 7) { MCC_TRACE("br\n");
			a[i] = nx << 1;
			nx += 2;
			continue;
		}

		if (bt == VT_STRUCT && size <= (8 - nx) * 8) { MCC_TRACE("br\n");
			a[i] = nx << 1;
			nx += (size + 7) >> 3;
			continue;
		}

		nx = 8;

		ns = (ns + 7) & ~7;
		ns = (ns + align - 1) & -align;

		if (bt == VT_STRUCT) { MCC_TRACE("br\n");
			a[i] = ns;
			ns += size;
			continue;
		}

		if (size < 8)
			{ MCC_TRACE("br\n"); size = 8; }

		a[i] = ns;
		ns += size;
	}

	return ns - 32;
}

static unsigned long arm64_pcs(int variadic, int n, CType **type, unsigned long *a) { MCC_TRACE("enter\n");
	unsigned long stack;

	if ((type[0]->t & VT_BTYPE) == VT_VOID)
		{ MCC_TRACE("br\n"); a[0] = -1; }
	else { MCC_TRACE("br\n");
		arm64_pcs_aux(0, 1, type, a);
		assert(a[0] == 0 || a[0] == 1 || a[0] == 16);
	}

	stack = arm64_pcs_aux(variadic, n - 1, type + 1, a + 1);

	if (0) { MCC_TRACE("br\n");
		for (int i = 0; i < n; i++) { MCC_TRACE("br\n");
			if (!i)
				{ MCC_TRACE("br\n"); printf("arm64_pcs return: "); }
			else
				{ MCC_TRACE("br\n"); printf("arm64_pcs arg %d: ", i); }
			if (a[i] == (unsigned long)-1)
				{ MCC_TRACE("br\n"); printf("void\n"); }
			else if (a[i] == 1 && !i)
				{ MCC_TRACE("br\n"); printf("X8 pointer\n"); }
			else if (a[i] < 16)
				{ MCC_TRACE("br\n"); printf("X%lu%s\n", a[i] / 2, a[i] & 1 ? " pointer" : ""); }
			else if (a[i] < 32)
				{ MCC_TRACE("br\n"); printf("V%lu\n", a[i] / 2 - 8); }
			else
				{ MCC_TRACE("br\n"); printf("stack %lu%s\n",
							 (a[i] - 32) & ~1, a[i] & 1 ? " pointer" : ""); }
		}
	}

	return stack;
}

static int n_func_args(CType *type) { MCC_TRACE("enter\n");
	int n_args = 0;
	Sym *arg;

	for (arg = type->ref->next; arg; arg = arg->next)
		n_args++;
	return n_args;
}

static void arm64_sub_sp(uint64_t diff) { MCC_TRACE("enter\n");
	if (!diff)
		{ MCC_TRACE("br\n"); return; }
#ifdef MCC_TARGET_PE
	if (diff >= 4096) { MCC_TRACE("br\n");
		Sym *sym = external_helper_sym(TOK___chkstk);

		arm64_movimm(15, diff >> 4);
		greloca(cur_text_section, sym, ind, R_AARCH64_CALL26, 0);
		o(ARM64_BL);
		o(0xcb2f73ff);
		return;
	}
#endif
	if (!(diff >> 24)) { MCC_TRACE("br\n");
		if (diff & 0xffful)
			{ MCC_TRACE("br\n"); o(ARM64_SUB_IMM | ARM64_SF(1) | 0 | ARM64_RN(31) | ARM64_RD(31) | ARM64_IMM12(diff & 0xfff)); }
		if (diff >> 12)
			{ MCC_TRACE("br\n"); o(ARM64_SUB_IMM | ARM64_SF(1) | ARM64_SH(1) | ARM64_RN(31) | ARM64_RD(31) | ARM64_IMM12((diff >> 12) & 0xfff)); }
	} else { MCC_TRACE("br\n");
		arm64_movimm(16, diff);
		o(0xCB3063FFU);
	}
}

static int gv_addr(int r) { MCC_TRACE("enter\n");
	gaddrof();
	vtop->type.t = VT_PTR;
	return gv(r);
}

ST_FUNC void gfunc_call(int nb_args) { MCC_TRACE("enter\n");
	CType *return_type;
	CType **t;
	unsigned long *a, *a1;
	unsigned long stack;
	int func_type = vtop[-nb_args].type.ref->f.func_type;
	int variadic = (func_type == FUNC_ELLIPSIS);
	int old_style = (func_type == FUNC_OLD);
	int var_nb_arg = variadic ? n_func_args(&vtop[-nb_args].type) : 0;

	save_regs(nb_args + 1);

#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gbound_args(nb_args); }
#endif

	return_type = &vtop[-nb_args].type.ref->type;
	if ((return_type->t & VT_BTYPE) == VT_STRUCT)
		{ MCC_TRACE("br\n"); --nb_args; }

	t = mcc_malloc((nb_args + 1) * sizeof(*t));
	a = mcc_malloc((nb_args + 1) * sizeof(*a));
	a1 = mcc_malloc((nb_args + 1) * sizeof(*a1));

	t[0] = return_type;
	for (int i = 0; i < nb_args; i++)
		t[nb_args - i] = &vtop[-i].type;

	stack = arm64_pcs(
#ifdef MCC_TARGET_PE
			old_style ? -1 :
#endif
								var_nb_arg,
			nb_args + 1, t, a);

	for (int i = nb_args; i; i--)
		if (a[i] & 1) { MCC_TRACE("br\n");
			SValue *arg = &vtop[i - nb_args];
			int align, size = type_size(&arg->type, &align);
			assert((arg->type.t & VT_BTYPE) == VT_STRUCT);
			stack = (stack + align - 1) & -align;
			a1[i] = stack;
			stack += size;
		}

	stack = (stack + 15) >> 4 << 4;

	if (stack >= 0x1000000)
		{ MCC_TRACE("br\n"); mcc_error("stack size too big %lu", stack); }
	arm64_sub_sp(stack);

	for (int i = nb_args; i; i--) { MCC_TRACE("br\n");
		vpushv(vtop - nb_args + i);

		if (a[i] & 1) { MCC_TRACE("br\n");
			int r = get_reg(MCC_RC_INT);
			arm64_spoff(intr(r), a1[i]);
			vset(&vtop->type, r | VT_LVAL, 0);
			vswap();
			vstore();
			if (a[i] >= 32) { MCC_TRACE("br\n");
				r = get_reg(MCC_RC_INT);
				arm64_spoff(intr(r), a1[i]);
				arm64_strx(3, intr(r), 31, (a[i] - 32) >> 1 << 1);
			}
		} else if (a[i] >= 32) { MCC_TRACE("br\n");
			if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
				int r = get_reg(MCC_RC_INT);
				arm64_spoff(intr(r), a[i] - 32);
				vset(&vtop->type, r | VT_LVAL, 0);
				vswap();
				vstore();
			} else if (is_float(vtop->type.t)) { MCC_TRACE("br\n");
				gv(MCC_RC_FLOAT);
				arm64_strv(arm64_type_size(vtop[0].type.t),
									 fltr(vtop[0].r), 31, a[i] - 32);
			} else { MCC_TRACE("br\n");
				gv(MCC_RC_INT);
				arm64_strx(3,
									 intr(vtop[0].r), 31, a[i] - 32);
			}
		}

		--vtop;
	}

	for (int i = nb_args; i; i--, vtop--) { MCC_TRACE("br\n");
		if (a[i] < 16 && !(a[i] & 1)) { MCC_TRACE("br\n");
			if ((variadic || old_style) && i > var_nb_arg && is_float(vtop->type.t)) { MCC_TRACE("br\n");
				gv(MCC_RC_FLOAT);
				if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE)
					{ MCC_TRACE("br\n"); o(ARM64_FMOV_XD | intr(a[i] / 2) | fltr(vtop->r) << 5); }
				else
					{ MCC_TRACE("br\n"); o(ARM64_FMOV_WS | intr(a[i] / 2) | fltr(vtop->r) << 5); }
			} else if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
				int align, size = type_size(&vtop->type, &align);
				if (size) { MCC_TRACE("br\n");
					gv_addr(MCC_RC_R(a[i] / 2));
					arm64_ldrs(a[i] / 2, size);
				}
			} else
				{ MCC_TRACE("br\n"); gv(MCC_RC_R(a[i] / 2)); }
		} else if (a[i] < 16)
			{ MCC_TRACE("br\n"); arm64_spoff(a[i] / 2, a1[i]); }
		else if (a[i] < 32) { MCC_TRACE("br\n");
			if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
				uint32_t sz, n = arm64_hfa(&vtop->type, &sz);
				if (n > 0) { MCC_TRACE("br\n");
					gv_addr(MCC_RC_R30);
					for (uint32_t j = 0; j < n; j++)
						o(0x3d4003c0 |
							(sz & 16) << 19 | -(sz & 8) << 27 | (sz & 4) << 29 |
							(a[i] / 2 - 8 + j) |
							j << 10);
				} else { MCC_TRACE("br\n");
					gv(MCC_RC_F(a[i] / 2 - 8));
				}
			} else
				{ MCC_TRACE("br\n"); gv(MCC_RC_F(a[i] / 2 - 8)); }
		}
	}

	if ((return_type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
		if (a[0] == 1) { MCC_TRACE("br\n");
			gv_addr(MCC_RC_R(8));
			--vtop;
		} else
			{ MCC_TRACE("br\n"); vswap(); }
	}

	arm64_gen_bl_or_b(0);
	--vtop;
	if (stack & 0xfff)
		{ MCC_TRACE("br\n"); o(0x910003ff | (stack & 0xfff) << 10); }
	if (stack >> 12)
		{ MCC_TRACE("br\n"); o(0x914003ff | (stack >> 12) << 10); }

	{
		int rt = return_type->t;
		int bt = rt & VT_BTYPE;
		if (bt == VT_STRUCT && !(a[0] & 1)) { MCC_TRACE("br\n");
			gv_addr(MCC_RC_R(8));
			--vtop;
			if (a[0] == 0) { MCC_TRACE("br\n");
				int align, size = type_size(return_type, &align);
				assert(size <= 16);
				if (size > 8)
					{ MCC_TRACE("br\n"); o(0xa9000500); }
				else if (size)
					{ MCC_TRACE("br\n"); arm64_strx(size > 4
												 ? 3
										 : size > 2
												 ? 2
												 : size > 1,
										 0, 8, 0); }
			} else if (a[0] == 16) { MCC_TRACE("br\n");
				uint32_t sz, n = arm64_hfa(return_type, &sz);
				for (uint32_t j = 0; j < n; j++)
					o(0x3d000100 |
						(sz & 16) << 19 | -(sz & 8) << 27 | (sz & 4) << 29 |
						(fltr(REG_FRET) + j) |
						j << 10);
			}
		}
	}

	mcc_free(a1);
	mcc_free(a);
	mcc_free(t);
}

#define arm64_func_va_list_stack (mcc_state->cg_arm64_func_va_list_stack)
#define arm64_func_va_list_gr_offs (mcc_state->cg_arm64_func_va_list_gr_offs)
#define arm64_func_va_list_vr_offs (mcc_state->cg_arm64_func_va_list_vr_offs)
#define arm64_func_sub_sp_offset (mcc_state->cg_arm64_func_sub_sp_offset)

#define arm64_func_start_offset (mcc_state->cg_arm64_func_start_offset)
#define ARM64_FUNC_STACK_SETUP_SLOTS 6

#ifdef MCC_TARGET_PE
static unsigned long arm64_pe_param_off(unsigned long a) { MCC_TRACE("enter\n");
	return a < 16
						 ? 160 + a / 2 * 8
				 : a < 32
						 ? 16 + (a - 16) / 2 * 16
						 : 224 + ((a - 32) >> 1 << 1);
}
#endif

#ifdef MCC_TARGET_MACHO

static void arm64_load_stack_guard(void) { MCC_TRACE("enter\n");
	Sym *guard = external_helper_sym(TOK___stack_chk_guard);
	arm64_sym(16, guard, 0);
	o(ARM64_LDR_X | ARM64_RN(16) | 16);
}

static void gen_stack_chk_prolog(void) { MCC_TRACE("enter\n");
	func_stack_chk_loc = (loc -= 8);
	arm64_load_stack_guard();
	arm64_strx(3, 16, 29, (uint64_t)func_stack_chk_loc);
}

static void gen_stack_chk_epilog(void) { MCC_TRACE("enter\n");
	Sym *fail = external_helper_sym(TOK___stack_chk_fail);
	arm64_ldrx(0, 3, 17, 29, (uint64_t)func_stack_chk_loc);
	arm64_load_stack_guard();
	o(0xeb11021f);
	o(0x54000040);
	greloca(cur_text_section, fail, ind, R_AARCH64_CALL26, 0);
	o(ARM64_BL);
}
#endif

/* -fasan-shadow stack redzones (mirrors x86_64 gen_asan_stack_{prolog,epilog}):
   the prologue reserves a slot and, once every local is known, the epilogue
   patches in a __asan_stack_enter(table, x29) call there and emits a
   __asan_stack_leave(table, x29) call. add_asan_locals (mccgen.c) records each
   local's x29-relative offset + size into .asan_lstack (arm64 locals sit below
   x29 like x86 locals sit below rbp, so the runtime's fp+offset works as-is). */
#define ARM64_ASAN_ENTER_SLOTS 4
static addr_t arm64_asan_off;
static addr_t arm64_asan_ind;

static void arm64_asan_stack_call(Sym *tab, const char *name) { MCC_TRACE("enter\n");
	arm64_sym(0, tab, 0);       /* x0 = &table (adrp + GOT ldr, 2 insns) */
	o(0xaa1d03e1);              /* mov x1, x29                           */
	greloca(cur_text_section, external_helper_sym(tok_alloc_const(name)), ind,
					R_AARCH64_CALL26, 0);
	o(ARM64_BL);                /* bl <name>                             */
}

static void gen_asan_stack_prolog(void) { MCC_TRACE("enter\n");
	if (!mcc_state->do_asan_shadow)
		{ MCC_TRACE("br\n"); return; }
	if (!asan_lstack_section)
		{ MCC_TRACE("br\n"); asan_lstack_section =
			new_section(mcc_state, ".asan_lstack", SHT_PROGBITS, SHF_ALLOC); }
	arm64_asan_off = asan_lstack_section->data_offset;
	arm64_asan_ind = ind;
	for (int i = 0; i < ARM64_ASAN_ENTER_SLOTS; i++)
		o(ARM64_NOP);
}

static void gen_asan_stack_epilog(void) { MCC_TRACE("enter\n");
	addr_t saved_ind;
	Sym *tab;
	if (!mcc_state->do_asan_shadow)
		{ MCC_TRACE("br\n"); return; }
	if (!gen_asan_stack_epilog_head(arm64_asan_off, &tab))
		{ MCC_TRACE("br\n"); return; }
	saved_ind = ind;
	ind = arm64_asan_ind;
	arm64_asan_stack_call(tab, "__asan_stack_enter");
	ind = saved_ind;
	/* leave runs in the epilogue where x0/d0 hold the return value */
	o(0xd10043ff); /* sub  sp, sp, #16   */
	o(0xf90003e0); /* str  x0, [sp]       */
	o(0x9e660000); /* fmov x0, d0         */
	o(0xf90007e0); /* str  x0, [sp, #8]   */
	arm64_asan_stack_call(tab, "__asan_stack_leave");
	o(0xf94007e0); /* ldr  x0, [sp, #8]   */
	o(0x9e670000); /* fmov d0, x0         */
	o(0xf94003e0); /* ldr  x0, [sp]       */
	o(0x910043ff); /* add  sp, sp, #16    */
}

ST_FUNC void gfunc_prolog(Sym *func_sym) { MCC_TRACE("enter\n");
	CType *func_type = &func_sym->type;
	int n = 0;
	int i = 0;
	Sym *sym;
	CType **t;
	unsigned long *a;
	int use_x8 = 0;
	int last_int = 0;
	int last_float = 0;
	int variadic = func_sym->type.ref->f.func_type == FUNC_ELLIPSIS;
	int var_nb_arg = variadic ? n_func_args(&func_sym->type) : 0;

	func_vc = 144;

	for (sym = func_type->ref; sym; sym = sym->next)
		++n;

#ifdef MCC_TARGET_PE
	n += variadic;
#endif

	t = mcc_malloc(n * sizeof(*t));
	a = mcc_malloc(n * sizeof(*a));
	for (sym = func_type->ref; sym; sym = sym->next)
		t[i++] = &sym->type;

#ifdef MCC_TARGET_PE
	if (variadic)
		{ MCC_TRACE("br\n"); t[i] = &int_type; }
#endif

	arm64_func_va_list_stack = arm64_pcs(var_nb_arg, n, t, a);

#ifdef MCC_TARGET_PE
	if (variadic)
		{ MCC_TRACE("br\n"); arm64_func_va_list_stack = arm64_pe_param_off(a[n - 1]); }
#endif

#if !defined(MCC_TARGET_MACHO)
	if (variadic) { MCC_TRACE("br\n");
		use_x8 = 1;
		last_int = 4;
		last_float = 4;
	}
#endif

	if (a && a[0] == 1)
		{ MCC_TRACE("br\n"); use_x8 = 1; }
	for (i = 1, sym = func_type->ref->next; sym; i++, sym = sym->next) { MCC_TRACE("br\n");
		if (a[i] < 16) { MCC_TRACE("br\n");
			int last, align, size = type_size(&sym->type, &align);
			last = a[i] / 4 + 1 + (size - 1) / 8;
			last_int = last > last_int ? last : last_int;
		} else if (a[i] < 32) { MCC_TRACE("br\n");
			int last, hfa = arm64_hfa(&sym->type, 0);
			last = a[i] / 4 - 3 + (hfa ? hfa - 1 : 0);
			last_float = last > last_float ? last : last_float;
		}
	}

	last_int = last_int > 4 ? 4 : last_int;
	last_float = last_float > 4 ? 4 : last_float;

	arm64_func_start_offset = ind;
	o(0xa9b27bfd);
	o(0x910003fd);

	for (i = 0; i < last_float; i++)
		o(0xad0087e0 + i * 0x10000 + (i << 11) + (i << 1));
	if (use_x8)
		{ MCC_TRACE("br\n"); o(0xa90923e8); }
	for (i = 0; i < last_int; i++)
		o(0xa90a07e0 + i * 0x10000 + (i << 11) + (i << 1));

	arm64_func_va_list_gr_offs = -64;
	arm64_func_va_list_vr_offs = -128;

	for (i = 1, sym = func_type->ref->next; sym; i++, sym = sym->next) { MCC_TRACE("br\n");
		int off = (a[i] < 16
									 ? 160 + a[i] / 2 * 8
							 : a[i] < 32
									 ? 16 + (a[i] - 16) / 2 * 16
									 : 224 + ((a[i] - 32) >> 1 << 1));

		gfunc_set_param(sym, off, a[i] & 1);

		if (a[i] < 16) { MCC_TRACE("br\n");
			int align, size = type_size(&sym->type, &align);
			arm64_func_va_list_gr_offs = (a[i] / 2 - 7 +
																		(!(a[i] & 1) && size > 8)) *
																	 8;
		} else if (a[i] < 32) { MCC_TRACE("br\n");
			uint32_t hfa = arm64_hfa(&sym->type, 0);
			arm64_func_va_list_vr_offs = (a[i] / 2 - 16 +
																		(hfa ? hfa : 1)) *
																	 16;
		}

		if (16 <= a[i] && a[i] < 32 && (sym->type.t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			uint32_t sz, k = arm64_hfa(&sym->type, &sz);
			if (k > 0 && sz < 16)
				{ MCC_TRACE("br\n"); for (uint32_t j = 0; j < k; j++) { MCC_TRACE("br\n");
					o(0x3d0003e0 | -(sz & 8) << 27 | (sz & 4) << 29 |
						((a[i] - 16) / 2 + j) | (off / sz + j) << 10);
				} }
		}
	}

	mcc_free(a);
	mcc_free(t);

	arm64_func_sub_sp_offset = ind;
	for (i = 0; i < ARM64_FUNC_STACK_SETUP_SLOTS; ++i)
		o(ARM64_NOP);
	loc = 0;
	gen_asan_stack_prolog();
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gen_bounds_prolog(); }
#endif
#ifdef MCC_TARGET_MACHO
	func_stack_chk_loc = 0;
	if (mcc_state->stack_protector)
		{ MCC_TRACE("br\n"); gen_stack_chk_prolog(); }
#endif
}

ST_FUNC void gen_va_start(void) { MCC_TRACE("enter\n");
	int r;
	--vtop;
	r = intr(gv_addr(MCC_RC_INT));

#ifdef MCC_TARGET_PE
	if (arm64_func_va_list_stack) { MCC_TRACE("br\n");
		arm64_movimm(30, arm64_func_va_list_stack);
		o(0x8b1e03be);
	} else
		{ MCC_TRACE("br\n"); o(0x910283be); }
	o(0xf900001e | r << 5);
#else
	if (arm64_func_va_list_stack) { MCC_TRACE("br\n");
		arm64_movimm(30, arm64_func_va_list_stack + 224);
		o(0x8b1e03be);
	} else
		{ MCC_TRACE("br\n"); o(0x910383be); }
	o(0xf900001e | r << 5);

#if !defined(MCC_TARGET_MACHO)
	if (arm64_func_va_list_gr_offs) { MCC_TRACE("br\n");
		if (arm64_func_va_list_stack)
			{ MCC_TRACE("br\n"); o(0x910383be); }
		o(0xf900041e | r << 5);
	}

	if (arm64_func_va_list_vr_offs) { MCC_TRACE("br\n");
		o(0x910243be);
		o(0xf900081e | r << 5);
	}

	arm64_movimm(30, arm64_func_va_list_gr_offs);
	o(0xb900181e | r << 5);

	arm64_movimm(30, arm64_func_va_list_vr_offs);
	o(0xb9001c1e | r << 5);
#endif
#endif

	--vtop;
}

ST_FUNC void gen_va_arg(CType *t) { MCC_TRACE("enter\n");
	int align, size = type_size(t, &align);
	uint32_t r0, r1;

#ifdef MCC_TARGET_PE
	int indirect = 0, slot = (size + 7) & -8;

	if (size > 16)
		{ MCC_TRACE("br\n"); indirect = 1, slot = 8; }

	r0 = intr(gv_addr(MCC_RC_INT));
	r1 = get_reg(MCC_RC_INT);
	vtop[0].r = r1 | VT_LVAL;
	r1 = intr(r1);

	o(ARM64_LDR_X | ARM64_RN(r0) | r1);
	if (slot) { MCC_TRACE("br\n");
		if (slot == 16) { MCC_TRACE("br\n");
			o(0x910363be);
			o(0xeb1e003f | r1 << 5);
			o(0x54000041);
			o(0x910383a0 | r1 | 29 << 5);
		}
		if (align == 16) { MCC_TRACE("br\n");
			o(0x91003c00 | r1 | r1 << 5);
			o(0x927cec00 | r1 | r1 << 5);
		}
		o(0x9100001e | r1 << 5 | slot << 10);
		o(0xf900001e | r0 << 5);
	}
	if (indirect)
		{ MCC_TRACE("br\n"); o(ARM64_LDR_X | ARM64_RN(r1) | r1); }

#else
	unsigned fsize = size, hfa = 1;

	if (!is_float(t->t))
		{ MCC_TRACE("br\n"); hfa = arm64_hfa(t, &fsize); }

	r0 = intr(gv_addr(MCC_RC_INT));
	r1 = get_reg(MCC_RC_INT);
	vtop[0].r = r1 | VT_LVAL;
	r1 = intr(r1);

	if (!hfa) { MCC_TRACE("br\n");
		uint32_t n = size > 16 ? 8 : (size + 7) & -8;

#if !defined(MCC_TARGET_MACHO)
		o(0xb940181e | r0 << 5);
		if (align == 16) { MCC_TRACE("br\n");
			assert(0);
			o(0x11003fde);
			o(0x121c6fde);
		}
		o(0x310003c0 | r1 | n << 10);
		o(0x540000ad);
#endif

		o(ARM64_LDR_X | ARM64_RN(r0) | r1);
		if (align == 16) { MCC_TRACE("br\n");
			o(0x91003c00 | r1 | r1 << 5);
			o(0x927cec00 | r1 | r1 << 5);
		}
		o(0x9100001e | r1 << 5 | n << 10);
		o(0xf900001e | r0 << 5);

#if !defined(MCC_TARGET_MACHO)
		o(ARM64_B | 4);
		o(0xb9001800 | r1 | r0 << 5);
		o(0xf9400400 | r1 | r0 << 5);
		o(0x8b3ec000 | r1 | r1 << 5);
#endif

		if (size > 16)
			{ MCC_TRACE("br\n"); o(ARM64_LDR_X | ARM64_RN(r1) | r1); }
	} else { MCC_TRACE("br\n");
		uint32_t ssz = (size + 7) & -(uint32_t)8;
#if !defined(MCC_TARGET_MACHO)
		uint32_t rsz = hfa << 4;
		uint32_t b1, b2;
		o(0xb9401c1e | r0 << 5);
		o(0x310003c0 | r1 | rsz << 10);
		b1 = ind;
		o(0x5400000d);
#endif
		o(ARM64_LDR_X | ARM64_RN(r0) | r1);
		if (fsize == 16) { MCC_TRACE("br\n");
			o(0x91003c00 | r1 | r1 << 5);
			o(0x927cec00 | r1 | r1 << 5);
		}
		o(0x9100001e | r1 << 5 | ssz << 10);
		o(0xf900001e | r0 << 5);
#if !defined(MCC_TARGET_MACHO)
		b2 = ind;
		o(ARM64_B);
		write32le(cur_text_section->data + b1, 0x5400000d | (ind - b1) << 3);
		o(0xb9001c00 | r1 | r0 << 5);
		o(0xf9400800 | r1 | r0 << 5);
		if (hfa == 1 || fsize == 16)
			{ MCC_TRACE("br\n"); o(0x8b3ec000 | r1 | r1 << 5); }
		else { MCC_TRACE("br\n");
			loc = (loc - size) & -(uint32_t)align;
			o(0x8b3ec000 | 30 | r1 << 5);
			arm64_movimm(r1, loc);
			o(0x8b0003a0 | r1 | r1 << 16);
			o(0x4c402bdc | (uint32_t)fsize << 7 |
				(uint32_t)(hfa == 2) << 15 |
				(uint32_t)(hfa == 3) << 14);
			o(0x0d00801c | r1 << 5 | (fsize == 8) << 10 |
				(uint32_t)(hfa != 2) << 13 |
				(uint32_t)(hfa != 3) << 21);
		}
		write32le(cur_text_section->data + b2, ARM64_B | ((ind - b2) >> 2));
#endif
	}
#endif
}

ST_FUNC int gfunc_sret(CType *vt, int variadic, CType *ret,
											 int *align, int *regsize) { MCC_TRACE("enter\n");
	return 0;
}

ST_FUNC void gfunc_return(CType *func_type) { MCC_TRACE("enter\n");
	CType *t = func_type;
	unsigned long a;

	arm64_pcs(0, 1, &t, &a);

	switch (a) { MCC_TRACE("br\n");
	case -1:
		break;
	case 0:
		if ((func_type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			int align, size = type_size(func_type, &align);
			gv_addr(MCC_RC_R(0));
			arm64_ldrs(0, size);
		} else
			{ MCC_TRACE("br\n"); gv(MCC_RC_IRET); }
		break;
	case 1: {
		CType type = *func_type;
		mk_pointer(&type);
		vset(&type, VT_LOCAL | VT_LVAL, func_vc);
		indir();
		vswap();
		vstore();
		break;
	}
	case 16:
		if ((func_type->t & VT_BTYPE) == VT_STRUCT) { MCC_TRACE("br\n");
			uint32_t sz, n = arm64_hfa(func_type, &sz);
			gv_addr(MCC_RC_R(0));
			for (uint32_t j = 0; j < n; j++)
				o(0x3d400000 |
					(sz & 16) << 19 | -(sz & 8) << 27 | (sz & 4) << 29 |
					(fltr(REG_FRET) + j) | j << 10);
		} else
			{ MCC_TRACE("br\n"); gv(MCC_RC_FRET); }
		break;
	default:
		assert(0);
	}
	vtop--;
}

ST_FUNC void gfunc_epilog(void) { MCC_TRACE("enter\n");
	gen_asan_stack_epilog();
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); gen_bounds_epilog(); }
#endif
#ifdef MCC_TARGET_MACHO
	if (func_stack_chk_loc)
		{ MCC_TRACE("br\n"); gen_stack_chk_epilog(); }
#endif

	if (loc) { MCC_TRACE("br\n");
		addr_t saved_ind = ind;
		addr_t patch_end = arm64_func_sub_sp_offset + ARM64_FUNC_STACK_SETUP_SLOTS * 4;
		uint64_t diff = (-loc + 15) & ~15;
		ind = arm64_func_sub_sp_offset;
		arm64_sub_sp(diff);
		for (int i = ind; i < patch_end; i += 4)
			write32le(cur_text_section->data + i, ARM64_NOP);
		ind = saved_ind;
	}
	o(0x910003bf);
	o(0xa8ce7bfd);

	o(0xd65f03c0);

#ifdef MCC_TARGET_PE
	pe_add_unwind_data(arm64_func_start_offset, ind, -loc);
#endif
}

ST_FUNC void gen_fill_nops(int bytes) { MCC_TRACE("enter\n");
	if ((bytes & 3))
		{ MCC_TRACE("br\n"); mcc_error("alignment of code section not multiple of 4"); }
	while (bytes > 0) { MCC_TRACE("br\n");
		o(ARM64_NOP);
		bytes -= 4;
	}
}

ST_FUNC int gjmp(int t) { MCC_TRACE("enter\n");
	int r = ind;
	if (nocode_wanted)
		{ MCC_TRACE("br\n"); return t; }
	o(t);
	return r;
}

ST_FUNC void gjmp_addr(int a) { MCC_TRACE("enter\n");
	assert(a - ind + 0x8000000 < 0x10000000);
	o(ARM64_B | (((a - ind) >> 2) & 0x3ffffff));
}

void arm64_vset_VT_CMP(int op) { MCC_TRACE("enter\n");
	if (op >= TOK_ULT && op <= TOK_GT) { MCC_TRACE("br\n");
		vtop->cmp_r = vtop->r;
		vset_VT_CMP(0x80);
	}
}

static void arm64_gen_opil(int op, uint32_t l);

static void arm64_load_cmp(int r, SValue *sv) { MCC_TRACE("enter\n");
	sv->r = sv->cmp_r;
	if (sv->c.i & 1) { MCC_TRACE("br\n");
		vpushi(1);
		arm64_gen_opil('^', 0);
	}
	if (r != sv->r) { MCC_TRACE("br\n");
		load(r, sv);
		sv->r = r;
	}
}

ST_FUNC int gjmp_cond(int op, int t) { MCC_TRACE("enter\n");
	int bt = vtop->type.t & VT_BTYPE;

	int inv = op & 1;
	vtop->r = vtop->cmp_r;

	if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
		uint32_t a, b, f = fltr(gv(MCC_RC_FLOAT));
		a = get_reg(MCC_RC_INT);
		vpushi(0);
		vtop[0].r = a;
		b = get_reg(MCC_RC_INT);
		a = intr(a);
		b = intr(b);
		o(0x4e083c00 | a | f << 5);
		o(0x4e183c00 | b | f << 5);
		o(0xaa000400 | a | a << 5 | b << 16);
		o(0xb4000040 | a | !!inv << 24);
		--vtop;
	} else if (bt == VT_FLOAT || bt == VT_DOUBLE) { MCC_TRACE("br\n");
		uint32_t a = fltr(gv(MCC_RC_FLOAT));
		o(0x1e202008 | a << 5 | (bt != VT_FLOAT) << 22);
		o(0x54000040 | !!inv);
	} else { MCC_TRACE("br\n");
		uint32_t ll = (bt == VT_PTR || bt == VT_LLONG);
		uint32_t a = intr(gv(MCC_RC_INT));
		o(0x34000040 | a | !!inv << 24 | ll << 31);
	}
	return gjmp(t);
}

static int arm64_iconst(uint64_t *val, SValue *sv) { MCC_TRACE("enter\n");
	if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
		{ MCC_TRACE("br\n"); return 0; }
	if (val) { MCC_TRACE("br\n");
		int t = sv->type.t;
		int bt = t & VT_BTYPE;
		*val = ((bt == VT_LLONG || bt == VT_PTR)
								? sv->c.i
								: (uint32_t)sv->c.i | (t & VT_UNSIGNED ? 0 : -(sv->c.i & 0x80000000)));
	}
	return 1;
}

static int arm64_gen_opic(int op, uint32_t l, int rev, uint64_t val,
													uint32_t x, uint32_t a) { MCC_TRACE("enter\n");
	if (op == '-' && !rev) { MCC_TRACE("br\n");
		val = -val;
		op = '+';
	}
	val = l ? val : (uint32_t)val;

	switch (op) { MCC_TRACE("br\n");
	case '+': {
		uint32_t s = l ? val >> 63 : val >> 31;
		val = s ? -val : val;
		val = l ? val : (uint32_t)val;

		if (!(val & ~(uint64_t)0xfff))
			{ MCC_TRACE("br\n"); o(0x11000000 | l << 31 | s << 30 | x | a << 5 | val << 10); }
		else if (!(val & ~(uint64_t)0xfff000))
			{ MCC_TRACE("br\n"); o(0x11400000 | l << 31 | s << 30 | x | a << 5 | val >> 12 << 10); }
		else { MCC_TRACE("br\n");
			arm64_movimm(30, val);
			o(0x0b1e0000 | l << 31 | s << 30 | x | a << 5);
		}
		return 1;
	}

	case '-':
		if (!val)
			{ MCC_TRACE("br\n"); o(0x4b0003e0 | l << 31 | x | a << 16); }
		else if (val == (l ? (uint64_t)-1 : (uint32_t)-1))
			{ MCC_TRACE("br\n"); o(0x2a2003e0 | l << 31 | x | a << 16); }
		else { MCC_TRACE("br\n");
			arm64_movimm(30, val);
			o(0x4b0003c0 | l << 31 | x | a << 16);
		}
		return 1;

	case '^':
		if (val == -1 || (val == 0xffffffff && !l)) { MCC_TRACE("br\n");
			o(0x2a2003e0 | l << 31 | x | a << 16);
			return 1;
		}
	case '&':
	case '|': {
		int e = arm64_encode_bimm64(l ? val : val | val << 32);
		if (e < 0)
			{ MCC_TRACE("br\n"); return 0; }
		o((op == '&'
					 ? 0x12000000
			 : op == '|'
					 ? 0x32000000
					 : 0x52000000) |
			l << 31 | x | a << 5 | (uint32_t)e << 10);
		return 1;
	}

	case TOK_SAR:
	case TOK_SHL:
	case TOK_SHR: {
		uint32_t n = 32 << l;
		val = val & (n - 1);
		if (rev)
			{ MCC_TRACE("br\n"); return 0; }
		if (!val) { MCC_TRACE("br\n");
			o(0x2a0003e0 | l << 31 | a << 16);
			return 1;
		} else if (op == TOK_SHL)
			{ MCC_TRACE("br\n"); o(0x53000000 | l << 31 | l << 22 | x | a << 5 |
				(n - val) << 16 | (n - 1 - val) << 10); }
		else
			{ MCC_TRACE("br\n"); o(0x13000000 | (op == TOK_SHR) << 30 | l << 31 | l << 22 |
				x | a << 5 | val << 16 | (n - 1) << 10); }
		return 1;
	}
	}
	return 0;
}

static void arm64_ubsan_trap_if_zero(uint32_t reg, uint32_t l) { MCC_TRACE("enter\n");
	if (!mcc_state->do_sanitize_undefined || nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	o(0x35000000 | l << 31 | (2u << 5) | reg);
	o(0xd4200000);
}

static int arm64_ubsan_on(void) { MCC_TRACE("enter\n");
	return mcc_state->do_sanitize_undefined && !nocode_wanted;
}

void gen_ubsan_nullptr(void) { MCC_TRACE("enter\n");
	if (!arm64_ubsan_on())
		{ MCC_TRACE("br\n"); return; }
	if ((vtop->r & VT_VALMASK) >= VT_CONST)
		{ MCC_TRACE("br\n"); return; }
	o(0xb5000000 | (2u << 5) | intr(vtop->r));
	o(0xd4200000);
}

static void arm64_ubsan_trap_cond(uint32_t skip_cond) { MCC_TRACE("enter\n");
	o(0x54000000 | (2u << 5) | skip_cond);
	o(0xd4200000);
}

static void arm64_ubsan_shift_check(uint32_t cnt, uint32_t l) { MCC_TRACE("enter\n");
	if (!arm64_ubsan_on())
		{ MCC_TRACE("br\n"); return; }
	uint32_t width = l ? 64 : 32;
	o(0x7100001f | l << 31 | (width << 10) | (cnt << 5));
	arm64_ubsan_trap_cond(3);
}

/* Inline ASan shadow probe for -fasan-shadow (mirrors x86_64 gen_asan_shadow_check):
   shadow = (int8)*(( addr>>3 ) + ASAN_OFF); trap if shadow!=0 && (addr&7)+sz-1 >= shadow.
   x16/x17 are the scratch pair; x15 additionally carries the full faulting address
   (x30 is a save-pair filler). All saved/restored so the probe is transparent to the
   pointer register, even when it IS x15/x16/x17. At the brk the runtime SIGTRAP handler
   reads x15=faulting address, w16=granule offset, w17=shadow byte (runtime/lib/mccasan.c). */
void gen_asan_shadow_check(int sz) { MCC_TRACE("enter\n");
	uint32_t a;
	if (!mcc_state->do_asan_shadow || nocode_wanted)
		{ MCC_TRACE("br\n"); return; }
	if ((vtop->r & VT_VALMASK) >= VT_CONST || sz <= 0 || sz > 8)
		{ MCC_TRACE("br\n"); return; }
	a = intr(vtop->r);
	o(0xa9bf47f0);                              /* stp   x16, x17, [sp, #-16]!   */
	o(0xa9bf7bef);                              /* stp   x15, x30, [sp, #-16]!   */
	o(0xaa0003f0 | (a << 16));                  /* mov   x16, Xaddr              */
	arm64_movimm(17, 0x7fff8000);               /* mov   x17, #ASAN_OFF          */
	o(0x8b500e31);                              /* add   x17, x17, x16, lsr #3   */
	o(0x39c00231);                              /* ldrsb w17, [x17]              */
	o(0x340000f1);                              /* cbz   w17, ok  (+7 insns)     */
	o(0xaa1003ef);                              /* mov   x15, x16  (fault addr)  */
	o(0x12000a10);                              /* and   w16, w16, #7            */
	o(0x11000210 | ((uint32_t)(sz - 1) << 10)); /* add   w16, w16, #(sz-1)       */
	o(0x6b11021f);                              /* cmp   w16, w17                */
	o(0x5400004b);                              /* b.lt  ok  (+2 insns)          */
	o(0xd4200000);                              /* brk   #0  (poison -> trap)    */
	o(0xa8c17bef);                              /* ok: ldp x15, x30, [sp], #16   */
	o(0xa8c147f0);                              /*     ldp x16, x17, [sp], #16   */
}

static void arm64_gen_opil(int op, uint32_t l) { MCC_TRACE("enter\n");
	uint32_t x, a, b;

	{
		uint64_t val;
		int rev = 1;

		if (arm64_iconst(0, &vtop[0])) { MCC_TRACE("br\n");
			vswap();
			rev = 0;
		}
		if (arm64_iconst(&val, &vtop[-1])) { MCC_TRACE("br\n");
			gv(MCC_RC_INT);
			a = intr(vtop[0].r);
			--vtop;
			x = get_reg(MCC_RC_INT);
			++vtop;
			if (arm64_gen_opic(op, l, rev, val, intr(x), a)) { MCC_TRACE("br\n");
				vtop[0].r = x;
				vswap();
				--vtop;
				return;
			}
		}
		if (!rev)
			{ MCC_TRACE("br\n"); vswap(); }
	}

	gv2(MCC_RC_INT, MCC_RC_INT);
	assert(vtop[-1].r < VT_CONST && vtop[0].r < VT_CONST);
	a = intr(vtop[-1].r);
	b = intr(vtop[0].r);
	uint32_t uns = (vtop[-1].type.t & VT_UNSIGNED) != 0;
	vtop -= 2;
	x = get_reg(MCC_RC_INT);
	++vtop;
	vtop[0].r = x;
	x = intr(x);

	switch (op) { MCC_TRACE("br\n");
	case '%':
		arm64_ubsan_trap_if_zero(b, l);
		o(0x1ac00c00 | l << 31 | 30 | a << 5 | b << 16);
		o(0x1b008000 | l << 31 | x | (uint32_t)30 << 5 |
			b << 16 | a << 10);
		break;
	case '&':
		o(0x0a000000 | l << 31 | x | a << 5 | b << 16);
		break;
	case '*':
		if (arm64_ubsan_on() && !uns) { MCC_TRACE("br\n");
			if (l) { MCC_TRACE("br\n");
				o(0x9b407c00 | 30u | (uint32_t)a << 5 | (uint32_t)b << 16);
				o(0x1b007c00 | 1u << 31 | x | (uint32_t)a << 5 | (uint32_t)b << 16);
				o(0xeb800000 | (uint32_t)x << 16 | 63u << 10 | 30u << 5 | 31u);
			} else { MCC_TRACE("br\n");
				o(0x9b207c00 | 30u | (uint32_t)a << 5 | (uint32_t)b << 16);
				o(0x1b007c00 | x | (uint32_t)a << 5 | (uint32_t)b << 16);
				o(0xeb200000 | 30u << 16 | 6u << 13 | 30u << 5 | 31u);
			}
			arm64_ubsan_trap_cond(0);
		} else
			{ MCC_TRACE("br\n"); o(0x1b007c00 | l << 31 | x | a << 5 | b << 16); }
		break;
	case '+':
		if (arm64_ubsan_on() && !uns) { MCC_TRACE("br\n");
			o(0x2b000000 | l << 31 | x | a << 5 | b << 16);
			arm64_ubsan_trap_cond(7);
		} else
			{ MCC_TRACE("br\n"); o(0x0b000000 | l << 31 | x | a << 5 | b << 16); }
		break;
	case '-':
		if (arm64_ubsan_on() && !uns) { MCC_TRACE("br\n");
			o(0x6b000000 | l << 31 | x | a << 5 | b << 16);
			arm64_ubsan_trap_cond(7);
		} else
			{ MCC_TRACE("br\n"); o(0x4b000000 | l << 31 | x | a << 5 | b << 16); }
		break;
	case '/':
	case TOK_PDIV:
		arm64_ubsan_trap_if_zero(b, l);
		o(0x1ac00c00 | l << 31 | x | a << 5 | b << 16);
		break;
	case '^':
		o(0x4a000000 | l << 31 | x | a << 5 | b << 16);
		break;
	case '|':
		o(0x2a000000 | l << 31 | x | a << 5 | b << 16);
		break;
	case TOK_EQ:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9f17e0 | x);
		break;
	case TOK_GE:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9fb7e0 | x);
		break;
	case TOK_GT:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9fd7e0 | x);
		break;
	case TOK_LE:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9fc7e0 | x);
		break;
	case TOK_LT:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9fa7e0 | x);
		break;
	case TOK_NE:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9f07e0 | x);
		break;
	case TOK_SAR:
		arm64_ubsan_shift_check(b, l);
		o(0x1ac02800 | l << 31 | x | a << 5 | b << 16);
		break;
	case TOK_SHL:
		arm64_ubsan_shift_check(b, l);
		o(0x1ac02000 | l << 31 | x | a << 5 | b << 16);
		break;
	case TOK_SHR:
		arm64_ubsan_shift_check(b, l);
		o(0x1ac02400 | l << 31 | x | a << 5 | b << 16);
		break;
	case TOK_UDIV:
		arm64_ubsan_trap_if_zero(b, l);
		o(0x1ac00800 | l << 31 | x | a << 5 | b << 16);
		break;
	case TOK_UGE:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9f37e0 | x);
		break;
	case TOK_UGT:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9f97e0 | x);
		break;
	case TOK_ULT:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9f27e0 | x);
		break;
	case TOK_ULE:
		o(0x6b00001f | l << 31 | a << 5 | b << 16);
		o(0x1a9f87e0 | x);
		break;
	case TOK_UMOD:
		arm64_ubsan_trap_if_zero(b, l);
		o(0x1ac00800 | l << 31 | 30 | a << 5 | b << 16);
		o(0x1b008000 | l << 31 | x | (uint32_t)30 << 5 |
			b << 16 | a << 10);
		break;
	default:
		assert(0);
	}
}

ST_FUNC void gen_opi(int op) { MCC_TRACE("enter\n");
	arm64_gen_opil(op, 0);
	arm64_vset_VT_CMP(op);
}

ST_FUNC void gen_opl(int op) { MCC_TRACE("enter\n");
	arm64_gen_opil(op, 1);
	arm64_vset_VT_CMP(op);
}

ST_FUNC void gen_opf(int op) { MCC_TRACE("enter\n");
	uint32_t x, a, b, dbl;
	int bt = vtop[0].type.t & VT_BTYPE;

	if (op == TOK_NEG) { MCC_TRACE("br\n");
		if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
			vpush_helper_func(TOK___negtf2);
			vrott(2);
			gfunc_call(1);
			vpushi(0);
			vtop->type.t = bt;
			vtop->r = REG_FRET;
		} else { MCC_TRACE("br\n");
			gv(MCC_RC_FLOAT);
			dbl = bt == VT_DOUBLE;
			a = fltr(vtop[0].r);
			o(0x1e214000 | dbl << 22 | a | a << 5);
		}
		return;
	}

	if (bt == VT_LDOUBLE) { MCC_TRACE("br\n");
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
		vtop->r = cond < 0 ? REG_FRET : REG_IRET;
		if (cond < 0)
			{ MCC_TRACE("br\n"); vtop->type = type; }
		else { MCC_TRACE("br\n");
			o(0x7100001f);
			o(0x1a9f07e0 | (uint32_t)cond << 12);
		}
		arm64_vset_VT_CMP(op);
		return;
	}

	dbl = bt != VT_FLOAT;
	gv2(MCC_RC_FLOAT, MCC_RC_FLOAT);
	assert(vtop[-1].r < VT_CONST && vtop[0].r < VT_CONST);
	a = fltr(vtop[-1].r);
	b = fltr(vtop[0].r);
	vtop -= 2;
	switch (op) { MCC_TRACE("br\n");
	case TOK_EQ:
	case TOK_NE:
	case TOK_LT:
	case TOK_GE:
	case TOK_LE:
	case TOK_GT:
		x = get_reg(MCC_RC_INT);
		++vtop;
		vtop[0].r = x;
		x = intr(x);
		break;
	default:
		x = get_reg(MCC_RC_FLOAT);
		++vtop;
		vtop[0].r = x;
		x = fltr(x);
		break;
	}

	switch (op) { MCC_TRACE("br\n");
	case '*':
		o(0x1e200800 | dbl << 22 | x | a << 5 | b << 16);
		break;
	case '+':
		o(0x1e202800 | dbl << 22 | x | a << 5 | b << 16);
		break;
	case '-':
		o(0x1e203800 | dbl << 22 | x | a << 5 | b << 16);
		break;
	case '/':
		o(0x1e201800 | dbl << 22 | x | a << 5 | b << 16);
		break;
	case TOK_EQ:
		o(0x1e202000 | dbl << 22 | a << 5 | b << 16);
		o(0x1a9f17e0 | x);
		break;
	case TOK_GE:
		o(0x1e202000 | dbl << 22 | a << 5 | b << 16);
		o(0x1a9fb7e0 | x);
		break;
	case TOK_GT:
		o(0x1e202000 | dbl << 22 | a << 5 | b << 16);
		o(0x1a9fd7e0 | x);
		break;
	case TOK_LE:
		o(0x1e202000 | dbl << 22 | a << 5 | b << 16);
		o(0x1a9f87e0 | x);
		break;
	case TOK_LT:
		o(0x1e202000 | dbl << 22 | a << 5 | b << 16);
		o(0x1a9f57e0 | x);
		break;
	case TOK_NE:
		o(0x1e202000 | dbl << 22 | a << 5 | b << 16);
		o(0x1a9f07e0 | x);
		break;
	default:
		assert(0);
	}
	arm64_vset_VT_CMP(op);
}

ST_FUNC void gen_cvt_sxtw(void) { MCC_TRACE("enter\n");
	uint32_t r = intr(gv(MCC_RC_INT));
	o(0x93407c00 | r | r << 5);
}

ST_FUNC void gen_cvt_csti(int t) { MCC_TRACE("enter\n");
	int r = intr(gv(MCC_RC_INT));
	o(0x13001c00 | ((t & VT_BTYPE) == VT_SHORT) << 13 | (uint32_t)!!(t & VT_UNSIGNED) << 30 | r | r << 5);
}

ST_FUNC void gen_cvt_itof(int t) { MCC_TRACE("enter\n");
	if (t == VT_LDOUBLE) { MCC_TRACE("br\n");
		int f = vtop->type.t;
		int func = (f & VT_BTYPE) == VT_LLONG
									 ? (f & VT_UNSIGNED ? TOK___floatunditf : TOK___floatditf)
									 : (f & VT_UNSIGNED ? TOK___floatunsitf : TOK___floatsitf);
		vpush_helper_func(func);
		vrott(2);
		gfunc_call(1);
		vpushi(0);
		vtop->type.t = t;
		vtop->r = REG_FRET;
		return;
	} else { MCC_TRACE("br\n");
		int d, n = intr(gv(MCC_RC_INT));
		int s = !(vtop->type.t & VT_UNSIGNED);
		uint32_t l = ((vtop->type.t & VT_BTYPE) == VT_LLONG);
		--vtop;
		d = get_reg(MCC_RC_FLOAT);
		++vtop;
		vtop[0].r = d;
		o(0x1e220000 | (uint32_t)!s << 16 |
			(uint32_t)(t != VT_FLOAT) << 22 | fltr(d) |
			l << 31 | n << 5);
	}
}

ST_FUNC void gen_cvt_ftoi(int t) { MCC_TRACE("enter\n");
	if ((vtop->type.t & VT_BTYPE) == VT_LDOUBLE) { MCC_TRACE("br\n");
		int func = (t & VT_BTYPE) == VT_LLONG
									 ? (t & VT_UNSIGNED ? TOK___fixunstfdi : TOK___fixtfdi)
									 : (t & VT_UNSIGNED ? TOK___fixunstfsi : TOK___fixtfsi);
		vpush_helper_func(func);
		vrott(2);
		gfunc_call(1);
		vpushi(0);
		vtop->type.t = t;
		vtop->r = REG_IRET;
		return;
	} else { MCC_TRACE("br\n");
		int d, n = fltr(gv(MCC_RC_FLOAT));
		uint32_t l = ((vtop->type.t & VT_BTYPE) != VT_FLOAT);
		--vtop;
		d = get_reg(MCC_RC_INT);
		++vtop;
		vtop[0].r = d;
		o(0x1e380000 |
			(uint32_t)!!(t & VT_UNSIGNED) << 16 |
			(uint32_t)((t & VT_BTYPE) == VT_LLONG) << 31 | intr(d) |
			l << 22 | n << 5);
	}
}

ST_FUNC void gen_cvt_ftof(int t) { MCC_TRACE("enter\n");
	int f = vtop[0].type.t & VT_BTYPE;
	assert(t == VT_FLOAT || t == VT_DOUBLE || t == VT_LDOUBLE);
	assert(f == VT_FLOAT || f == VT_DOUBLE || f == VT_LDOUBLE);
	if (t == f)
		{ MCC_TRACE("br\n"); return; }

	if (t == VT_LDOUBLE || f == VT_LDOUBLE) { MCC_TRACE("br\n");
		int func = (t == VT_LDOUBLE)
									 ? (f == VT_FLOAT ? TOK___extendsftf2 : TOK___extenddftf2)
									 : (t == VT_FLOAT ? TOK___trunctfsf2 : TOK___trunctfdf2);
		vpush_helper_func(func);
		vrott(2);
		gfunc_call(1);
		vpushi(0);
		vtop->type.t = t;
		vtop->r = REG_FRET;
	} else { MCC_TRACE("br\n");
		int x, a;
		gv(MCC_RC_FLOAT);
		assert(vtop[0].r < VT_CONST);
		a = fltr(vtop[0].r);
		x = a;
		if (f == VT_FLOAT)
			{ MCC_TRACE("br\n"); o(0x1e22c000 | x | a << 5); }
		else
			{ MCC_TRACE("br\n"); o(0x1e624000 | x | a << 5); }
	}
}

ST_FUNC void gen_increment_tcov(SValue *sv) { MCC_TRACE("enter\n");
	int r1, r2;

	vpushv(sv);
	vtop->r = r1 = get_reg(MCC_RC_INT);
	r2 = get_reg(MCC_RC_INT);
	arm64_sym(r1, sv->sym, 0);
	o(ARM64_LDR_X | ARM64_RN(intr(r1)) | intr(r2));
	o(0x91000400 | (intr(r2) << 5) | intr(r2));
	o(0xf9000000 | (intr(r1) << 5) | intr(r2));
	vpop();
}

ST_FUNC void ggoto(void) { MCC_TRACE("enter\n");
	arm64_gen_bl_or_b(1);
	--vtop;
}

ST_FUNC void gen_clear_cache(void) { MCC_TRACE("enter\n");
	uint32_t beg, end, dsz, isz, p, lab1, b1;
	gv2(MCC_RC_INT, MCC_RC_INT);
	vpushi(0);
	vtop->r = get_reg(MCC_RC_INT);
	vpushi(0);
	vtop->r = get_reg(MCC_RC_INT);
	vpushi(0);
	vtop->r = get_reg(MCC_RC_INT);
	beg = intr(vtop[-4].r);
	end = intr(vtop[-3].r);
	dsz = intr(vtop[-2].r);
	isz = intr(vtop[-1].r);
	p = intr(vtop[0].r);
	vtop -= 5;

	o(0xd53b0020 | isz);
	o(0x52800080 | p);
	o(0x53104c00 | dsz | isz << 5);
	o(0x1ac02000 | dsz | p << 5 | dsz << 16);
	o(0x12000c00 | isz | isz << 5);
	o(0x1ac02000 | isz | p << 5 | isz << 16);
	o(0x51000400 | p | dsz << 5);
	o(0x8a240004 | p | beg << 5 | p << 16);
	b1 = ind;
	o(ARM64_B);
	lab1 = ind;
	o(0xd50b7b20 | p);
	o(0x8b000000 | p | p << 5 | dsz << 16);
	write32le(cur_text_section->data + b1, ARM64_B | ((ind - b1) >> 2));
	o(0xeb00001f | p << 5 | end << 16);
	o(0x54ffffa3 | ((lab1 - ind) << 3 & 0xffffe0));
	o(0xd5033b9f);
	o(0x51000400 | p | isz << 5);
	o(0x8a240004 | p | beg << 5 | p << 16);
	b1 = ind;
	o(ARM64_B);
	lab1 = ind;
	o(0xd50b7520 | p);
	o(0x8b000000 | p | p << 5 | isz << 16);
	write32le(cur_text_section->data + b1, ARM64_B | ((ind - b1) >> 2));
	o(0xeb00001f | p << 5 | end << 16);
	o(0x54ffffa3 | ((lab1 - ind) << 3 & 0xffffe0));
	o(0xd5033b9f);
	o(0xd5033fdf);
}

ST_FUNC void gen_vla_sp_save(int addr) { MCC_TRACE("enter\n");
	uint32_t r = intr(get_reg(MCC_RC_INT));
	o(0x910003e0 | r);
	arm64_strx(3, r, 29, addr);
}

ST_FUNC void gen_vla_sp_restore(int addr) { MCC_TRACE("enter\n");
	uint32_t r = 30;
	arm64_ldrx(0, 3, r, 29, addr);
	o(0x9100001f | r << 5);
}

ST_FUNC void gen_vla_alloc(CType *type, int align) { MCC_TRACE("enter\n");
	uint32_t r;
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); vpushv(vtop); }
#endif
	r = intr(gv(MCC_RC_INT));
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check)
		{ MCC_TRACE("br\n"); o(0x91004000 | r | r << 5); }
	else
#endif
		o(0x91003c00 | r | r << 5);
	o(0x927cec00 | r | r << 5);
	o(0xcb2063ff | r << 16);
	{
		int a = align < 16 ? 16 : align;
		if (a > 16) { MCC_TRACE("br\n");
			int e = arm64_encode_bimm64((uint64_t)(-(int64_t)a));
			if (e < 0)
				{ MCC_TRACE("br\n"); mcc_error("unsupported over-alignment %d", a); }
			o(0x910003e0 | r);
			o(0x92000000 | r | (r << 5) | ((uint32_t)e << 10));
			o(0x9100001f | (r << 5));
		}
	}
	vpop();
#if MCC_CONFIG_DIAG_RT >= 2
	if (mcc_state->do_bounds_check) { MCC_TRACE("br\n");
		vpushi(0);
		vtop->r = MCC_TREG_R(0);
		o(0x910003e0 | vtop->r);
		vswap();
		vpush_helper_func(TOK___bound_new_region);
		vrott(3);
		gfunc_call(2);
		func_bound_add_epilog = 1;
	}
#endif
}

