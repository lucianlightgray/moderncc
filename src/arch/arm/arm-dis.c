/*
 * arm-dis.c - ARM (AArch32, ARM mode) instruction disassembler for `-S`.
 *
 * Decodes the instruction subset arm-gen.c emits (data-processing with the
 * condition field, mul/mla/umull/smull, ldr/str byte/half/word with imm/reg
 * offsets and pre/post indexing, ldm/stm/push/pop, b/bl/bx/blx, and the VFP
 * subset used by the EABI hardfloat configuration) and prints it in the
 * syntax the integrated assembler (arm-asm.c) accepts, so `-S` listings
 * re-assemble to the same machine code.
 *
 * Every printed form is chosen so arm-asm.c re-encodes the SAME bits:
 *  - rotated immediates are only printed when the encoding uses the minimal
 *    rotation (both arm-gen.c's stuff_const() and arm-asm.c pick it);
 *  - vldr/vstr offsets beyond arm-asm.c's 8-bit operand classifier, and any
 *    other coprocessor encodings, fall back to the generic ldc/stc/cdp/
 *    mcr/mrc spellings which cover the whole coprocessor space byte-exactly;
 *  - anything not provably re-encodable is emitted as `.long 0x%08x`
 *    (mcc's assembler treats `.word` as 16-bit, so `.long` is the 32-bit
 *    data directive here).
 *
 * See x86_64-dis.c for the reference decoder shape.
 */
#include "mcc.h"

#ifdef MCC_TARGET_ARM

/* ---- tables ----------------------------------------------------------- */

/* condition suffix, indexed by the cond field; 14 (AL) prints empty and
   15 (the unconditional-extension space) is never printed. */
static const char * const cc_sfx[16] = {
    "eq","ne","cs","cc","mi","pl","vs","vc",
    "hi","ls","ge","lt","gt","le","",""
};

static const char * const regnm[16] = {
    "r0","r1","r2","r3","r4","r5","r6","r7",
    "r8","r9","r10","r11","r12","sp","lr","pc"
};

static const char * const dpnm[16] = {
    "and","eor","sub","rsb","add","adc","sbc","rsc",
    "tst","teq","cmp","cmn","orr","mov","bic","mvn"
};

static const char * const shnm[4] = { "lsl","lsr","asr","ror" };

/* ---- small text builder ------------------------------------------------ */

typedef struct {
    char *p;
    char *end;
} SB;

static void sb_init(SB *b, char *buf, int size)
{
    b->p = buf;
    b->end = buf + size;
    buf[0] = 0;
}

static void sb(SB *b, const char *fmt, ...)
{
    va_list ap;
    int n;
    if (b->p >= b->end)
        return;
    va_start(ap, fmt);
    n = vsnprintf(b->p, b->end - b->p, fmt, ap);
    va_end(ap);
    if (n > 0)
        b->p += (n < b->end - b->p) ? n : (int)(b->end - b->p);
}

/* ---- helpers ----------------------------------------------------------- */

static uint32_t ror32(uint32_t v, unsigned r)
{
    r &= 31;
    return r ? (v >> r) | (v << (32 - r)) : v;
}

static uint32_t rol32(uint32_t v, unsigned r)
{
    r &= 31;
    return r ? (v << r) | (v >> (32 - r)) : v;
}

/* A data-processing immediate (rot,imm8) is only printed if it is the
   canonical minimal-rotation encoding of its value: arm-gen.c's stuff_const
   and arm-asm.c's encoder both pick the smallest rotation, so any other
   choice would re-assemble to different bits. */
static int imm_canonical(uint32_t rot, uint32_t imm8)
{
    uint32_t v = ror32(imm8, 2 * rot);
    uint32_t r;
    for (r = 0; r < 16; r++)
        if (rol32(v, 2 * r) < 0x100)
            return r == rot;
    return 0;
}

static void sb_imm(SB *b, uint32_t v)
{
    if (v < 1024)
        sb(b, "#%u", v);
    else
        sb(b, "#0x%x", v);
}

/* Barrel-shifter operand text for bits [11:0] of a register-form operand
   (also used for register-offset addressing).  Returns 0 if the encoding
   has no arm-asm.c spelling (LSR/ASR #32, register shift with bit7 set). */
static int sb_shift_op(SB *b, uint32_t w, int allow_regshift)
{
    int rm = w & 15, ty = (w >> 5) & 3;
    if (w & 0x10) { /* shift by register */
        if (!allow_regshift || (w & 0x80))
            return 0;
        sb(b, "%s, %s %s", regnm[rm], shnm[ty], regnm[(w >> 8) & 15]);
    } else {
        int amt = (w >> 7) & 31;
        if (amt == 0) {
            if (ty == 0)
                sb(b, "%s", regnm[rm]);          /* no shift */
            else if (ty == 3)
                sb(b, "%s, rrx", regnm[rm]);     /* ROR #0 == RRX */
            else
                return 0;                        /* LSR/ASR #32: no spelling */
        } else {
            sb(b, "%s, %s #%d", regnm[rm], shnm[ty], amt);
        }
    }
    return 1;
}

/* {r0, r1, ...} register list */
static void sb_reglist(SB *b, uint32_t mask)
{
    int i, first = 1;
    sb(b, "{");
    for (i = 0; i < 16; i++)
        if (mask & (1u << i)) {
            sb(b, "%s%s", first ? "" : ", ", regnm[i]);
            first = 0;
        }
    sb(b, "}");
}

/* ---- decoders (build text into b; return 0 => fall back to .long) ----- */

/* branches: B/BL (bits 27-25 == 101) */
static int dis_branch(disasm_ctx *dc, uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    const char *nm = (w & 0x01000000) ? "bl" : "b";
    int32_t off = w & 0xffffff;
    int rtype = 0;
    const char *rel;
    if (off & 0x800000)
        off -= 0x1000000;
    rel = disasm_reloc(dc, dc->pc, 4, &rtype);
    if (rel && (rtype == R_ARM_PC24 || rtype == R_ARM_CALL
             || rtype == R_ARM_JUMP24 || rtype == R_ARM_PLT32)) {
        /* REL branch field: A = off*4, operand value v satisfies
           field = (v-8)/4 in arm-asm.c, i.e. v = off*4 + 8. */
        long v = (long)off * 4 + 8;
        if (v == 0)
            sb(b, "%s%s\t%s", nm, cc, rel);
        else if (v > 0)
            sb(b, "%s%s\t%s+%ld", nm, cc, rel, v);
        else
            sb(b, "%s%s\t%s-%ld", nm, cc, rel, -v);
        return 1;
    }
    if (rel)
        return 0; /* unexpected reloc type on a branch field */
    {
        /* a target outside the section (e.g. a literal-pool constant that
           happens to look like a branch) has no label to resolve against */
        addr_t target = dc->pc + 8 + (addr_t)((long)off * 4);
        if (target >= dc->size || (target & 3))
            return 0;
        sb(b, "%s%s\t%s", nm, cc, disasm_label(dc, target));
    }
    return 1;
}

/* data processing (register or immediate operand2), incl. mov-shift forms */
static int dis_dp(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int I = (w >> 25) & 1, opc = (w >> 21) & 15, S = (w >> 20) & 1;
    int rn = (w >> 16) & 15, rd = (w >> 12) & 15;
    char o2buf[64];
    SB o2;

    /* movw/movt live in the imm space where cmp-family would have S=0 */
    if ((w & 0x0FF00000) == 0x03000000 || (w & 0x0FF00000) == 0x03400000) {
        uint32_t imm16 = ((w >> 4) & 0xf000) | (w & 0xfff);
        sb(b, "%s%s\t%s, ", (w & 0x00400000) ? "movt" : "movw", cc, regnm[rd]);
        sb_imm(b, imm16);
        return 1;
    }

    if (!S && opc >= 8 && opc <= 11)
        return 0; /* mrs/msr/misc space */

    sb_init(&o2, o2buf, sizeof o2buf);
    if (I) {
        uint32_t rot = (w >> 8) & 15, imm8 = w & 0xff;
        if (!imm_canonical(rot, imm8))
            return 0;
        sb_imm(&o2, ror32(imm8, 2 * rot));
    } else {
        if (!sb_shift_op(&o2, w, 1))
            return 0;
    }

    if (opc == 13 || opc == 15) { /* mov/mvn: no rn operand */
        if (rn)
            return 0;
        if (opc == 13 && !I) {
            /* print register-shift forms with the shift mnemonics that
               arm-asm.c's asm_shift_opcode encodes identically */
            int rm = w & 15, ty = (w >> 5) & 3;
            if (w & 0x10) { /* mov rd, rm, <sh> rs  =>  <sh> rd, rm, rs */
                if (w & 0x80)
                    return 0;
                sb(b, "%s%s%s\t%s, %s, %s", shnm[ty], S ? "s" : "", cc,
                   regnm[rd], regnm[rm], regnm[(w >> 8) & 15]);
                return 1;
            } else {
                int amt = (w >> 7) & 31;
                if (amt == 0 && ty == 3) { /* RRX */
                    sb(b, "rrx%s%s\t%s, %s", S ? "s" : "", cc,
                       regnm[rd], regnm[rm]);
                    return 1;
                }
                if (amt) {
                    sb(b, "%s%s%s\t%s, %s, #%d", shnm[ty], S ? "s" : "", cc,
                       regnm[rd], regnm[rm], amt);
                    return 1;
                }
                if (ty)
                    return 0; /* LSR/ASR #32 */
                /* plain mov rd, rm falls through */
            }
        }
        sb(b, "%s%s%s\t%s, %s", dpnm[opc], S ? "s" : "", cc, regnm[rd], o2buf);
        return 1;
    }
    if (opc >= 8 && opc <= 11) { /* tst/teq/cmp/cmn: no rd operand */
        if (rd)
            return 0;
        sb(b, "%s%s\t%s, %s", dpnm[opc], cc, regnm[rn], o2buf);
        return 1;
    }
    sb(b, "%s%s%s\t%s, %s, %s", dpnm[opc], S ? "s" : "", cc,
       regnm[rd], regnm[rn], o2buf);
    return 1;
}

/* multiplies: mul/mla (bits 27-22 == 0, bits 7-4 == 1001) */
static int dis_mul(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int A = (w >> 21) & 1, S = (w >> 20) & 1;
    int rd = (w >> 16) & 15, ra = (w >> 12) & 15;
    int rs = (w >> 8) & 15, rm = w & 15;
    if (A)
        sb(b, "mla%s%s\t%s, %s, %s, %s", S ? "s" : "", cc,
           regnm[rd], regnm[rm], regnm[rs], regnm[ra]);
    else {
        if (ra)
            return 0;
        sb(b, "mul%s%s\t%s, %s, %s", S ? "s" : "", cc,
           regnm[rd], regnm[rm], regnm[rs]);
    }
    return 1;
}

/* long multiplies: umull/umlal/smull/smlal (bits 27-23 == 00001, 7-4 == 1001) */
static int dis_mull(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int sg = (w >> 22) & 1, A = (w >> 21) & 1, S = (w >> 20) & 1;
    int rdhi = (w >> 16) & 15, rdlo = (w >> 12) & 15;
    int rs = (w >> 8) & 15, rm = w & 15;
    sb(b, "%s%s%s%s\t%s, %s, %s, %s",
       sg ? "s" : "u", A ? "mlal" : "mull", S ? "s" : "", cc,
       regnm[rdlo], regnm[rdhi], regnm[rm], regnm[rs]);
    return 1;
}

/* common addressing-mode tail: "[rn, OFF]" / "[rn, OFF]!" / "[rn], OFF".
   `off` is the operand text (may be "" for none); pre/wb from P/W bits.
   Returns 0 for the unindexed-translation form (P=0,W=1). */
static int sb_addr(SB *b, int rn, const char *off, int Pb, int W)
{
    if (Pb) {
        if (off[0])
            sb(b, "[%s, %s]%s", regnm[rn], off, W ? "!" : "");
        else if (W)
            sb(b, "[%s, #0]!", regnm[rn]); /* '[rn]!' has no parse */
        else
            sb(b, "[%s]", regnm[rn]);
    } else {
        if (W)
            return 0; /* ldrt/strt family: not expressible */
        sb(b, "[%s], %s", regnm[rn], off[0] ? off : "#0");
    }
    return 1;
}

/* word/byte loads and stores (bits 27-26 == 01) */
static int dis_mem(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int I = (w >> 25) & 1, Pb = (w >> 24) & 1, U = (w >> 23) & 1;
    int B = (w >> 22) & 1, W = (w >> 21) & 1, L = (w >> 20) & 1;
    int rn = (w >> 16) & 15, rd = (w >> 12) & 15;
    char offbuf[64];
    SB off;

    sb_init(&off, offbuf, sizeof offbuf);
    if (I) { /* register offset (bit 25 set means register form here) */
        if (w & 0x10)
            return 0; /* media space */
        if (!U)
            sb(&off, "-");
        if (!sb_shift_op(&off, w, 0))
            return 0;
    } else {
        uint32_t imm = w & 0xfff;
        if (imm)
            sb(&off, "#%s%u", U ? "" : "-", imm);
        else if (!U)
            return 0; /* -0: re-assembles as +0 */
    }
    sb(b, "%s%s%s\t%s, ", L ? "ldr" : "str", B ? "b" : "", cc, regnm[rd]);
    return sb_addr(b, rn, offbuf, Pb, W);
}

/* halfword / signed loads, strh (bits 27-25 == 000, bit7 & bit4, SH != 00) */
static int dis_hmem(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int Pb = (w >> 24) & 1, U = (w >> 23) & 1, I = (w >> 22) & 1;
    int W = (w >> 21) & 1, L = (w >> 20) & 1, SH = (w >> 5) & 3;
    int rn = (w >> 16) & 15, rd = (w >> 12) & 15;
    const char *nm;
    char offbuf[32];
    SB off;

    if (L)
        nm = SH == 1 ? "ldrh" : SH == 2 ? "ldrsb" : "ldrsh";
    else if (SH == 1)
        nm = "strh";
    else
        return 0; /* ldrd/strd: not expressible */

    sb_init(&off, offbuf, sizeof offbuf);
    if (I) {
        uint32_t imm = ((w >> 4) & 0xf0) | (w & 15);
        if (imm)
            sb(&off, "#%s%u", U ? "" : "-", imm);
        else if (!U)
            return 0;
    } else {
        if (w & 0xf00)
            return 0;
        sb(&off, "%s%s", U ? "" : "-", regnm[w & 15]);
    }
    sb(b, "%s%s\t%s, ", nm, cc, regnm[rd]);
    return sb_addr(b, rn, offbuf, Pb, W);
}

/* block transfers (bits 27-25 == 100) */
static int dis_ldm(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int Pb = (w >> 24) & 1, U = (w >> 23) & 1, S = (w >> 22) & 1;
    int W = (w >> 21) & 1, L = (w >> 20) & 1;
    int rn = (w >> 16) & 15;
    uint32_t list = w & 0xffff;
    static const char * const sfx[4] = { "da", "ia", "db", "ib" };

    if (S || !list)
        return 0;
    if (rn == 13 && W && L && !Pb && U)
        sb(b, "pop%s\t", cc);
    else if (rn == 13 && W && !L && Pb && !U)
        sb(b, "push%s\t", cc);
    else
        sb(b, "%s%s%s\t%s%s, ", L ? "ldm" : "stm", sfx[(Pb << 1) | U], cc,
           regnm[rn], W ? "!" : "");
    sb_reglist(b, list);
    return 1;
}

/* ---- VFP / coprocessor space ------------------------------------------ */

/* single-precision register name from its (even<<1|odd) parts */
static void sb_sreg(SB *b, int base4, int lowbit)
{
    sb(b, "s%d", (base4 << 1) | lowbit);
}

/* VFP register-range list {s0-s15} / {d0-d1} */
static void sb_vlist(SB *b, int dbl, int first, int count)
{
    if (count == 1)
        sb(b, "{%c%d}", dbl ? 'd' : 's', first);
    else
        sb(b, "{%c%d-%c%d}", dbl ? 'd' : 's', first,
           dbl ? 'd' : 's', first + count - 1);
}

/* coprocessor loads/stores (bits 27-25 == 110): vldr/vstr/vpush/vpop/
   vldm/vstm for cp10/cp11, generic ldc/stc otherwise (also the fallback
   when the v-form operand does not survive arm-asm.c's operand parser). */
static int dis_cpmem(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int Pb = (w >> 24) & 1, U = (w >> 23) & 1, D = (w >> 22) & 1;
    int W = (w >> 21) & 1, L = (w >> 20) & 1;
    int rn = (w >> 16) & 15, crd = (w >> 12) & 15, cp = (w >> 8) & 15;
    uint32_t imm8 = w & 0xff;
    int off = imm8 * 4;
    int vfp = (cp == 10 || cp == 11), dbl = (cp == 11);

    if (vfp && Pb && !W && !dbl) {
        /* vldr/vstr, single precision only: arm-asm.c's vldr/vstr parser
           mis-consumes a token after a d<N> operand (extra next()), and only
           accepts an 8-bit immediate operand (-255..255) -- so doubles and
           offsets above 252 use the byte-identical ldc spelling below */
        if (imm8 <= 63 && (imm8 || U)) {
            sb(b, "%s%s\t", L ? "vldr" : "vstr", cc);
            sb_sreg(b, crd, D);
            if (imm8)
                sb(b, ", [%s, #%s%d]", regnm[rn], U ? "" : "-", off);
            else
                sb(b, ", [%s]", regnm[rn]);
            return 1;
        }
    } else if (vfp && imm8 && !(Pb && !W) && !(dbl && ((imm8 & 1) || D))) {
        int count = dbl ? imm8 >> 1 : imm8;
        int first = dbl ? crd : (crd << 1) | D;
        if (Pb && !U && W) {          /* vstmdb/vldmdb (vpush) */
            if (rn == 13 && !L)
                sb(b, "vpush%s\t", cc);
            else
                sb(b, "%s%s\t%s!, ", L ? "vldmdb" : "vstmdb", cc, regnm[rn]);
            sb_vlist(b, dbl, first, count);
            return 1;
        }
        if (!Pb && U) {               /* vldmia/vstmia (vpop) */
            if (rn == 13 && W && L)
                sb(b, "vpop%s\t", cc);
            else
                sb(b, "%s%s\t%s%s, ", L ? "vldmia" : "vstmia", cc,
                   regnm[rn], W ? "!" : "");
            sb_vlist(b, dbl, first, count);
            return 1;
        }
    }

    /* generic coprocessor transfer; covers every remaining P/U/W form the
       assembler can express */
    {
        char offbuf[24];
        SB ob;
        sb_init(&ob, offbuf, sizeof offbuf);
        if (imm8)
            sb(&ob, "#%s%d", U ? "" : "-", off);
        else if (!U)
            return 0;
        sb(b, "%s%s%s\tp%d, c%d, ", L ? "ldc" : "stc", D ? "l" : "", cc,
           cp, crd);
        return sb_addr(b, rn, offbuf, Pb, W);
    }
}

/* VFP data-processing (CDP space, cp10/cp11).  Returns 0 to let the caller
   fall back to the generic cdp spelling (also byte-exact).
   D/N/M extend Fd/Fn/Fm to a single-precision register number (s = F*2+bit);
   for double-precision operands arm-asm.c can only name d0-d15, so the
   extension bit of a double operand must be clear. */
static int dis_vfp_dp(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int dbl = ((w >> 8) & 15) == 11;
    const char *pr = dbl ? "f64" : "f32";
    int D = (w >> 22) & 1, N = (w >> 7) & 1, M = (w >> 5) & 1;
    int Fd = (w >> 12) & 15, Fn = (w >> 16) & 15, Fm = w & 15;
    int op6 = (w >> 6) & 1;
    int sel = (w >> 20) & 0xb; /* opc1 with the D bit masked out */
    char d[8], m[8];

    /* dest and src-m in the instruction's own precision */
    if (dbl) {
        if (D)
            return 0;
        snprintf(d, sizeof d, "d%d", Fd);
        snprintf(m, sizeof m, "d%d", Fm);
    } else {
        snprintf(d, sizeof d, "s%d", (Fd << 1) | D);
        snprintf(m, sizeof m, "s%d", (Fm << 1) | M);
    }

    if (sel == 0xb) { /* extension ops: opcode in Fn, bit6 must be set */
        if (!op6)
            return 0;
        switch (Fn) {
        case 0:
        case 1:
        case 4: {
            static const char * const enm[3][2] = {
                { "vmov", "vabs" }, { "vneg", "vsqrt" }, { "vcmp", "vcmpe" } };
            if (dbl && M)
                return 0;
            sb(b, "%s%s.%s\t%s, %s",
               enm[Fn == 4 ? 2 : Fn][N], cc, pr, d, m);
            return 1;
        }
        case 5: /* compare with zero: Fm/M must be zero */
            if (Fm || M)
                return 0;
            sb(b, "%s%s.%s\t%s, #0", N ? "vcmpe" : "vcmp", cc, pr, d);
            return 1;
        case 7: /* f32 <-> f64 */
            if (!N)
                return 0;
            if (dbl) { /* vcvt.f32.f64 sX, dY: dest single, src double */
                if (M)
                    return 0;
                sb(b, "vcvt%s.f32.f64\ts%d, d%d", cc, (Fd << 1) | D, Fm);
            } else {   /* vcvt.f64.f32 dX, sY: dest double, src single */
                if (D)
                    return 0;
                sb(b, "vcvt%s.f64.f32\td%d, s%d", cc, Fd, (Fm << 1) | M);
            }
            return 1;
        case 8: /* int -> float; source is always a single register */
            sb(b, "vcvt%s.%s.%s\t%s, s%d", cc, pr, N ? "s32" : "u32", d,
               (Fm << 1) | M);
            return 1;
        case 0xc:
        case 0xd: /* float -> int; dest is always a single register.
                     N=1: round-to-zero (vcvt); N=0: current mode (vcvtr) */
            if (dbl && M)
                return 0;
            sb(b, "%s%s.%s.%s\ts%d, %s", N ? "vcvt" : "vcvtr", cc,
               (Fn & 1) ? "s32" : "u32", pr, (Fd << 1) | D, m);
            return 1;
        }
        return 0;
    }

    /* three-operand arithmetic */
    {
        char n[8];
        if (dbl) {
            if (N || M)
                return 0;
            snprintf(n, sizeof n, "d%d", Fn);
        } else {
            snprintf(n, sizeof n, "s%d", (Fn << 1) | N);
        }
        switch (sel) {
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
                return 0;
            sb(b, "vdiv%s.%s\t%s, %s, %s", cc, pr, d, n, m);
            return 1;
        }
    }
    return 0;
}

/* CDP space (bits 27-24 == 1110, bit4 == 0) */
static int dis_cdp(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int cp = (w >> 8) & 15;
    if ((cp == 10 || cp == 11) && dis_vfp_dp(w, b))
        return 1;
    /* generic cdp round-trips any remaining encoding byte-exactly */
    sb(b, "cdp%s\tp%d, %d, c%d, c%d, c%d, %d", cc, cp,
       (w >> 20) & 15, (w >> 12) & 15, (w >> 16) & 15, w & 15, (w >> 5) & 7);
    return 1;
}

/* MRC/MCR space (bits 27-24 == 1110, bit4 == 1) */
static int dis_mrc(uint32_t w, SB *b)
{
    const char *cc = cc_sfx[w >> 28];
    int cp = (w >> 8) & 15, L = (w >> 20) & 1;
    int rt = (w >> 12) & 15;

    if (cp == 10) {
        if ((w & 0x0FFF0FFF) == 0x0EF10A10) { /* vmrs rt, fpscr */
            sb(b, "vmrs%s\t%s, fpscr", cc, rt == 15 ? "apsr_nzcv" : regnm[rt]);
            return 1;
        }
        if ((w & 0x0FFF0FFF) == 0x0EE10A10 && rt != 15) { /* vmsr fpscr, rt */
            sb(b, "vmsr%s\tfpscr, %s", cc, regnm[rt]);
            return 1;
        }
        if ((w & 0x0FE00F7F) == 0x0E000A10) { /* vmov sN <-> rT */
            int sn = (((w >> 16) & 15) << 1) | ((w >> 7) & 1);
            if (rt != 15) {
                if (L)
                    sb(b, "vmov%s.f32\t%s, s%d", cc, regnm[rt], sn);
                else
                    sb(b, "vmov%s.f32\ts%d, %s", cc, sn, regnm[rt]);
                return 1;
            }
        }
    }
    /* generic mrc/mcr covers the rest of this space byte-exactly
       (including cp15 TLS reads and the cp11 lane moves) */
    sb(b, "%s%s\tp%d, %d, %s, c%d, c%d, %d", L ? "mrc" : "mcr", cc, cp,
       (w >> 21) & 7, regnm[rt], (w >> 16) & 15, w & 15, (w >> 5) & 7);
    return 1;
}

/* ---- top level --------------------------------------------------------- */

static int decode(disasm_ctx *dc, uint32_t w, SB *b)
{
    uint32_t cond = w >> 28;

    if (cond == 15)
        return 0; /* unconditional-extension space: not emitted, not parsed */

    switch ((w >> 25) & 7) {
    case 0:
        if ((w & 0x0FFFFFF0) == 0x012FFF10) {
            sb(b, "bx%s\t%s", cc_sfx[cond], regnm[w & 15]);
            return 1;
        }
        if ((w & 0x0FFFFFF0) == 0x012FFF30) {
            sb(b, "blx%s\t%s", cc_sfx[cond], regnm[w & 15]);
            return 1;
        }
        if ((w & 0x0FFF0FF0) == 0x016F0F10) {
            sb(b, "clz%s\t%s, %s", cc_sfx[cond],
               regnm[(w >> 12) & 15], regnm[w & 15]);
            return 1;
        }
        if ((w & 0x0FC000F0) == 0x00000090)
            return dis_mul(w, b);
        if ((w & 0x0F8000F0) == 0x00800090)
            return dis_mull(w, b);
        if ((w & 0x00000090) == 0x00000090 && (w & 0x60))
            return dis_hmem(w, b);
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
        if (w & 0x01000000) { /* swi/svc */
            uint32_t imm = w & 0xffffff;
            if (imm > 255)
                return 0;
            sb(b, "svc%s\t#%u", cc_sfx[cond], imm);
            return 1;
        }
        if (w & 0x10)
            return dis_mrc(w, b);
        return dis_cdp(w, b);
    }
    return 0;
}

ST_FUNC int mcc_disasm_insn(disasm_ctx *dc)
{
    char line[352];
    SB b;
    uint32_t w;
    int rtype = 0;
    const char *rel;

    if (dc->pc + 4 > dc->size) { /* trailing bytes (unaligned tail) */
        if (!dc->collect)
            fprintf(dc->out, ".byte\t0x%02x", dc->data[dc->pc]);
        return 1;
    }
    w = read32le(dc->data + dc->pc);

    /* a data-word relocation here means this is a literal-pool word, not an
       instruction (ldr rX, [pc]; b .L; .long sym) */
    rel = disasm_reloc(dc, dc->pc, 4, &rtype);
    if (rel && mcc_disasm_reloc_size(rtype) == 4) {
        if (!dc->collect)
            fprintf(dc->out, ".long\t%s", rel);
        return 4;
    }

    sb_init(&b, line, sizeof line);
    if (decode(dc, w, &b)) {
        if (!dc->collect)
            fprintf(dc->out, "%s", line);
    } else {
        /* unknown encoding: mcc's assembler treats .word as 16-bit, so the
           4-byte raw fallback is .long */
        if (!dc->collect)
            fprintf(dc->out, ".long\t0x%08x", w);
    }
    return 4;
}

/* ---- reloc metadata for the arch-independent driver ------------------- */

ST_FUNC int mcc_disasm_reloc_size(int type)
{
    switch (type) {
    case R_ARM_ABS32:
    case R_ARM_REL32:
    case R_ARM_GOT_PREL:
    case R_ARM_GOT32:
    case R_ARM_GOTOFF:
    case R_ARM_TLS_IE32:
    case R_ARM_TLS_LE32:
        return 4;
    /* branch relocations (R_ARM_PC24/CALL/JUMP24/PLT32) patch a 24-bit
       word-count field, not a byte-addressed 4-byte addend: keep them 0 so
       the central in-place addend reader does not misread them. */
    }
    return 0;
}

ST_FUNC int mcc_disasm_reloc_addend_bias(int type, int size)
{
    /* in-place REL addends on ARM read back exactly as written */
    (void)type; (void)size;
    return 0;
}

#endif /* MCC_TARGET_ARM */
