#ifdef TARGET_DEFS_ONLY

#define NB_REGS 28

#define TREG_R(x) (x)
#define TREG_R30  19
#define TREG_F(x) (x + 20)

#define RC_INT (1 << 0)
#define RC_FLOAT (1 << 1)
#define RC_R(x) (1 << (2 + (x)))
#define RC_R30  (1 << 21)
#define RC_F(x) (1 << (22 + (x)))

#define RC_IRET (RC_R(0))
#define RC_FRET (RC_F(0))

#define REG_IRET (TREG_R(0))
#define REG_FRET (TREG_F(0))

#define PTR_SIZE 8

#define LDOUBLE_SIZE 16
#define LDOUBLE_ALIGN 16

#define MAX_ALIGN 16

#if !defined(MCC_TARGET_MACHO) && !defined(MCC_TARGET_PE)
#define CHAR_IS_UNSIGNED
#endif

#define PROMOTE_RET
#else
#define USING_GLOBALS
#include "mcc.h"
#include <assert.h>

ST_DATA const char * const target_machine_defs =
    "__aarch64__\0"
#if defined(MCC_TARGET_MACHO)
    "__arm64__\0"
#endif
    "__AARCH64EL__\0"
    ;

ST_DATA const int reg_classes[NB_REGS] = {
  RC_INT | RC_R(0),
  RC_INT | RC_R(1),
  RC_INT | RC_R(2),
  RC_INT | RC_R(3),
  RC_INT | RC_R(4),
  RC_INT | RC_R(5),
  RC_INT | RC_R(6),
  RC_INT | RC_R(7),
  RC_INT | RC_R(8),
  RC_INT | RC_R(9),
  RC_INT | RC_R(10),
  RC_INT | RC_R(11),
  RC_INT | RC_R(12),
  RC_INT | RC_R(13),
  RC_INT | RC_R(14),
  RC_INT | RC_R(15),
  RC_INT | RC_R(16),
  RC_INT | RC_R(17),
#ifdef MCC_TARGET_PE
  RC_R(18),
#else
  RC_INT | RC_R(18),
#endif
  RC_R30,
  RC_FLOAT | RC_F(0),
  RC_FLOAT | RC_F(1),
  RC_FLOAT | RC_F(2),
  RC_FLOAT | RC_F(3),
  RC_FLOAT | RC_F(4),
  RC_FLOAT | RC_F(5),
  RC_FLOAT | RC_F(6),
  RC_FLOAT | RC_F(7)
};

#if defined(CONFIG_MCC_BCHECK)
static addr_t func_bound_offset;
static unsigned long func_bound_ind;
ST_DATA int func_bound_add_epilog;
#endif

#define IS_FREG(x) ((x) >= TREG_F(0))

static uint32_t intr(int r)
{
    assert(TREG_R(0) <= r && r <= TREG_R30);
    return r < TREG_R30 ? r : 30;
}

static uint32_t fltr(int r)
{
    assert(TREG_F(0) <= r && r <= TREG_F(7));
    return r - TREG_F(0);
}

ST_FUNC void o(unsigned int c)
{
    int ind1 = ind + 4;
    if (nocode_wanted)
        return;
    if (ind1 > cur_text_section->data_allocated)
        section_realloc(cur_text_section, ind1);
    write32le(cur_text_section->data + ind, c);
    ind = ind1;
}

static int arm64_encode_bimm64(uint64_t x)
{
    int neg = x & 1;
    int rep, pos, len;

    if (neg)
        x = ~x;
    if (!x)
        return -1;

    if (x >> 2 == (x & (((uint64_t)1 << (64 - 2)) - 1)))
        rep = 2, x &= ((uint64_t)1 << 2) - 1;
    else if (x >> 4 == (x & (((uint64_t)1 << (64 - 4)) - 1)))
        rep = 4, x &= ((uint64_t)1 <<  4) - 1;
    else if (x >> 8 == (x & (((uint64_t)1 << (64 - 8)) - 1)))
        rep = 8, x &= ((uint64_t)1 <<  8) - 1;
    else if (x >> 16 == (x & (((uint64_t)1 << (64 - 16)) - 1)))
        rep = 16, x &= ((uint64_t)1 << 16) - 1;
    else if (x >> 32 == (x & (((uint64_t)1 << (64 - 32)) - 1)))
        rep = 32, x &= ((uint64_t)1 << 32) - 1;
    else
        rep = 64;

    pos = 0;
    if (!(x & (((uint64_t)1 << 32) - 1))) x >>= 32, pos += 32;
    if (!(x & (((uint64_t)1 << 16) - 1))) x >>= 16, pos += 16;
    if (!(x & (((uint64_t)1 <<  8) - 1))) x >>= 8, pos += 8;
    if (!(x & (((uint64_t)1 <<  4) - 1))) x >>= 4, pos += 4;
    if (!(x & (((uint64_t)1 <<  2) - 1))) x >>= 2, pos += 2;
    if (!(x & (((uint64_t)1 <<  1) - 1))) x >>= 1, pos += 1;

    len = 0;
    if (!(~x & (((uint64_t)1 << 32) - 1))) x >>= 32, len += 32;
    if (!(~x & (((uint64_t)1 << 16) - 1))) x >>= 16, len += 16;
    if (!(~x & (((uint64_t)1 << 8) - 1))) x >>= 8, len += 8;
    if (!(~x & (((uint64_t)1 << 4) - 1))) x >>= 4, len += 4;
    if (!(~x & (((uint64_t)1 << 2) - 1))) x >>= 2, len += 2;
    if (!(~x & (((uint64_t)1 << 1) - 1))) x >>= 1, len += 1;

    if (x)
        return -1;
    if (neg) {
        pos = (pos + len) & (rep - 1);
        len = rep - len;
    }
    return ((0x1000 & rep << 6) | (((rep - 1) ^ 31) << 1 & 63) |
            ((rep - pos) & (rep - 1)) << 6 | (len - 1));
}

static uint32_t arm64_movi(int r, uint64_t x)
{
    uint64_t m = 0xffff;
    int e;
    if (!(x & ~m))
        return ARM64_MOVZ | r | x << 5;
    if (!(x & ~(m << 16)))
        return (ARM64_MOVZ | ARM64_HW(1) | r | x >> 11);
    if (!(x & ~(m << 32)))
        return (ARM64_MOVZ64 | ARM64_HW(2) | r | x >> 27);
    if (!(x & ~(m << 48)))
        return (ARM64_MOVZ64 | ARM64_HW(3) | r | x >> 43);
    if ((x & ~m) == m << 16)
        return (ARM64_MOVN | r |
                (~x << 5 & 0x1fffe0));
    if ((x & ~(m << 16)) == m)
        return (ARM64_MOVN | ARM64_HW(1) | r |
                (~x >> 11 & 0x1fffe0));
    if (!~(x | m))
        return (ARM64_MOVN64 | r |
                (~x << 5 & 0x1fffe0));
    if (!~(x | m << 16))
        return (ARM64_MOVN64 | ARM64_HW(1) | r |
                (~x >> 11 & 0x1fffe0));
    if (!~(x | m << 32))
        return (ARM64_MOVN64 | ARM64_HW(2) | r |
                (~x >> 27 & 0x1fffe0));
    if (!~(x | m << 48))
        return (ARM64_MOVN64 | ARM64_HW(3) | r |
                (~x >> 43 & 0x1fffe0));
    if (!(x >> 32) && (e = arm64_encode_bimm64(x | x << 32)) >= 0)
        return (ARM64_ORR_IMM | r | (uint32_t)e << 10);
    if ((e = arm64_encode_bimm64(x)) >= 0)
        return (ARM64_ORR_IMM | ARM64_SF(1) | r | (uint32_t)e << 10);
    return 0;
}

static void arm64_movimm(int r, uint64_t x)
{
    uint32_t i;
    if ((i = arm64_movi(r, x)))
        o(i);
    else {
        int z = 0, m = 0;
        uint32_t mov1 = ARM64_MOVZ64;
        uint64_t x1 = x;
        for (i = 0; i < 64; i += 16) {
            z += !(x >> i & 0xffff);
            m += !(~x >> i & 0xffff);
        }
        if (m > z) {
            x1 = ~x;
            mov1 = ARM64_MOVN64;
        }
        for (i = 0; i < 64; i += 16)
            if (x1 >> i & 0xffff) {
                o(mov1 | r | (x1 >> i & 0xffff) << 5 | i << 17);
                break;
            }
        for (i += 16; i < 64; i += 16)
            if (x1 >> i & 0xffff)
                o(ARM64_MOVK | ARM64_SF(1) | r | (x >> i & 0xffff) << 5 | i << 17);
    }
}

ST_FUNC void gsym_addr(int t_, int a_)
{
    uint32_t t = t_;
    uint32_t a = a_;
    while (t) {
        unsigned char *ptr = cur_text_section->data + t;
        uint32_t next = read32le(ptr);
        if (a - t + 0x8000000 >= 0x10000000)
            mcc_error("branch out of range");
        write32le(ptr, (a - t == 4 ? ARM64_NOP :
                        ARM64_B | ((a - t) >> 2 & 0x3ffffff)));
        t = next;
    }
}

static int arm64_type_size(int t)
{
    switch (t & VT_BTYPE) {
    case VT_BYTE: return 0;
    case VT_SHORT: return 1;
    case VT_INT: return 2;
    case VT_LLONG: return 3;
    case VT_PTR: return 3;
    case VT_FUNC: return 3;
    case VT_STRUCT: return 3;
    case VT_FLOAT: return 2;
    case VT_DOUBLE: return 3;
    case VT_LDOUBLE: return 4;
    case VT_BOOL: return 0;
    }
    assert(0);
    return 0;
}

static void arm64_spoff(int reg, uint64_t off)
{
    uint32_t sub = off >> 63;
    if (sub)
        off = -off;
    if (off < 4096)
        o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RN(31) | ARM64_RD(reg) | ARM64_IMM12(off));
    else {
        arm64_movimm(30, off);
        o(ARM64_ADD_REG | ARM64_SF(1) | ARM64_RM(30) | ARM64_RN(31) | ARM64_RD(reg) | (sub << 30));
    }
}

static uint64_t arm64_check_offset(int invert, int sz_, uint64_t off)
{
    uint32_t sz = sz_;
    uint64_t scaled_mask = 0xffful << sz;

    if (!(off & ~scaled_mask) ||
        (off < 256 || -off <= 256))
        return invert ? off : 0ul;
    else if (off & scaled_mask)
        return invert ? off & scaled_mask : off & ~scaled_mask;
    else if (off & 0x1fful)
        return invert ? off & 0x1fful : off & ~0x1fful;
    else
        return invert ? 0ul : off;
}

static void arm64_ldrx(int sg, int sz_, int dst, int bas, uint64_t off)
{
    uint32_t sz = sz_;
    uint64_t scaled_mask = 0xffful << sz;
    if (sz >= 2)
        sg = 0;
    if (!(off & ~scaled_mask))
        o(ARM64_LDR_B | dst | bas << 5 | off << (10 - sz) |
          (uint32_t)!!sg << 23 | sz << 30);
    else if (off < 256 || -off <= 256)
        o(ARM64_LDUR_B | dst | bas << 5 | (off & 511) << 12 |
          (uint32_t)!!sg << 23 | sz << 30);
    else {
        arm64_movimm(30, off);
        o(ARM64_LDR_B_REG | dst | bas << 5 | (uint32_t)30 << 16 |
          (uint32_t)(!!sg + 1) << 22 | sz << 30);
    }
}

static void arm64_ldrv(int sz_, int dst, int bas, uint64_t off)
{
    uint32_t sz = sz_;
    uint64_t scaled_mask = 0xffful << sz;

    if (!(off & ~scaled_mask))
        o(ARM64_LDR_SCALAR | dst | bas << 5 | off << (10 - sz) |
          (sz & 4) << 21 | (sz & 3) << 30);
    else if (off < 256 || -off <= 256)
        o(ARM64_LDUR_Q_SIMD | dst | bas << 5 | (off & 511) << 12 |
          (sz & 4) << 21 | (sz & 3) << 30);
    else {
        arm64_movimm(30, off);
        o(ARM64_LDR_Q_REG | dst | bas << 5 | (uint32_t)30 << 16 |
          sz << 30 | (sz & 4) << 21);
    }
}

static void arm64_ldrs(int reg_, int size)
{
    uint32_t reg = reg_;
    switch (size) {
    default: assert(0); break;
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
        o(0x53087c00 | (reg+1) | (reg+1) << 5);
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 12:
        arm64_ldrx(0, 2, reg + 1, reg, 8);
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 13:
        arm64_ldrx(0, 3, reg + 1, reg, 5);
        o(0xd358fc00 | (reg+1) | (reg+1) << 5);
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 14:
        arm64_ldrx(0, 3, reg + 1, reg, 6);
        o(0xd350fc00 | (reg+1) | (reg+1) << 5);
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 15:
        arm64_ldrx(0, 3, reg + 1, reg, 7);
        o(0xd348fc00 | (reg+1) | (reg+1) << 5);
        arm64_ldrx(0, 3, reg, reg, 0);
        break;
    case 16:
        o(0xa9400000 | reg | (reg+1) << 10 | reg << 5);
        break;
    }
}

static void arm64_strx(int sz_, int dst, int bas, uint64_t off)
{
    uint32_t sz = sz_;
    uint64_t scaled_mask = 0xffful << sz;

    if (!(off & ~scaled_mask))
        o(0x39000000 | dst | bas << 5 | off << (10 - sz) | sz << 30);
    else if (off < 256 || -off <= 256)
        o(0x38000000 | dst | bas << 5 | (off & 511) << 12 | sz << 30);
    else {
        arm64_movimm(30, off);
        o(0x38206800 | dst | bas << 5 | (uint32_t)30 << 16 | sz << 30);
    }
}

static void arm64_strv(int sz_, int dst, int bas, uint64_t off)
{
    uint32_t sz = sz_;
    uint64_t scaled_mask = 0xffful << sz;

    if (!(off & ~scaled_mask))
        o(0x3d000000 | dst | bas << 5 | off << (10 - sz) |
          (sz & 4) << 21 | (sz & 3) << 30);
    else if (off < 256 || -off <= 256)
        o(0x3c000000 | dst | bas << 5 | (off & 511) << 12 |
          (sz & 4) << 21 | (sz & 3) << 30);
    else {
        arm64_movimm(30, off);
        o(0x3c206800 | dst | bas << 5 | (uint32_t)30 << 16 |
          sz << 30 | (sz & 4) << 21);
    }
}

static void arm64_sym(int r, Sym *sym, unsigned long addend)
{
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
    if (addend) {
	if (addend & 0xffful)
           o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RN(r) | r |
             (addend & 0xfff) << 10);
        if (addend > 0xffful) {
	    if (addend & 0xfff000ul)
                o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
                  ARM64_RN(r) | r | ((addend >> 12) & 0xfff) << 10);
            if (addend > 0xfffffful) {
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

ST_FUNC void load(int r, SValue *sv)
{
    int svtt = sv->type.t;
    int svr = sv->r & ~(VT_BOUNDED | VT_NONCONST);
    int svrv = svr & VT_VALMASK;
    uint64_t svcul = sv->c.i;
    uint64_t svcoff = (uint64_t)(int64_t)(int32_t)sv->c.i;

    if (svr == (VT_LOCAL | VT_LVAL)) {
        if (IS_FREG(r))
            arm64_ldrv(arm64_type_size(svtt), fltr(r), 29, svcoff);
        else
            arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
                       intr(r), 29, svcoff);
        return;
    }

    if (svr == (VT_CONST | VT_LVAL)) {
	uint64_t i = sv->c.i;

	if (sv->sym)
            arm64_sym(30, sv->sym,
	              arm64_check_offset(0, arm64_type_size(svtt), i));
	else
	    arm64_movimm (30, i), i = 0;
        if (IS_FREG(r))
            arm64_ldrv(arm64_type_size(svtt), fltr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), i));
        else
            arm64_ldrx(!(svtt&VT_UNSIGNED), arm64_type_size(svtt), intr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), i));
        return;
    }

    if ((svr & ~VT_VALMASK) == VT_LVAL && svrv < VT_CONST) {
        if ((svtt & VT_BTYPE) != VT_VOID) {
            if (IS_FREG(r))
                arm64_ldrv(arm64_type_size(svtt), fltr(r), intr(svrv), 0);
            else
                arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
                           intr(r), intr(svrv), 0);
        }
        return;
    }

    if (svr == (VT_CONST | VT_LVAL | VT_SYM)) {
        if (sv->sym->type.t & VT_TLS) {
            o(0xd53bd05e);
            greloca(cur_text_section, sv->sym, ind,
                    R_AARCH64_TLSLE_ADD_TPREL_HI12, 0);
            o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
              ARM64_RN(30) | ARM64_RD(30));
            greloca(cur_text_section, sv->sym, ind,
                    R_AARCH64_TLSLE_ADD_TPREL_LO12, 0);
            o(ARM64_ADD_IMM | ARM64_SF(1) |
              ARM64_RN(30) | ARM64_RD(30));
            if (IS_FREG(r))
                arm64_ldrv(arm64_type_size(svtt), fltr(r), 30, svcoff);
            else
                arm64_ldrx(!(svtt&VT_UNSIGNED), arm64_type_size(svtt),
                           intr(r), 30, svcoff);
            return;
        }
        arm64_sym(30, sv->sym,
		  arm64_check_offset(0, arm64_type_size(svtt), svcoff));
        if (IS_FREG(r))
            arm64_ldrv(arm64_type_size(svtt), fltr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), svcoff));
        else
            arm64_ldrx(!(svtt&VT_UNSIGNED), arm64_type_size(svtt), intr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), svcoff));
        return;
    }

    if (svr == (VT_CONST | VT_SYM)) {
        if (sv->sym->type.t & VT_TLS) {
            /* &thread_local (Local Exec): tpidr_el0 + sym@tprel.  The value
               load/store paths build this same tp-relative address before the
               ldr/str; a bare &address must materialise it too, not the section
               image address (only the per-thread init template). */
            o(0xd53bd05e);                          /* mrs x30, tpidr_el0   */
            greloca(cur_text_section, sv->sym, ind,
                    R_AARCH64_TLSLE_ADD_TPREL_HI12, svcul);
            o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
              ARM64_RN(30) | ARM64_RD(30));
            greloca(cur_text_section, sv->sym, ind,
                    R_AARCH64_TLSLE_ADD_TPREL_LO12, svcul);
            o(ARM64_ADD_IMM | ARM64_SF(1) |
              ARM64_RN(30) | ARM64_RD(intr(r)));
            return;
        }
        arm64_sym(intr(r), sv->sym, svcul);
        return;
    }

    if (svr == VT_CONST) {
        if ((svtt & VT_BTYPE) != VT_VOID)
            arm64_movimm(intr(r), arm64_type_size(svtt) == 3 ?
                         sv->c.i : (uint32_t)svcul);
        return;
    }

    if (svr < VT_CONST) {
        if (IS_FREG(r) && IS_FREG(svr))
            if (svtt == VT_LDOUBLE)
                o(ARM64_MOV_V16B | fltr(r) | fltr(svr) * 0x10020);
            else
                o(ARM64_FMOV_SCALAR | fltr(r) | fltr(svr) << 5);
        else if (!IS_FREG(r) && !IS_FREG(svr))
            o(ARM64_MOV_REG | ARM64_SF(1) | intr(r) | intr(svr) << 16);
        else
            assert(0);
      return;
    }

    if (svr == VT_LOCAL) {
        if (-svcoff < 0x1000)
            o(0xd10003a0 | intr(r) | -svcoff << 10);
        else {
            arm64_movimm(30, -svcoff);
            o(0xcb0003a0 | intr(r) | (uint32_t)30 << 16);
        }
        return;
    }

    if (svr == VT_JMP || svr == VT_JMPI) {
        int t = (svr == VT_JMPI);
        arm64_movimm(intr(r), t);
        o(ARM64_B | 2);
        gsym(svcul);
        arm64_movimm(intr(r), t ^ 1);
        return;
    }

    if (svr == (VT_LLOCAL | VT_LVAL)) {
        arm64_ldrx(0, 3, 30, 29, svcoff);
        if (IS_FREG(r))
            arm64_ldrv(arm64_type_size(svtt), fltr(r), 30, 0);
        else
            arm64_ldrx(!(svtt & VT_UNSIGNED), arm64_type_size(svtt),
                       intr(r), 30, 0);
        return;
    }

    if (svr == VT_CMP) {
        arm64_load_cmp(r, sv);
        return;
    }

    printf("load(%x, (%x, %x, %lx))\n", r, svtt, sv->r, (long)svcul);
    assert(0);
}

ST_FUNC void store(int r, SValue *sv)
{
    int svtt = sv->type.t;
    int svr = sv->r & ~VT_BOUNDED;
    int svrv = svr & VT_VALMASK;
    uint64_t svcoff = (uint64_t)(int64_t)(int32_t)sv->c.i;

    if (svr == (VT_LOCAL | VT_LVAL)) {
        if (IS_FREG(r))
            arm64_strv(arm64_type_size(svtt), fltr(r), 29, svcoff);
        else
            arm64_strx(arm64_type_size(svtt), intr(r), 29, svcoff);
        return;
    }

    if (svr == (VT_CONST | VT_LVAL)) {
	uint64_t i = sv->c.i;

	if (sv->sym && (sv->sym->type.t & VT_TLS)) {
            o(0xd53bd05e);
            greloca(cur_text_section, sv->sym, ind,
                    R_AARCH64_TLSLE_ADD_TPREL_HI12, 0);
            o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
              ARM64_RN(30) | ARM64_RD(30));
            greloca(cur_text_section, sv->sym, ind,
                    R_AARCH64_TLSLE_ADD_TPREL_LO12, 0);
            o(ARM64_ADD_IMM | ARM64_SF(1) |
              ARM64_RN(30) | ARM64_RD(30));
            if (IS_FREG(r))
                arm64_strv(arm64_type_size(svtt), fltr(r), 30, i);
            else
                arm64_strx(arm64_type_size(svtt), intr(r), 30, i);
            return;
        }
	if (sv->sym)
            arm64_sym(30, sv->sym,
		      arm64_check_offset(0, arm64_type_size(svtt), i));
	else
	    arm64_movimm (30, i), i = 0;
        if (IS_FREG(r))
            arm64_strv(arm64_type_size(svtt), fltr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), i));
        else
            arm64_strx(arm64_type_size(svtt), intr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), i));
        return;
    }

    if ((svr & ~VT_VALMASK) == VT_LVAL && svrv < VT_CONST) {
        if (IS_FREG(r))
            arm64_strv(arm64_type_size(svtt), fltr(r), intr(svrv), 0);
        else
            arm64_strx(arm64_type_size(svtt), intr(r), intr(svrv), 0);
        return;
    }

    if (svr == (VT_CONST | VT_LVAL | VT_SYM)) {
        if (sv->sym->type.t & VT_TLS) {
            o(0xd53bd05e);
            greloca(cur_text_section, sv->sym, ind,
                    R_AARCH64_TLSLE_ADD_TPREL_HI12, 0);
            o(ARM64_ADD_IMM | ARM64_SF(1) | ARM64_SH(1) |
              ARM64_RN(30) | ARM64_RD(30));
            greloca(cur_text_section, sv->sym, ind,
                    R_AARCH64_TLSLE_ADD_TPREL_LO12, 0);
            o(ARM64_ADD_IMM | ARM64_SF(1) |
              ARM64_RN(30) | ARM64_RD(30));
            if (IS_FREG(r))
                arm64_strv(arm64_type_size(svtt), fltr(r), 30, svcoff);
            else
                arm64_strx(arm64_type_size(svtt), intr(r), 30, svcoff);
            return;
        }
        arm64_sym(30, sv->sym,
		  arm64_check_offset(0, arm64_type_size(svtt), svcoff));
        if (IS_FREG(r))
            arm64_strv(arm64_type_size(svtt), fltr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), svcoff));
        else
            arm64_strx(arm64_type_size(svtt), intr(r), 30,
		       arm64_check_offset(1, arm64_type_size(svtt), svcoff));
        return;
    }

    printf("store(%x, (%x, %x, %lx))\n", r, svtt, sv->r, (long)svcoff);
    assert(0);
}

static void arm64_gen_bl_or_b(int b)
{
    if ((vtop->r & (VT_VALMASK | VT_LVAL)) == VT_CONST && (vtop->r & VT_SYM)) {
	greloca(cur_text_section, vtop->sym, ind,
                b ? R_AARCH64_JUMP26 :  R_AARCH64_CALL26, 0);
	o(b ? ARM64_B : ARM64_BL);
    }
    else {
#ifdef CONFIG_MCC_BCHECK
        vtop->r &= ~VT_MUSTBOUND;
#endif
        o((b ? ARM64_BR : ARM64_BLR) | intr(gv(RC_R30)) << 5);
    }
}

#if defined(CONFIG_MCC_BCHECK)

static void gen_bounds_call(int v)
{
    Sym *sym = external_helper_sym(v);

    greloca(cur_text_section, sym, ind, R_AARCH64_CALL26, 0);
    o(ARM64_BL);
}

static void gen_bounds_prolog(void)
{
    func_bound_offset = lbounds_section->data_offset;
    func_bound_ind = ind;
    func_bound_add_epilog = 0;
    o(ARM64_NOP);
    o(ARM64_NOP);
    o(ARM64_NOP);
    o(ARM64_NOP);
}

static void gen_bounds_epilog(void)
{
    addr_t saved_ind;
    addr_t *bounds_ptr;
    Sym *sym_data;
    int offset_modified = func_bound_offset != lbounds_section->data_offset;

    if (!offset_modified && !func_bound_add_epilog)
        return;

    bounds_ptr = section_ptr_add(lbounds_section, sizeof(addr_t));
    *bounds_ptr = 0;

    sym_data = get_sym_ref(&char_pointer_type, lbounds_section,
                           func_bound_offset, PTR_SIZE);

    if (offset_modified) {
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

static int arm64_hfa_aux(CType *type, int *fsize, int num)
{
    if (is_float(type->t)) {
        int a, n = type_size(type, &a);
        if (num >= 4 || (*fsize && *fsize != n))
            return -1;
        *fsize = n;
        return num + 1;
    }
    else if ((type->t & VT_BTYPE) == VT_STRUCT) {
        Sym *field;
        if (!IS_UNION(type->t)) {
            int num0 = num;
            for (field = type->ref->next; field; field = field->next) {
                if (field->c != (num - num0) * *fsize)
                    return -1;
                num = arm64_hfa_aux(&field->type, fsize, num);
                if (num == -1)
                    return -1;
            }
            if (type->ref->c != (num - num0) * *fsize)
                return -1;
            return num;
        }
        else {
            int num0 = num;
            for (field = type->ref->next; field; field = field->next) {
                int num1 = arm64_hfa_aux(&field->type, fsize, num0);
                if (num1 == -1)
                    return -1;
                num = num1 < num ? num : num1;
            }
            if (type->ref->c != (num - num0) * *fsize)
                return -1;
            return num;
        }
    }
    else if (type->t & VT_ARRAY) {
        int num1;
        if (!type->ref->c)
            return num;
        num1 = arm64_hfa_aux(&type->ref->type, fsize, num);
        if (num1 == -1 || (num1 != num && type->ref->c > 4))
            return -1;
        num1 = num + type->ref->c * (num1 - num);
        if (num1 > 4)
            return -1;
        return num1;
    }
    return -1;
}

static int arm64_hfa(CType *type, unsigned *fsize)
{
    if ((type->t & VT_BTYPE) == VT_STRUCT) {
        int sz = 0;
        int n = arm64_hfa_aux(type, &sz, 0);
        if (0 < n && n <= 4) {
            if (fsize)
                *fsize = sz;
            return n;
        }
    }
    return 0;
}

static unsigned long arm64_pcs_aux(int variadic, int n, CType **type, unsigned long *a)
{
    int nx = 0;
    int nv = 0;
    unsigned long ns = 32;
    for (int i = 0; i < n; i++) {
        int hfa = arm64_hfa(type[i], 0);
        int size, align, bt;

        bt = type[i]->t & VT_BTYPE;
        if (bt == VT_PTR || bt == VT_FUNC)
            size = align = 8;
        else
            size = type_size(type[i], &align);

#if defined(MCC_TARGET_MACHO)
        if (variadic && i == variadic) {
            nx = 8;
            nv = 8;
	}

#elif defined(MCC_TARGET_PE)
        if (variadic && i >= variadic) {
            hfa = 0;
            if (is_float(bt))
                bt = VT_INT, size = align = 8;
        }
#endif
        if (hfa)
            ;
        else if (size > 16) {
            if (nx < 8)
                a[i] = nx++ << 1 | 1;
            else {
                ns = (ns + 7) & ~7;
                a[i] = ns | 1;
                ns += 8;
            }
            continue;
        }
        else if (bt == VT_STRUCT)
            size = (size + 7) & ~7;

        if (is_float(bt) && nv < 8) {
            a[i] = 16 + (nv++ << 1);
            continue;
        }

        if (hfa && nv + hfa <= 8) {
            a[i] = 16 + (nv << 1);
            nv += hfa;
            continue;
        }

        if (hfa) {
            nv = 8;
            size = (size + 7) & ~7;
        }

        if (hfa || bt == VT_LDOUBLE) {
            ns = (ns + 7) & ~7;
            ns = (ns + align - 1) & -align;
        }

        if (bt == VT_FLOAT)
            size = 8;

        if (hfa || is_float(bt)) {
            a[i] = ns;
            ns += size;
            continue;
        }

        if (bt != VT_STRUCT && size <= 8 && nx < 8) {
            a[i] = nx++ << 1;
            continue;
        }

        if (align == 16)
            nx = (nx + 1) & ~1;

        if (bt != VT_STRUCT && size == 16 && nx < 7) {
            a[i] = nx << 1;
            nx += 2;
            continue;
        }

        if (bt == VT_STRUCT && size <= (8 - nx) * 8) {
            a[i] = nx << 1;
            nx += (size + 7) >> 3;
            continue;
        }

        nx = 8;

        ns = (ns + 7) & ~7;
        ns = (ns + align - 1) & -align;

        if (bt == VT_STRUCT) {
            a[i] = ns;
            ns += size;
            continue;
        }

        if (size < 8)
            size = 8;

        a[i] = ns;
        ns += size;
    }

    return ns - 32;
}

static unsigned long arm64_pcs(int variadic, int n, CType **type, unsigned long *a)
{
    unsigned long stack;

    if ((type[0]->t & VT_BTYPE) == VT_VOID)
        a[0] = -1;
    else {
        arm64_pcs_aux(0, 1, type, a);
        assert(a[0] == 0 || a[0] == 1 || a[0] == 16);
    }

    stack = arm64_pcs_aux(variadic, n - 1, type + 1, a + 1);

    if (0) {
        for (int i = 0; i < n; i++) {
            if (!i)
                printf("arm64_pcs return: ");
            else
                printf("arm64_pcs arg %d: ", i);
            if (a[i] == (unsigned long)-1)
                printf("void\n");
            else if (a[i] == 1 && !i)
                printf("X8 pointer\n");
            else if (a[i] < 16)
                printf("X%lu%s\n", a[i] / 2, a[i] & 1 ? " pointer" : "");
            else if (a[i] < 32)
                printf("V%lu\n", a[i] / 2 - 8);
            else
                printf("stack %lu%s\n",
                       (a[i] - 32) & ~1, a[i] & 1 ? " pointer" : "");
        }
    }

    return stack;
}

static int n_func_args(CType *type)
{
    int n_args = 0;
    Sym *arg;

    for (arg = type->ref->next; arg; arg = arg->next)
        n_args++;
    return n_args;
}

static void arm64_sub_sp(uint64_t diff)
{
    if (!diff)
        return;
#ifdef MCC_TARGET_PE
    if (diff >= 4096) {
        Sym *sym = external_helper_sym(TOK___chkstk);

        arm64_movimm(15, diff >> 4);
        greloca(cur_text_section, sym, ind, R_AARCH64_CALL26, 0);
        o(ARM64_BL);
        o(0xcb2f73ff);
        return;
    }
#endif
    if (!(diff >> 24)) {
        if (diff & 0xffful)
            o(ARM64_SUB_IMM | ARM64_SF(1) | 0           | ARM64_RN(31) | ARM64_RD(31) | ARM64_IMM12(diff & 0xfff));
        if (diff >> 12)
            o(ARM64_SUB_IMM | ARM64_SF(1) | ARM64_SH(1) | ARM64_RN(31) | ARM64_RD(31) | ARM64_IMM12((diff >> 12) & 0xfff));
    } else {
        arm64_movimm(16, diff);
        o(0xCB3063FFU);
    }
}

static int gv_addr(int r)
{
    gaddrof();
    vtop->type.t = VT_PTR;
    return gv(r);
}

ST_FUNC void gfunc_call(int nb_args)
{
    CType *return_type;
    CType **t;
    unsigned long *a, *a1;
    unsigned long stack;
    int func_type = vtop[-nb_args].type.ref->f.func_type;
    int variadic = (func_type == FUNC_ELLIPSIS);
    int old_style = (func_type == FUNC_OLD);
    int var_nb_arg = variadic ? n_func_args(&vtop[-nb_args].type) : 0;

    save_regs(nb_args + 1);

#ifdef CONFIG_MCC_BCHECK
    if (mcc_state->do_bounds_check)
        gbound_args(nb_args);
#endif

    return_type = &vtop[-nb_args].type.ref->type;
    if ((return_type->t & VT_BTYPE) == VT_STRUCT)
        --nb_args;

    t = mcc_malloc((nb_args + 1) * sizeof(*t));
    a = mcc_malloc((nb_args + 1) * sizeof(*a));
    a1 = mcc_malloc((nb_args + 1) * sizeof(*a1));

    t[0] = return_type;
    for (int i = 0; i < nb_args; i++)
        t[nb_args - i] = &vtop[-i].type;

    stack = arm64_pcs(
#ifdef MCC_TARGET_PE
        old_style ?   -1 :
#endif
        var_nb_arg, nb_args + 1, t, a);

    for (int i = nb_args; i; i--)
        if (a[i] & 1) {
            SValue *arg = &vtop[i - nb_args];
            int align, size = type_size(&arg->type, &align);
            assert((arg->type.t & VT_BTYPE) == VT_STRUCT);
            stack = (stack + align - 1) & -align;
            a1[i] = stack;
            stack += size;
        }

    stack = (stack + 15) >> 4 << 4;

    if (stack >= 0x1000000)
        mcc_error("stack size too big %lu", stack);
    arm64_sub_sp(stack);

    for (int i = nb_args; i; i--) {
        vpushv(vtop - nb_args + i);

        if (a[i] & 1) {
            int r = get_reg(RC_INT);
            arm64_spoff(intr(r), a1[i]);
            vset(&vtop->type, r | VT_LVAL, 0);
            vswap();
            vstore();
            if (a[i] >= 32) {
                r = get_reg(RC_INT);
                arm64_spoff(intr(r), a1[i]);
                arm64_strx(3, intr(r), 31, (a[i] - 32) >> 1 << 1);
            }
        }
        else if (a[i] >= 32) {
            if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
                int r = get_reg(RC_INT);
                arm64_spoff(intr(r), a[i] - 32);
                vset(&vtop->type, r | VT_LVAL, 0);
                vswap();
                vstore();
            }
            else if (is_float(vtop->type.t)) {
                gv(RC_FLOAT);
                arm64_strv(arm64_type_size(vtop[0].type.t),
                           fltr(vtop[0].r), 31, a[i] - 32);
            }
            else {
                gv(RC_INT);
                arm64_strx(3,
                           intr(vtop[0].r), 31, a[i] - 32);
            }
        }

        --vtop;
    }

    for (int i = nb_args; i; i--, vtop--) {
        if (a[i] < 16 && !(a[i] & 1)) {
            if ((variadic || old_style) && i > var_nb_arg && is_float(vtop->type.t)) {
                gv(RC_FLOAT);
                if ((vtop->type.t & VT_BTYPE) == VT_DOUBLE)
                    o(ARM64_FMOV_XD | intr(a[i] / 2) | fltr(vtop->r) << 5);
                else
                    o(ARM64_FMOV_WS | intr(a[i] / 2) | fltr(vtop->r) << 5);
            }
            else if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
                int align, size = type_size(&vtop->type, &align);
                if (size) {
                    gv_addr(RC_R(a[i] / 2));
                    arm64_ldrs(a[i] / 2, size);
                }
            }
            else
                gv(RC_R(a[i] / 2));
        }
        else if (a[i] < 16)
            arm64_spoff(a[i] / 2, a1[i]);
        else if (a[i] < 32) {
            if ((vtop->type.t & VT_BTYPE) == VT_STRUCT) {
                uint32_t sz, n = arm64_hfa(&vtop->type, &sz);
                if (n > 0) {
                    gv_addr(RC_R30);
                    for (uint32_t j = 0; j < n; j++)
                        o(0x3d4003c0 |
                          (sz & 16) << 19 | -(sz & 8) << 27 | (sz & 4) << 29 |
                          (a[i] / 2 - 8 + j) |
                          j << 10);
                } else {
                    gv(RC_F(a[i] / 2 - 8));
                }
            }
            else
                gv(RC_F(a[i] / 2 - 8));
        }
    }

    if ((return_type->t & VT_BTYPE) == VT_STRUCT) {
        if (a[0] == 1) {
            gv_addr(RC_R(8));
            --vtop;
        }
        else
            vswap();
    }

    arm64_gen_bl_or_b(0);
    --vtop;
    if (stack & 0xfff)
        o(0x910003ff | (stack & 0xfff) << 10);
    if (stack >> 12)
        o(0x914003ff | (stack >> 12) << 10);

    {
        int rt = return_type->t;
        int bt = rt & VT_BTYPE;
        if (bt == VT_STRUCT && !(a[0] & 1)) {
            gv_addr(RC_R(8));
            --vtop;
            if (a[0] == 0) {
                int align, size = type_size(return_type, &align);
                assert(size <= 16);
                if (size > 8)
                    o(0xa9000500);
                else if (size)
                    arm64_strx(size > 4 ? 3 : size > 2 ? 2 : size > 1, 0, 8, 0);

            }
            else if (a[0] == 16) {
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

static unsigned long arm64_func_va_list_stack;
static int arm64_func_va_list_gr_offs;
static int arm64_func_va_list_vr_offs;
static int arm64_func_sub_sp_offset;

static unsigned arm64_func_start_offset;
#define ARM64_FUNC_STACK_SETUP_SLOTS 6

#ifdef MCC_TARGET_PE
static unsigned long arm64_pe_param_off(unsigned long a)
{
    return a < 16 ? 160 + a / 2 * 8 :
           a < 32 ? 16 + (a - 16) / 2 * 16 :
           224 + ((a - 32) >> 1 << 1);
}
#endif

ST_FUNC void gfunc_prolog(Sym *func_sym)
{
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
        t[i] = &int_type;
#endif

    arm64_func_va_list_stack = arm64_pcs(var_nb_arg, n, t, a);

#ifdef MCC_TARGET_PE
    if (variadic)
        arm64_func_va_list_stack = arm64_pe_param_off(a[n - 1]);
#endif

#if !defined(MCC_TARGET_MACHO)
    if (variadic) {
        use_x8 = 1;
        last_int = 4;
        last_float = 4;
    }
#endif

    if (a && a[0] == 1)
        use_x8 = 1;
    for (i = 1, sym = func_type->ref->next; sym; i++, sym = sym->next) {
        if (a[i] < 16) {
            int last, align, size = type_size(&sym->type, &align);
	    last = a[i] / 4 + 1 + (size - 1) / 8;
	    last_int = last > last_int ? last : last_int;
	}
        else if (a[i] < 32) {
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
        o(0xa90923e8);
    for (i = 0; i < last_int; i++)
        o(0xa90a07e0 + i * 0x10000 + (i << 11) + (i << 1));

    arm64_func_va_list_gr_offs = -64;
    arm64_func_va_list_vr_offs = -128;

    for (i = 1, sym = func_type->ref->next; sym; i++, sym = sym->next) {
        int off = (a[i] < 16 ? 160 + a[i] / 2 * 8 :
                   a[i] < 32 ? 16 + (a[i] - 16) / 2 * 16 :
                   224 + ((a[i] - 32) >> 1 << 1));

        gfunc_set_param(sym, off, a[i] & 1);

        if (a[i] < 16) {
            int align, size = type_size(&sym->type, &align);
            arm64_func_va_list_gr_offs = (a[i] / 2 - 7 +
                                          (!(a[i] & 1) && size > 8)) * 8;
        }
        else if (a[i] < 32) {
            uint32_t hfa = arm64_hfa(&sym->type, 0);
            arm64_func_va_list_vr_offs = (a[i] / 2 - 16 +
                                          (hfa ? hfa : 1)) * 16;
        }

        if (16 <= a[i] && a[i] < 32 && (sym->type.t & VT_BTYPE) == VT_STRUCT) {
            uint32_t sz, k = arm64_hfa(&sym->type, &sz);
            if (k > 0 && sz < 16)
                for (uint32_t j = 0; j < k; j++) {
                    o(0x3d0003e0 | -(sz & 8) << 27 | (sz & 4) << 29 |
                      ((a[i] - 16) / 2 + j) | (off / sz + j) << 10);
                }
        }
    }

    mcc_free(a);
    mcc_free(t);

    arm64_func_sub_sp_offset = ind;
    for (i = 0; i < ARM64_FUNC_STACK_SETUP_SLOTS; ++i)
        o(ARM64_NOP);
    loc = 0;
#ifdef CONFIG_MCC_BCHECK
    if (mcc_state->do_bounds_check)
        gen_bounds_prolog();
#endif
}

ST_FUNC void gen_va_start(void)
{
    int r;
    --vtop;
    r = intr(gv_addr(RC_INT));

#ifdef MCC_TARGET_PE
    if (arm64_func_va_list_stack) {
        arm64_movimm(30, arm64_func_va_list_stack);
        o(0x8b1e03be);
    } else
        o(0x910283be);
    o(0xf900001e | r << 5);
#else
    if (arm64_func_va_list_stack) {
        arm64_movimm(30, arm64_func_va_list_stack + 224);
        o(0x8b1e03be);
    }
    else
        o(0x910383be);
    o(0xf900001e | r << 5);

#if !defined(MCC_TARGET_MACHO)
    if (arm64_func_va_list_gr_offs) {
        if (arm64_func_va_list_stack)
            o(0x910383be);
        o(0xf900041e | r << 5);
    }

    if (arm64_func_va_list_vr_offs) {
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

ST_FUNC void gen_va_arg(CType *t)
{
    int align, size = type_size(t, &align);
    uint32_t r0, r1;

#ifdef MCC_TARGET_PE
    int indirect = 0, slot = (size + 7) & -8;

    if (size > 16)
        indirect = 1, slot = 8;

    r0 = intr(gv_addr(RC_INT));
    r1 = get_reg(RC_INT);
    vtop[0].r = r1 | VT_LVAL;
    r1 = intr(r1);

    o(ARM64_LDR_X | ARM64_RN(r0) | r1);
    if (slot) {
        if (slot == 16) {
            o(0x910363be);
            o(0xeb1e003f | r1 << 5);
            o(0x54000041);
            o(0x910383a0 | r1 | 29 << 5);
        }
        if (align == 16) {
            o(0x91003c00 | r1 | r1 << 5);
            o(0x927cec00 | r1 | r1 << 5);
        }
        o(0x9100001e | r1 << 5 | slot << 10);
        o(0xf900001e | r0 << 5);
    }
    if (indirect)
        o(ARM64_LDR_X | ARM64_RN(r1) | r1);

#else
    unsigned fsize = size, hfa = 1;

    if (!is_float(t->t))
        hfa = arm64_hfa(t, &fsize);

    r0 = intr(gv_addr(RC_INT));
    r1 = get_reg(RC_INT);
    vtop[0].r = r1 | VT_LVAL;
    r1 = intr(r1);

    if (!hfa) {
        uint32_t n = size > 16 ? 8 : (size + 7) & -8;

#if !defined(MCC_TARGET_MACHO)
        o(0xb940181e | r0 << 5);
        if (align == 16) {
            assert(0);
            o(0x11003fde);
            o(0x121c6fde);
        }
        o(0x310003c0 | r1 | n << 10);
        o(0x540000ad);
#endif

        o(ARM64_LDR_X | ARM64_RN(r0) | r1);
        if (align == 16) {
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
            o(ARM64_LDR_X | ARM64_RN(r1) | r1);
    }
    else {
        uint32_t ssz = (size + 7) & -(uint32_t)8;
#if !defined(MCC_TARGET_MACHO)
        uint32_t rsz = hfa << 4;
        uint32_t b1, b2;
        o(0xb9401c1e | r0 << 5);
        o(0x310003c0 | r1 | rsz << 10);
        b1 = ind; o(0x5400000d);
#endif
        o(ARM64_LDR_X | ARM64_RN(r0) | r1);
        if (fsize == 16) {
            o(0x91003c00 | r1 | r1 << 5);
            o(0x927cec00 | r1 | r1 << 5);
        }
        o(0x9100001e | r1 << 5 | ssz << 10);
        o(0xf900001e | r0 << 5);
#if !defined(MCC_TARGET_MACHO)
        b2 = ind; o(ARM64_B);
        write32le(cur_text_section->data + b1, 0x5400000d | (ind - b1) << 3);
        o(0xb9001c00 | r1 | r0 << 5);
        o(0xf9400800 | r1 | r0 << 5);
        if (hfa == 1 || fsize == 16)
            o(0x8b3ec000 | r1 | r1 << 5);
        else {
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
                       int *align, int *regsize)
{
    return 0;
}

ST_FUNC void gfunc_return(CType *func_type)
{
    CType *t = func_type;
    unsigned long a;

    arm64_pcs(0, 1, &t, &a);

    switch (a) {
    case -1:
        break;
    case 0:
        if ((func_type->t & VT_BTYPE) == VT_STRUCT) {
            int align, size = type_size(func_type, &align);
            gv_addr(RC_R(0));
            arm64_ldrs(0, size);
        }
        else
            gv(RC_IRET);
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
        if ((func_type->t & VT_BTYPE) == VT_STRUCT) {
          uint32_t sz, n = arm64_hfa(func_type, &sz);
          gv_addr(RC_R(0));
          for (uint32_t j = 0; j < n; j++)
              o(0x3d400000 |
                (sz & 16) << 19 | -(sz & 8) << 27 | (sz & 4) << 29 |
                (fltr(REG_FRET) + j) | j << 10);
        }
        else
            gv(RC_FRET);
        break;
    default:
      assert(0);
    }
    vtop--;
}

ST_FUNC void gfunc_epilog(void)
{
#ifdef CONFIG_MCC_BCHECK
    if (mcc_state->do_bounds_check)
        gen_bounds_epilog();
#endif

    if (loc) {
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

ST_FUNC void gen_fill_nops(int bytes)
{
    if ((bytes & 3))
      mcc_error("alignment of code section not multiple of 4");
    while (bytes > 0) {
	o(ARM64_NOP);
	bytes -= 4;
    }
}

ST_FUNC int gjmp(int t)
{
    int r = ind;
    if (nocode_wanted)
        return t;
    o(t);
    return r;
}

ST_FUNC void gjmp_addr(int a)
{
    assert(a - ind + 0x8000000 < 0x10000000);
    o(ARM64_B | (((a - ind) >> 2) & 0x3ffffff));
}

ST_FUNC int gjmp_append(int n, int t)
{
    void *p;
    if (n) {
        uint32_t n1 = n, n2;
        while ((n2 = read32le(p = cur_text_section->data + n1)))
            n1 = n2;
        write32le(p, t);
        t = n;
    }
    return t;
}

void arm64_vset_VT_CMP(int op)
{
    if (op >= TOK_ULT && op <= TOK_GT) {
        vtop->cmp_r = vtop->r;
        vset_VT_CMP(0x80);
    }
}

static void arm64_gen_opil(int op, uint32_t l);

static void arm64_load_cmp(int r, SValue *sv)
{
    sv->r = sv->cmp_r;
    if (sv->c.i & 1) {
        vpushi(1);
        arm64_gen_opil('^', 0);
    }
    if (r != sv->r) {
        load(r, sv);
        sv->r = r;
    }
}

ST_FUNC int gjmp_cond(int op, int t)
{
    int bt = vtop->type.t & VT_BTYPE;

    int inv = op & 1;
    vtop->r = vtop->cmp_r;

    if (bt == VT_LDOUBLE) {
        uint32_t a, b, f = fltr(gv(RC_FLOAT));
        a = get_reg(RC_INT);
        vpushi(0);
        vtop[0].r = a;
        b = get_reg(RC_INT);
        a = intr(a);
        b = intr(b);
        o(0x4e083c00 | a | f << 5);
        o(0x4e183c00 | b | f << 5);
        o(0xaa000400 | a | a << 5 | b << 16);
        o(0xb4000040 | a | !!inv << 24);
        --vtop;
    }
    else if (bt == VT_FLOAT || bt == VT_DOUBLE) {
        uint32_t a = fltr(gv(RC_FLOAT));
        o(0x1e202008 | a << 5 | (bt != VT_FLOAT) << 22);
        o(0x54000040 | !!inv);
    }
    else {
        uint32_t ll = (bt == VT_PTR || bt == VT_LLONG);
        uint32_t a = intr(gv(RC_INT));
        o(0x34000040 | a | !!inv << 24 | ll << 31);
    }
    return gjmp(t);
}

static int arm64_iconst(uint64_t *val, SValue *sv)
{
    if ((sv->r & (VT_VALMASK | VT_LVAL | VT_SYM)) != VT_CONST)
        return 0;
    if (val) {
        int t = sv->type.t;
	int bt = t & VT_BTYPE;
        *val = ((bt == VT_LLONG || bt == VT_PTR) ? sv->c.i :
                (uint32_t)sv->c.i |
                (t & VT_UNSIGNED ? 0 : -(sv->c.i & 0x80000000)));
    }
    return 1;
}

static int arm64_gen_opic(int op, uint32_t l, int rev, uint64_t val,
                          uint32_t x, uint32_t a)
{
    if (op == '-' && !rev) {
        val = -val;
        op = '+';
    }
    val = l ? val : (uint32_t)val;

    switch (op) {

    case '+': {
        uint32_t s = l ? val >> 63 : val >> 31;
        val = s ? -val : val;
        val = l ? val : (uint32_t)val;
        if (!(val & ~0xffful))
            o(0x11000000 | l << 31 | s << 30 | x | a << 5 | val << 10);
        else if (!(val & ~0xfff000ul))
            o(0x11400000 | l << 31 | s << 30 | x | a << 5 | val >> 12 << 10);
        else {
            arm64_movimm(30, val);
            o(0x0b1e0000 | l << 31 | s << 30 | x | a << 5);
        }
        return 1;
      }

    case '-':
        if (!val)
            o(0x4b0003e0 | l << 31 | x | a << 16);
        else if (val == (l ? (uint64_t)-1 : (uint32_t)-1))
            o(0x2a2003e0 | l << 31 | x | a << 16);
        else {
            arm64_movimm(30, val);
            o(0x4b0003c0 | l << 31 | x | a << 16);
        }
        return 1;

    case '^':
        if (val == -1 || (val == 0xffffffff && !l)) {
            o(0x2a2003e0 | l << 31 | x | a << 16);
            return 1;
        }
    /* fall through */
    case '&':
    case '|': {
        int e = arm64_encode_bimm64(l ? val : val | val << 32);
        if (e < 0)
            return 0;
        o((op == '&' ? 0x12000000 :
           op == '|' ? 0x32000000 : 0x52000000) |
          l << 31 | x | a << 5 | (uint32_t)e << 10);
        return 1;
    }

    case TOK_SAR:
    case TOK_SHL:
    case TOK_SHR: {
        uint32_t n = 32 << l;
        val = val & (n - 1);
        if (rev)
            return 0;
        if (!val) {
            o(0x2a0003e0 | l << 31 | a << 16);
            return 1;
        }
        else if (op == TOK_SHL)
            o(0x53000000 | l << 31 | l << 22 | x | a << 5 |
              (n - val) << 16 | (n - 1 - val) << 10);
        else
            o(0x13000000 | (op == TOK_SHR) << 30 | l << 31 | l << 22 |
              x | a << 5 | val << 16 | (n - 1) << 10);
        return 1;
    }

    }
    return 0;
}

static void arm64_gen_opil(int op, uint32_t l)
{
    uint32_t x, a, b;

    {
        uint64_t val;
        int rev = 1;

        if (arm64_iconst(0, &vtop[0])) {
            vswap();
            rev = 0;
        }
        if (arm64_iconst(&val, &vtop[-1])) {
            gv(RC_INT);
            a = intr(vtop[0].r);
            --vtop;
            x = get_reg(RC_INT);
            ++vtop;
            if (arm64_gen_opic(op, l, rev, val, intr(x), a)) {
                vtop[0].r = x;
                vswap();
                --vtop;
                return;
            }
        }
        if (!rev)
            vswap();
    }

    gv2(RC_INT, RC_INT);
    assert(vtop[-1].r < VT_CONST && vtop[0].r < VT_CONST);
    a = intr(vtop[-1].r);
    b = intr(vtop[0].r);
    vtop -= 2;
    x = get_reg(RC_INT);
    ++vtop;
    vtop[0].r = x;
    x = intr(x);

    switch (op) {
    case '%':
        o(0x1ac00c00 | l << 31 | 30 | a << 5 | b << 16);
        o(0x1b008000 | l << 31 | x | (uint32_t)30 << 5 |
          b << 16 | a << 10);
        break;
    case '&':
        o(0x0a000000 | l << 31 | x | a << 5 | b << 16);
        break;
    case '*':
        o(0x1b007c00 | l << 31 | x | a << 5 | b << 16);
        break;
    case '+':
        o(0x0b000000 | l << 31 | x | a << 5 | b << 16);
        break;
    case '-':
        o(0x4b000000 | l << 31 | x | a << 5 | b << 16);
        break;
    case '/':
    case TOK_PDIV:
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
        o(0x1ac02800 | l << 31 | x | a << 5 | b << 16);
        break;
    case TOK_SHL:
        o(0x1ac02000 | l << 31 | x | a << 5 | b << 16);
        break;
    case TOK_SHR:
        o(0x1ac02400 | l << 31 | x | a << 5 | b << 16);
        break;
    case TOK_UDIV:
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
        o(0x1ac00800 | l << 31 | 30 | a << 5 | b << 16);
        o(0x1b008000 | l << 31 | x | (uint32_t)30 << 5 |
          b << 16 | a << 10);
        break;
    default:
        assert(0);
    }
}

ST_FUNC void gen_opi(int op)
{
    arm64_gen_opil(op, 0);
    arm64_vset_VT_CMP(op);
}

ST_FUNC void gen_opl(int op)
{
    arm64_gen_opil(op, 1);
    arm64_vset_VT_CMP(op);
}

ST_FUNC void gen_opf(int op)
{
    uint32_t x, a, b, dbl;
    int bt = vtop[0].type.t & VT_BTYPE;

    if (op == TOK_NEG) {
        if (bt == VT_LDOUBLE) {
            vpush_helper_func(TOK___negtf2);
            vrott(2);
            gfunc_call(1);
            vpushi(0);
            vtop->type.t = bt;
            vtop->r = REG_FRET;
        } else {
            gv(RC_FLOAT);
            dbl = bt == VT_DOUBLE;
            a = fltr(vtop[0].r);
            o(0x1e214000 | dbl << 22 | a | a << 5);
        }
        return;
    }

    if (bt == VT_LDOUBLE) {
        CType type = vtop[0].type;
        int func = 0;
        int cond = -1;
        switch (op) {
        case '*': func = TOK___multf3; break;
        case '+': func = TOK___addtf3; break;
        case '-': func = TOK___subtf3; break;
        case '/': func = TOK___divtf3; break;
        case TOK_EQ: func = TOK___eqtf2; cond = 1; break;
        case TOK_NE: func = TOK___netf2; cond = 0; break;
        case TOK_LT: func = TOK___lttf2; cond = 10; break;
        case TOK_GE: func = TOK___getf2; cond = 11; break;
        case TOK_LE: func = TOK___letf2; cond = 12; break;
        case TOK_GT: func = TOK___gttf2; cond = 13; break;
        default: assert(0); break;
        }
        vpush_helper_func(func);
        vrott(3);
        gfunc_call(2);
        vpushi(0);
        vtop->r = cond < 0 ? REG_FRET : REG_IRET;
        if (cond < 0)
            vtop->type = type;
        else {
            o(0x7100001f);
            o(0x1a9f07e0 | (uint32_t)cond << 12);
        }
        arm64_vset_VT_CMP(op);
        return;
    }

    dbl = bt != VT_FLOAT;
    gv2(RC_FLOAT, RC_FLOAT);
    assert(vtop[-1].r < VT_CONST && vtop[0].r < VT_CONST);
    a = fltr(vtop[-1].r);
    b = fltr(vtop[0].r);
    vtop -= 2;
    switch (op) {
    case TOK_EQ: case TOK_NE:
    case TOK_LT: case TOK_GE: case TOK_LE: case TOK_GT:
        x = get_reg(RC_INT);
        ++vtop;
        vtop[0].r = x;
        x = intr(x);
        break;
    default:
        x = get_reg(RC_FLOAT);
        ++vtop;
        vtop[0].r = x;
        x = fltr(x);
        break;
    }

    switch (op) {
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

ST_FUNC void gen_cvt_sxtw(void)
{
    uint32_t r = intr(gv(RC_INT));
    o(0x93407c00 | r | r << 5);
}

ST_FUNC void gen_cvt_csti(int t)
{
    int r = intr(gv(RC_INT));
    o(0x13001c00
        | ((t & VT_BTYPE) == VT_SHORT) << 13
        | (uint32_t)!!(t & VT_UNSIGNED) << 30
        | r | r << 5);
}

ST_FUNC void gen_cvt_itof(int t)
{
    if (t == VT_LDOUBLE) {
        int f = vtop->type.t;
        int func = (f & VT_BTYPE) == VT_LLONG ?
          (f & VT_UNSIGNED ? TOK___floatunditf : TOK___floatditf) :
          (f & VT_UNSIGNED ? TOK___floatunsitf : TOK___floatsitf);
        vpush_helper_func(func);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        vtop->type.t = t;
        vtop->r = REG_FRET;
        return;
    }
    else {
        int d, n = intr(gv(RC_INT));
        int s = !(vtop->type.t & VT_UNSIGNED);
        uint32_t l = ((vtop->type.t & VT_BTYPE) == VT_LLONG);
        --vtop;
        d = get_reg(RC_FLOAT);
        ++vtop;
        vtop[0].r = d;
        o(0x1e220000 | (uint32_t)!s << 16 |
          (uint32_t)(t != VT_FLOAT) << 22 | fltr(d) |
          l << 31 | n << 5);
    }
}

ST_FUNC void gen_cvt_ftoi(int t)
{
    if ((vtop->type.t & VT_BTYPE) == VT_LDOUBLE) {
        int func = (t & VT_BTYPE) == VT_LLONG ?
          (t & VT_UNSIGNED ? TOK___fixunstfdi : TOK___fixtfdi) :
          (t & VT_UNSIGNED ? TOK___fixunstfsi : TOK___fixtfsi);
        vpush_helper_func(func);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        vtop->type.t = t;
        vtop->r = REG_IRET;
        return;
    }
    else {
        int d, n = fltr(gv(RC_FLOAT));
        uint32_t l = ((vtop->type.t & VT_BTYPE) != VT_FLOAT);
        --vtop;
        d = get_reg(RC_INT);
        ++vtop;
        vtop[0].r = d;
        o(0x1e380000 |
          (uint32_t)!!(t & VT_UNSIGNED) << 16 |
          (uint32_t)((t & VT_BTYPE) == VT_LLONG) << 31 | intr(d) |
          l << 22 | n << 5);
    }
}

ST_FUNC void gen_cvt_ftof(int t)
{
    int f = vtop[0].type.t & VT_BTYPE;
    assert(t == VT_FLOAT || t == VT_DOUBLE || t == VT_LDOUBLE);
    assert(f == VT_FLOAT || f == VT_DOUBLE || f == VT_LDOUBLE);
    if (t == f)
        return;

    if (t == VT_LDOUBLE || f == VT_LDOUBLE) {
        int func = (t == VT_LDOUBLE) ?
            (f == VT_FLOAT ? TOK___extendsftf2 : TOK___extenddftf2) :
            (t == VT_FLOAT ? TOK___trunctfsf2 : TOK___trunctfdf2);
        vpush_helper_func(func);
        vrott(2);
        gfunc_call(1);
        vpushi(0);
        vtop->type.t = t;
        vtop->r = REG_FRET;
    }
    else {
        int x, a;
        gv(RC_FLOAT);
        assert(vtop[0].r < VT_CONST);
        a = fltr(vtop[0].r);
        x = a;
        if (f == VT_FLOAT)
            o(0x1e22c000 | x | a << 5);
        else
            o(0x1e624000 | x | a << 5);
    }
}

ST_FUNC void gen_increment_tcov (SValue *sv)
{
    int r1, r2;

    vpushv(sv);
    vtop->r = r1 = get_reg(RC_INT);
    r2 = get_reg(RC_INT);
    arm64_sym(r1, sv->sym, 0);
    o(ARM64_LDR_X | ARM64_RN(intr(r1)) | intr(r2));
    o(0x91000400 | (intr(r2)<<5) | intr(r2));
    o(0xf9000000 | (intr(r1)<<5) | intr(r2));
    vpop();
}

ST_FUNC void ggoto(void)
{
    arm64_gen_bl_or_b(1);
    --vtop;
}

ST_FUNC void gen_clear_cache(void)
{
    uint32_t beg, end, dsz, isz, p, lab1, b1;
    gv2(RC_INT, RC_INT);
    vpushi(0);
    vtop->r = get_reg(RC_INT);
    vpushi(0);
    vtop->r = get_reg(RC_INT);
    vpushi(0);
    vtop->r = get_reg(RC_INT);
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
    b1 = ind; o(ARM64_B);
    lab1 = ind;
    o(0xd50b7b20 | p);
    o(0x8b000000 | p | p << 5 | dsz << 16);
    write32le(cur_text_section->data + b1, ARM64_B | ((ind - b1) >> 2));
    o(0xeb00001f | p << 5 | end << 16);
    o(0x54ffffa3 | ((lab1 - ind) << 3 & 0xffffe0));
    o(0xd5033b9f);
    o(0x51000400 | p | isz << 5);
    o(0x8a240004 | p | beg << 5 | p << 16);
    b1 = ind; o(ARM64_B);
    lab1 = ind;
    o(0xd50b7520 | p);
    o(0x8b000000 | p | p << 5 | isz << 16);
    write32le(cur_text_section->data + b1, ARM64_B | ((ind - b1) >> 2));
    o(0xeb00001f | p << 5 | end << 16);
    o(0x54ffffa3 | ((lab1 - ind) << 3 & 0xffffe0));
    o(0xd5033b9f);
    o(0xd5033fdf);
}

ST_FUNC void gen_vla_sp_save(int addr) {
    uint32_t r = intr(get_reg(RC_INT));
    o(0x910003e0 | r);
    arm64_strx(3, r, 29, addr);
}

ST_FUNC void gen_vla_sp_restore(int addr) {
    uint32_t r = 30;
    arm64_ldrx(0, 3, r, 29, addr);
    o(0x9100001f | r << 5);
}

ST_FUNC void gen_vla_alloc(CType *type, int align) {
    uint32_t r;
#if defined(CONFIG_MCC_BCHECK)
    if (mcc_state->do_bounds_check)
        vpushv(vtop);
#endif
    r = intr(gv(RC_INT));
#if defined(CONFIG_MCC_BCHECK)
    if (mcc_state->do_bounds_check)
        o(0x91004000 | r | r << 5);
    else
#endif
    o(0x91003c00 | r | r << 5);
    o(0x927cec00 | r | r << 5);
    o(0xcb2063ff | r << 16);
    vpop();
#if defined(CONFIG_MCC_BCHECK)
    if (mcc_state->do_bounds_check) {
        vpushi(0);
        vtop->r = TREG_R(0);
        o(0x910003e0 | vtop->r);
        vswap();
        vpush_helper_func(TOK___bound_new_region);
        vrott(3);
        gfunc_call(2);
        func_bound_add_epilog = 1;
    }
#endif
}

#endif
