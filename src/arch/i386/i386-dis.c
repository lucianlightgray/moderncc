/*
 * i386-dis.c - x86 (32-bit) instruction disassembler for `-S` output.
 *
 * Decodes the integer + x87 instruction subset that i386-gen.c emits and
 * prints it in AT&T syntax the integrated assembler (i386-asm.c) accepts.
 * Relocated displacement/immediate fields are printed symbolically via
 * disasm_reloc(), so calls and absolute references re-assemble.  TLS
 * local-exec fields (R_386_TLS_LE) are printed with the gas @-operator,
 * `sym@ntpoff`, which i386-asm.c parses back to the same relocation.
 *
 * This is the 32-bit trim of x86_64-dis.c: no REX prefixes, default operand
 * size 32, ModRM mod=0/rm=5 is an absolute disp32 (not RIP-relative), and the
 * FP path is the full x87 block (i386 uses x87, not SSE, for floats).
 *
 * This is not a general-purpose x86 decoder: it targets the encodings mcc
 * produces.  Anything it does not recognise is emitted as `.byte 0xNN` so the
 * listing is always valid and length-correct.
 */
#include "mcc.h"

#ifdef MCC_TARGET_I386

/* ---- decoder state --------------------------------------------------- */

typedef struct {
    disasm_ctx *dc;
    addr_t pc0;       /* instruction start offset */
    int len;          /* bytes consumed so far */
    int opsz;         /* 0x66 present */
    int rep, repne;   /* 0xf3 / 0xf2 present */
    const char *seg;  /* segment override, or NULL */
    /* result of the last modrm decode */
    int reg;          /* /r register number (0..7) */
    char rm[352];     /* r/m operand text (a relocated symbol can be long) */
    int rm_is_mem;
} Dis;

static unsigned char peek(Dis *d, int i)
{
    addr_t o = d->pc0 + d->len + i;
    return (o < d->dc->size) ? d->dc->data[o] : 0;
}
static unsigned char get8(Dis *d)
{
    unsigned char c = peek(d, 0);
    d->len++;
    return c;
}
static int get16(Dis *d) { int v = get8(d); v |= get8(d) << 8; return v; }
static long get32(Dis *d)
{
    unsigned v = get8(d);
    v |= (unsigned)get8(d) << 8;
    v |= (unsigned)get8(d) << 16;
    v |= (unsigned)get8(d) << 24;
    return (int)v; /* sign-extend */
}

/* ---- register names -------------------------------------------------- */

static const char *reg32[8] = {
    "eax","ecx","edx","ebx","esp","ebp","esi","edi" };
static const char *reg16[8] = {
    "ax","cx","dx","bx","sp","bp","si","di" };
static const char *reg8[8] = { "al","cl","dl","bl","ah","ch","dh","bh" };

/* opsize: 1,2,4 bytes */
static const char *gpr(int size, int n)
{
    static char buf[8];
    const char *nm;
    switch (size) {
    case 4: nm = reg32[n & 7]; break;
    case 2: nm = reg16[n & 7]; break;
    default: nm = reg8[n & 7]; break;
    }
    snprintf(buf, sizeof buf, "%%%s", nm);
    return buf;
}

/* gas @-operator spelling for relocation types the plain symbol name would
   lose on re-assembly (a bare `sym` operand re-assembles to R_386_32). */
static const char *reloc_op(int rtype)
{
    return rtype == R_386_TLS_LE ? "@ntpoff" : "";
}

/* ---- ModRM / SIB ----------------------------------------------------- */

/* Decode ModRM (and SIB/disp) for an operand of `size` bytes.  Fills d->reg
   and d->rm[].  mod=0/rm=5 (and SIB base=5/mod=0) is an ABSOLUTE disp32,
   printed symbolically when the field carries a relocation. */
static void modrm(Dis *d, int size)
{
    unsigned char m = get8(d);
    int mod = m >> 6, rm = m & 7;
    d->reg = (m >> 3) & 7;

    if (mod == 3) {
        d->rm_is_mem = 0;
        snprintf(d->rm, sizeof d->rm, "%s", gpr(size, rm));
        return;
    }
    d->rm_is_mem = 1;

    {
        char base[8] = "", index[16] = "";
        int have_base = 1, have_disp = 0, disp_size = 0;
        long disp = 0;
        addr_t disp_off = 0;
        const char *seg = d->seg ? d->seg : "";

        if (rm == 4) { /* SIB */
            unsigned char sib = get8(d);
            int ss = sib >> 6, idx = (sib >> 3) & 7, bse = sib & 7;
            if (idx != 4) /* index==4 (esp) means no index */
                snprintf(index, sizeof index, ",%%%s,%d", reg32[idx], 1 << ss);
            if (bse == 5 && mod == 0) {
                have_base = 0;
                have_disp = 1;
                disp_size = 4;
                disp_off = d->pc0 + d->len;
                disp = get32(d);
            } else {
                snprintf(base, sizeof base, "%%%s", reg32[bse]);
            }
        } else if (rm == 5 && mod == 0) {
            /* absolute disp32 (no RIP-relative on i386) */
            int rtype = 0;
            const char *sym;
            disp_off = d->pc0 + d->len;
            disp = get32(d);
            sym = disasm_reloc(d->dc, disp_off, 4, &rtype);
            if (sym)
                snprintf(d->rm, sizeof d->rm, "%s%s%s", seg, sym,
                         reloc_op(rtype));
            else
                snprintf(d->rm, sizeof d->rm, "%s0x%lx", seg, disp);
            return;
        } else {
            snprintf(base, sizeof base, "%%%s", reg32[rm]);
        }

        if (mod == 1) { have_disp = 1; disp_size = 1; disp_off = d->pc0 + d->len; disp = (signed char)get8(d); }
        else if (mod == 2) { have_disp = 1; disp_size = 4; disp_off = d->pc0 + d->len; disp = get32(d); }

        /* symbolic displacement (absolute reloc)?  Only a 4-byte disp field can
           carry a relocation; querying a 1-byte disp8 with a wider window would
           spuriously match the *next* instruction's relocation. */
        {
            int rtype = 0;
            const char *sym = (have_disp && disp_size == 4)
                            ? disasm_reloc(d->dc, disp_off, 4, &rtype) : NULL;
            char dbuf[288];
            if (sym)
                snprintf(dbuf, sizeof dbuf, "%s%s", sym, reloc_op(rtype));
            else if (have_disp && disp)
                snprintf(dbuf, sizeof dbuf, disp < 0 ? "-0x%lx" : "0x%lx",
                         disp < 0 ? -disp : disp);
            else
                dbuf[0] = 0;

            snprintf(d->rm, sizeof d->rm, "%s%s%s%s%s%s",
                     seg, dbuf,
                     (have_base || index[0]) ? "(" : "",
                     have_base ? base : "",
                     index,
                     (have_base || index[0]) ? ")" : "");
            if (!have_base && !index[0] && !dbuf[0])
                snprintf(d->rm, sizeof d->rm, "%s0x0", seg);
        }
    }
}

/* ---- output helpers -------------------------------------------------- */

static void P(Dis *d, const char *fmt, ...)
{
    va_list ap;
    if (d->dc->collect) /* pass 1 only gathers branch targets */
        return;
    va_start(ap, fmt);
    vfprintf(d->dc->out, fmt, ap);
    va_end(ap);
}

/* size suffix letter for AT&T when an operand is memory/immediate only */
static char sfx(int size)
{
    return size == 4 ? 'l' : size == 2 ? 'w' : 'b';
}

/* operand size from prefixes for the "v" (word/dword) class */
static int vsize(Dis *d) { return d->opsz ? 2 : 4; }

/* Immediate encoded in `size` bytes for an `opsize`-byte operand.  Symbolic
   if the field is relocated.  A sign-extended imm8 in a wider operation is
   printed SIGNED ($-0x1): the integrated assembler classifies immediates by
   value, and only the signed form re-selects the imm8 (0x83/0x6a) encoding.
   Full-width immediates print masked-unsigned, matching objdump. */
static void imm_ext(Dis *d, int size, int opsize, char *out, int outsz)
{
    addr_t off = d->pc0 + d->len;
    int rtype = 0;
    /* only a 4-byte immediate can be relocated; a narrow window on an
       imm8/imm16 could spuriously match a neighbouring instruction's reloc */
    const char *sym = size >= 4 ? disasm_reloc(d->dc, off, size, &rtype) : NULL;
    long v;
    unsigned long mask;
    if (size == 1) v = (signed char)get8(d);
    else if (size == 2) v = (short)get16(d);
    else v = get32(d);
    if (sym) {
        snprintf(out, outsz, "$%s%s", sym, reloc_op(rtype));
        return;
    }
    if (size < opsize && v < 0) {
        snprintf(out, outsz, "$-0x%lx", -v);
        return;
    }
    mask = opsize == 4 ? 0xffffffffUL : opsize == 2 ? 0xffffUL : 0xffUL;
    snprintf(out, outsz, "$0x%lx", (unsigned long)v & mask);
}
/* immediate whose display width equals its encoded width */
static void imm(Dis *d, int size, char *out, int outsz)
{
    imm_ext(d, size, size, out, outsz);
}

/* ---- two-operand ALU (add/or/adc/sbb/and/sub/xor/cmp/mov) ------------ */

static const char *alu8[8] =
    { "add","or","adc","sbb","and","sub","xor","cmp" };

/* print "op src, dst" honoring the direction bit */
static void alu_rm(Dis *d, const char *name, int size, int dbit)
{
    modrm(d, size);
    if (dbit) /* Gv,Ev : reg is dest */
        P(d, "%s\t%s, %s", name, d->rm, gpr(size, d->reg));
    else      /* Ev,Gv : rm is dest */
        P(d, "%s\t%s, %s", name, gpr(size, d->reg), d->rm);
}

/* ---- jcc / setcc / cmovcc suffixes ----------------------------------- */

static const char *cc[16] = {
    "o","no","b","ae","e","ne","be","a",
    "s","ns","p","np","l","ge","le","g" };

/* ---- main decode ----------------------------------------------------- */

static int decode(Dis *d)
{
    unsigned char op;
    char i1[64];
    int size;

    /* prefixes */
    for (;;) {
        unsigned char b = peek(d, 0);
        if (b == 0x66) { d->opsz = 1; d->len++; }
        else if (b == 0xf2) { d->repne = 1; d->len++; }
        else if (b == 0xf3) { d->rep = 1; d->len++; }
        else if (b == 0xf0) { P(d, "lock "); d->len++; }
        else if (b == 0x2e || b == 0x36 || b == 0x3e || b == 0x26) { d->len++; }
        else if (b == 0x64) { d->seg = "%fs:"; d->len++; }
        else if (b == 0x65) { d->seg = "%gs:"; d->len++; }
        else break;
    }

    op = get8(d);

    /* --- ALU family 0x00..0x3d --------------------------------------- */
    if (op < 0x40 && (op & 7) < 6 && (op & 0xc0) == 0) {
        int grp = (op >> 3) & 7;
        int lo = op & 7;
        if (lo < 4) { /* modrm forms */
            size = (lo & 1) ? vsize(d) : 1;
            alu_rm(d, alu8[grp], size, lo & 2);
            return d->len;
        }
        /* 0x04/0x05: AL/eAX, imm */
        size = (lo & 1) ? vsize(d) : 1;
        imm_ext(d, size, size, i1, sizeof i1);
        P(d, "%s\t%s, %s", alu8[grp], i1, gpr(size, 0));
        return d->len;
    }

    /* --- 0x40..0x4f: inc/dec r32 (no REX on i386!) -------------------- */
    if (op >= 0x40 && op <= 0x4f) {
        P(d, "%s\t%s", op < 0x48 ? "inc" : "dec", gpr(vsize(d), op & 7));
        return d->len;
    }

    switch (op) {
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56: case 0x57:
        P(d, "push\t%s", gpr(4, op & 7));
        return d->len;
    case 0x58: case 0x59: case 0x5a: case 0x5b:
    case 0x5c: case 0x5d: case 0x5e: case 0x5f:
        P(d, "pop\t%s", gpr(4, op & 7));
        return d->len;
    case 0x68: /* push imm32 */
        imm(d, 4, i1, sizeof i1);
        P(d, "push\t%s", i1);
        return d->len;
    case 0x6a: /* push imm8 (sign-extended) */
        imm_ext(d, 1, 4, i1, sizeof i1);
        P(d, "push\t%s", i1);
        return d->len;
    case 0x69: /* imul Gv,Ev,imm32 */
        modrm(d, vsize(d));
        imm_ext(d, vsize(d), vsize(d), i1, sizeof i1);
        P(d, "imul\t%s, %s, %s", i1, d->rm, gpr(vsize(d), d->reg));
        return d->len;
    case 0x6b: /* imul Gv,Ev,imm8 */
        modrm(d, vsize(d));
        imm_ext(d, 1, vsize(d), i1, sizeof i1);
        P(d, "imul\t%s, %s, %s", i1, d->rm, gpr(vsize(d), d->reg));
        return d->len;
    case 0x70: case 0x71: case 0x72: case 0x73:
    case 0x74: case 0x75: case 0x76: case 0x77:
    case 0x78: case 0x79: case 0x7a: case 0x7b:
    case 0x7c: case 0x7d: case 0x7e: case 0x7f: {
        int rtype = 0;
        const char *sym;
        addr_t off = d->pc0 + d->len;
        signed char rel = get8(d);
        sym = disasm_reloc(d->dc, off, 1, &rtype);
        if (sym) P(d, "j%s\t%s", cc[op & 15], sym);
        else P(d, "j%s\t%s", cc[op & 15],
               disasm_label(d->dc, d->pc0 + d->len + rel));
        return d->len;
    }
    case 0x80: case 0x81: case 0x83: { /* group1 Ev,imm */
        const char *nm;
        int isz;
        size = (op == 0x80) ? 1 : vsize(d);
        modrm(d, size);
        nm = alu8[d->reg & 7];
        isz = (op == 0x81) ? size : 1;
        imm_ext(d, isz, size, i1, sizeof i1);
        if (d->rm_is_mem)
            P(d, "%s%c\t%s, %s", nm, sfx(size), i1, d->rm);
        else
            P(d, "%s\t%s, %s", nm, i1, d->rm);
        return d->len;
    }
    case 0x84: case 0x85: /* test Ev,Gv */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size);
        P(d, "test\t%s, %s", gpr(size, d->reg), d->rm);
        return d->len;
    case 0x86: case 0x87: /* xchg Ev,Gv */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size);
        P(d, "xchg\t%s, %s", gpr(size, d->reg), d->rm);
        return d->len;
    case 0x88: case 0x89: case 0x8a: case 0x8b: /* mov */
        size = (op & 1) ? vsize(d) : 1;
        alu_rm(d, "mov", size, op & 2);
        return d->len;
    case 0x8d: /* lea */
        modrm(d, vsize(d));
        P(d, "lea\t%s, %s", d->rm, gpr(vsize(d), d->reg));
        return d->len;
    case 0x90:
        if (d->rep) { P(d, "pause"); return d->len; }
        P(d, "nop");
        return d->len;
    case 0x98:
        P(d, d->opsz ? "cbtw" : "cwtl");
        return d->len;
    case 0x99:
        P(d, d->opsz ? "cwtd" : "cltd");
        return d->len;
    case 0xa0: case 0xa1: case 0xa2: case 0xa3: { /* mov moffs32 <-> AL/eAX */
        int rtype = 0;
        const char *sym;
        addr_t off = d->pc0 + d->len;
        long a = get32(d);
        char abuf[288];
        size = (op & 1) ? vsize(d) : 1;
        sym = disasm_reloc(d->dc, off, 4, &rtype);
        if (sym) snprintf(abuf, sizeof abuf, "%s%s", d->seg ? d->seg : "", sym);
        else snprintf(abuf, sizeof abuf, "%s0x%lx", d->seg ? d->seg : "", a);
        if (op & 2) P(d, "mov\t%s, %s", gpr(size, 0), abuf);
        else P(d, "mov\t%s, %s", abuf, gpr(size, 0));
        return d->len;
    }
    case 0xa4: case 0xa5: /* movs */
    case 0xaa: case 0xab: /* stos */
    case 0xac: case 0xad: /* lods */
    case 0xa6: case 0xa7: /* cmps */
    case 0xae: case 0xaf: { /* scas */
        int sz = (op & 1) ? vsize(d) : 1;
        const char *nm = (op < 0xa6) ? "movs"
                       : (op < 0xaa) ? "cmps"
                       : (op < 0xac) ? "stos"
                       : (op < 0xae) ? "lods" : "scas";
        if (d->rep) P(d, "rep ");
        else if (d->repne) P(d, "repnz ");
        P(d, "%s%c", nm, sfx(sz));
        return d->len;
    }
    case 0xa8: case 0xa9: /* test AL/eAX, imm */
        size = (op & 1) ? vsize(d) : 1;
        imm_ext(d, size, size, i1, sizeof i1);
        P(d, "test\t%s, %s", i1, gpr(size, 0));
        return d->len;
    case 0xb0: case 0xb1: case 0xb2: case 0xb3:
    case 0xb4: case 0xb5: case 0xb6: case 0xb7: /* mov r8, imm8 */
        imm(d, 1, i1, sizeof i1);
        P(d, "mov\t%s, %s", i1, gpr(1, op & 7));
        return d->len;
    case 0xb8: case 0xb9: case 0xba: case 0xbb:
    case 0xbc: case 0xbd: case 0xbe: case 0xbf: /* mov rv, imm */
        size = vsize(d);
        imm(d, size, i1, sizeof i1);
        P(d, "mov\t%s, %s", i1, gpr(size, op & 7));
        return d->len;
    case 0xc0: case 0xc1: case 0xd0: case 0xd1:
    case 0xd2: case 0xd3: { /* group2 shifts */
        static const char *sh[8] =
            { "rol","ror","rcl","rcr","shl","shr","sal","sar" };
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size);
        if (op <= 0xc1) {
            imm(d, 1, i1, sizeof i1);
            P(d, "%s%s\t%s, %s", sh[d->reg & 7],
              d->rm_is_mem ? (char[2]){sfx(size),0} : "", i1, d->rm);
        } else if (op <= 0xd1) {
            P(d, "%s%s\t%s", sh[d->reg & 7],
              d->rm_is_mem ? (char[2]){sfx(size),0} : "", d->rm);
        } else {
            P(d, "%s%s\t%%cl, %s", sh[d->reg & 7],
              d->rm_is_mem ? (char[2]){sfx(size),0} : "", d->rm);
        }
        return d->len;
    }
    case 0xc2: /* ret imm16 */
        { int v = get16(d); P(d, "ret\t$0x%x", v); }
        return d->len;
    case 0xc3:
        P(d, "ret");
        return d->len;
    case 0xc6: case 0xc7: /* mov Ev, imm */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size);
        imm_ext(d, size, size, i1, sizeof i1);
        if (d->rm_is_mem)
            P(d, "mov%c\t%s, %s", sfx(size), i1, d->rm);
        else
            P(d, "mov\t%s, %s", i1, d->rm);
        return d->len;
    case 0xc9:
        P(d, "leave");
        return d->len;
    case 0xcc:
        P(d, "int3");
        return d->len;
    case 0xe8: case 0xe9: { /* call/jmp rel32 */
        int rtype = 0;
        const char *sym;
        addr_t off = d->pc0 + d->len;
        long rel = get32(d);
        sym = disasm_reloc(d->dc, off, 4, &rtype);
        if (sym) P(d, "%s\t%s", op == 0xe8 ? "call" : "jmp", sym);
        else P(d, "%s\t%s", op == 0xe8 ? "call" : "jmp",
               disasm_label(d->dc, d->pc0 + d->len + rel));
        return d->len;
    }
    case 0xeb: { /* jmp rel8 */
        signed char rel = get8(d);
        P(d, "jmp\t%s", disasm_label(d->dc, d->pc0 + d->len + rel));
        return d->len;
    }
    case 0xf6: case 0xf7: { /* group3 */
        static const char *g3[8] =
            { "test","test","not","neg","mul","imul","div","idiv" };
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size);
        if ((d->reg & 7) < 2) { /* test Ev, imm */
            imm_ext(d, size, size, i1, sizeof i1);
            if (d->rm_is_mem) P(d, "test%c\t%s, %s", sfx(size), i1, d->rm);
            else P(d, "test\t%s, %s", i1, d->rm);
        } else {
            P(d, "%s%s\t%s", g3[d->reg & 7],
              d->rm_is_mem ? (char[2]){sfx(size),0} : "", d->rm);
        }
        return d->len;
    }
    case 0xfe: case 0xff: { /* group4/5 */
        int r;
        size = (op == 0xfe) ? 1 : vsize(d);
        modrm(d, size);
        r = d->reg & 7;
        if (op == 0xfe) {
            P(d, "%s%s\t%s", r == 0 ? "inc" : "dec",
              d->rm_is_mem ? (char[2]){sfx(size),0} : "", d->rm);
            return d->len;
        }
        switch (r) {
        case 0: P(d, "inc%s\t%s", d->rm_is_mem ? (char[2]){sfx(size),0} : "", d->rm); break;
        case 1: P(d, "dec%s\t%s", d->rm_is_mem ? (char[2]){sfx(size),0} : "", d->rm); break;
        case 2: P(d, "call\t*%s", d->rm); break;
        case 4: P(d, "jmp\t*%s", d->rm); break;
        case 6: P(d, "push\t%s", d->rm); break;
        default: P(d, "(bad)"); break;
        }
        return d->len;
    }
    case 0xd8: case 0xd9: case 0xda: case 0xdb:
    case 0xdc: case 0xdd: case 0xde: case 0xdf: { /* x87 FPU */
        unsigned char mb = peek(d, 0);
        int rf = (mb >> 3) & 7;
        if ((mb >> 6) == 3) {
            /* register / no-operand form: opcode + modrm (2 bytes total).
               NOTE: fsub/fsubr and fdiv/fdivr follow the integrated
               assembler's (Intel-order) naming, which modern binutils also
               uses; the historical AT&T name swap does not apply here. */
            static const char *st[8] =
                { "%st(0)","%st(1)","%st(2)","%st(3)",
                  "%st(4)","%st(5)","%st(6)","%st(7)" };
            int r = mb & 7;
            get8(d); /* consume modrm */
            switch (op) {
            case 0xd9:
                if (mb >= 0xc0 && mb <= 0xc7) P(d, "fld\t%s", st[r]);
                else if (mb >= 0xc8 && mb <= 0xcf) P(d, "fxch\t%s", st[r]);
                else switch (mb) {
                    case 0xe0: P(d, "fchs"); break;
                    case 0xe1: P(d, "fabs"); break;
                    case 0xe8: P(d, "fld1"); break;
                    case 0xee: P(d, "fldz"); break;
                    case 0xf8: P(d, "fprem"); break;
                    case 0xfa: P(d, "fsqrt"); break;
                    case 0xfc: P(d, "frndint"); break;
                    case 0xd0: P(d, "fnop"); break;
                    default: P(d, ".byte\t0x%02x, 0x%02x", op, mb); break;
                }
                break;
            case 0xd8:
                if (mb <= 0xc7) P(d, "fadd\t%s", st[r]);
                else if (mb <= 0xcf) P(d, "fmul\t%s", st[r]);
                else if (mb <= 0xd7) P(d, "fcom\t%s", st[r]);
                else if (mb <= 0xdf) P(d, "fcomp\t%s", st[r]);
                else if (mb <= 0xe7) P(d, "fsub\t%s", st[r]);
                else if (mb <= 0xef) P(d, "fsubr\t%s", st[r]);
                else if (mb <= 0xf7) P(d, "fdiv\t%s", st[r]);
                else P(d, "fdivr\t%s", st[r]);
                break;
            case 0xdc:
                if (mb <= 0xc7) P(d, "fadd\t%%st, %s", st[r]);
                else if (mb <= 0xcf) P(d, "fmul\t%%st, %s", st[r]);
                else if (mb >= 0xe0 && mb <= 0xe7) P(d, "fsub\t%%st, %s", st[r]);
                else if (mb >= 0xe8 && mb <= 0xef) P(d, "fsubr\t%%st, %s", st[r]);
                else if (mb >= 0xf0 && mb <= 0xf7) P(d, "fdiv\t%%st, %s", st[r]);
                else if (mb >= 0xf8) P(d, "fdivr\t%%st, %s", st[r]);
                else P(d, ".byte\t0x%02x, 0x%02x", op, mb);
                break;
            case 0xdd:
                if (mb >= 0xd8 && mb <= 0xdf) P(d, "fstp\t%s", st[r]);
                else if (mb >= 0xd0 && mb <= 0xd7) P(d, "fst\t%s", st[r]);
                else if (mb <= 0xc7) P(d, "ffree\t%s", st[r]);
                else if (mb >= 0xe0 && mb <= 0xe7) P(d, "fucom\t%s", st[r]);
                else if (mb >= 0xe8 && mb <= 0xef) P(d, "fucomp\t%s", st[r]);
                else P(d, ".byte\t0x%02x, 0x%02x", op, mb);
                break;
            case 0xde:
                if (mb <= 0xc7) P(d, "faddp\t%s", st[r]);
                else if (mb <= 0xcf) P(d, "fmulp\t%s", st[r]);
                else if (mb == 0xd9) P(d, "fcompp");
                else if (mb >= 0xe0 && mb <= 0xe7) P(d, "fsubp\t%s", st[r]);
                else if (mb >= 0xe8 && mb <= 0xef) P(d, "fsubrp\t%s", st[r]);
                else if (mb >= 0xf0 && mb <= 0xf7) P(d, "fdivp\t%s", st[r]);
                else if (mb >= 0xf8) P(d, "fdivrp\t%s", st[r]);
                else P(d, ".byte\t0x%02x, 0x%02x", op, mb);
                break;
            case 0xda:
                if (mb == 0xe9) P(d, "fucompp");
                else if (mb <= 0xc7) P(d, "fcmovb\t%s, %%st", st[r]);
                else if (mb <= 0xcf) P(d, "fcmove\t%s, %%st", st[r]);
                else if (mb <= 0xd7) P(d, "fcmovbe\t%s, %%st", st[r]);
                else if (mb <= 0xdf) P(d, "fcmovu\t%s, %%st", st[r]);
                else P(d, ".byte\t0x%02x, 0x%02x", op, mb);
                break;
            case 0xdb:
                if (mb == 0xe2) P(d, "fnclex");
                else if (mb == 0xe3) P(d, "fninit");
                else if (mb >= 0xc0 && mb <= 0xc7) P(d, "fcmovnb\t%s, %%st", st[r]);
                else if (mb >= 0xc8 && mb <= 0xcf) P(d, "fcmovne\t%s, %%st", st[r]);
                else if (mb >= 0xd0 && mb <= 0xd7) P(d, "fcmovnbe\t%s, %%st", st[r]);
                else if (mb >= 0xd8 && mb <= 0xdf) P(d, "fcmovnu\t%s, %%st", st[r]);
                else if (mb >= 0xe8 && mb <= 0xef) P(d, "fucomi\t%s, %%st", st[r]);
                else if (mb >= 0xf0 && mb <= 0xf7) P(d, "fcomi\t%s, %%st", st[r]);
                else P(d, ".byte\t0x%02x, 0x%02x", op, mb);
                break;
            case 0xdf:
                if (mb == 0xe0) P(d, "fnstsw\t%%ax");
                else if (mb >= 0xe8 && mb <= 0xef) P(d, "fucomip\t%s, %%st", st[r]);
                else if (mb >= 0xf0 && mb <= 0xf7) P(d, "fcomip\t%s, %%st", st[r]);
                else P(d, ".byte\t0x%02x, 0x%02x", op, mb);
                break;
            default:
                P(d, ".byte\t0x%02x, 0x%02x", op, mb);
                break;
            }
            return d->len;
        }
        /* memory operand form: mnemonic by (opcode, reg field) */
        {
            static const char *m_d8[8] = {"fadds","fmuls","fcoms","fcomps","fsubs","fsubrs","fdivs","fdivrs"};
            static const char *m_d9[8] = {"flds",0,"fsts","fstps","fldenv","fldcw","fnstenv","fnstcw"};
            static const char *m_da[8] = {"fiaddl","fimull","ficoml","ficompl","fisubl","fisubrl","fidivl","fidivrl"};
            static const char *m_db[8] = {"fildl","fisttpl","fistl","fistpl",0,"fldt",0,"fstpt"};
            static const char *m_dc[8] = {"faddl","fmull","fcoml","fcompl","fsubl","fsubrl","fdivl","fdivrl"};
            static const char *m_dd[8] = {"fldl","fisttpll","fstl","fstpl","frstor",0,"fnsave","fnstsw"};
            static const char *m_de[8] = {"fiadds","fimuls","ficoms","ficomps","fisubs","fisubrs","fidivs","fidivrs"};
            static const char *m_df[8] = {"filds","fisttps","fists","fistps","fbld","fildll","fbstp","fistpll"};
            static const char **tab[8] = { m_d8,m_d9,m_da,m_db,m_dc,m_dd,m_de,m_df };
            const char *nm = tab[op - 0xd8][rf];
            modrm(d, 4); /* decode length; size unused for x87 mem */
            if (nm) P(d, "%s\t%s", nm, d->rm);
            else P(d, "fx87.%x/%d\t%s", op, rf, d->rm);
        }
        return d->len;
    }
    case 0x0f:
        break; /* two-byte, handled below */
    default:
        P(d, ".byte\t0x%02x", op);
        return d->len;
    }

    /* --- two-byte 0x0F ------------------------------------------------ */
    op = get8(d);
    switch (op) {
    case 0x40: case 0x41: case 0x42: case 0x43:
    case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x48: case 0x49: case 0x4a: case 0x4b:
    case 0x4c: case 0x4d: case 0x4e: case 0x4f: /* cmovcc */
        modrm(d, vsize(d));
        P(d, "cmov%s\t%s, %s", cc[op & 15], d->rm, gpr(vsize(d), d->reg));
        return d->len;
    case 0x80: case 0x81: case 0x82: case 0x83:
    case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8a: case 0x8b:
    case 0x8c: case 0x8d: case 0x8e: case 0x8f: { /* jcc rel32 */
        int rtype = 0;
        const char *sym;
        addr_t off = d->pc0 + d->len;
        long rel = get32(d);
        sym = disasm_reloc(d->dc, off, 4, &rtype);
        if (sym) P(d, "j%s\t%s", cc[op & 15], sym);
        else P(d, "j%s\t%s", cc[op & 15],
               disasm_label(d->dc, d->pc0 + d->len + rel));
        return d->len;
    }
    case 0x90: case 0x91: case 0x92: case 0x93:
    case 0x94: case 0x95: case 0x96: case 0x97:
    case 0x98: case 0x99: case 0x9a: case 0x9b:
    case 0x9c: case 0x9d: case 0x9e: case 0x9f: /* setcc */
        modrm(d, 1);
        P(d, "set%s\t%s", cc[op & 15], d->rm);
        return d->len;
    case 0xa2: P(d, "cpuid"); return d->len;
    case 0xa3: modrm(d, vsize(d)); P(d, "bt\t%s, %s", gpr(vsize(d), d->reg), d->rm); return d->len;
    case 0xaf: /* imul Gv, Ev */
        modrm(d, vsize(d));
        P(d, "imul\t%s, %s", d->rm, gpr(vsize(d), d->reg));
        return d->len;
    case 0xb0: case 0xb1: /* cmpxchg */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size);
        P(d, "cmpxchg\t%s, %s", gpr(size, d->reg), d->rm);
        return d->len;
    case 0xb6: case 0xb7: /* movzx */
        modrm(d, op == 0xb6 ? 1 : 2);
        P(d, "movz%c%c\t%s, %s", op == 0xb6 ? 'b' : 'w', sfx(vsize(d)),
          d->rm, gpr(vsize(d), d->reg));
        return d->len;
    case 0xbe: case 0xbf: /* movsx */
        modrm(d, op == 0xbe ? 1 : 2);
        P(d, "movs%c%c\t%s, %s", op == 0xbe ? 'b' : 'w', sfx(vsize(d)),
          d->rm, gpr(vsize(d), d->reg));
        return d->len;
    case 0xc0: case 0xc1: /* xadd */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size);
        P(d, "xadd\t%s, %s", gpr(size, d->reg), d->rm);
        return d->len;
    default:
        P(d, ".byte\t0x0f, 0x%02x", op);
        return d->len;
    }
}

ST_FUNC int mcc_disasm_insn(disasm_ctx *dc)
{
    Dis d;
    memset(&d, 0, sizeof d);
    d.dc = dc;
    d.pc0 = dc->pc;
    return decode(&d);
}

/* ---- reloc metadata for the arch-independent driver ------------------ */

ST_FUNC int mcc_disasm_reloc_size(int type)
{
    switch (type) {
    case R_386_32:
    case R_386_PC32:
    case R_386_PLT32:
    case R_386_GOT32:
    case R_386_GOT32X:
    case R_386_GOTOFF:
    case R_386_GOTPC:
    case R_386_TLS_GD:
    case R_386_TLS_LDM:
    case R_386_TLS_LDO_32:
    case R_386_TLS_LE:
        return 4;
    case R_386_16:
    case R_386_PC16:
        return 2;
    }
    return 0;
}

ST_FUNC int mcc_disasm_reloc_addend_bias(int type, int size)
{
    /* i386 is a REL target: the addend lives in the patched field itself and
       disasm_reloc() reads it back in place.  PC-relative fields carry
       -(field-to-insn-end), i.e. -4 for `call sym`; gas re-derives that, so
       fold it out for a clean "sym" operand. */
    if (type == R_386_PC32 || type == R_386_PLT32 || type == R_386_GOTPC)
        return size;
    return 0;
}

#endif /* MCC_TARGET_I386 */
