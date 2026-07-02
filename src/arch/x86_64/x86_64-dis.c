/*
 * x86_64-dis.c - x86-64 instruction disassembler for `-S` output.
 *
 * Decodes the integer + common-SSE instruction subset that x86_64-gen.c emits
 * and prints it in AT&T syntax (matching `gcc -S` / objdump).  Relocated
 * displacement/immediate fields are printed symbolically via disasm_reloc(),
 * so calls, rip-relative accesses and absolute references re-assemble.
 *
 * This is not a general-purpose x86 decoder: it targets the encodings mcc
 * produces.  Anything it does not recognise is emitted as `.byte 0xNN` so the
 * listing is always valid and length-correct.
 */
#include "mcc.h"

#ifdef MCC_TARGET_X86_64

/* ---- decoder state --------------------------------------------------- */

typedef struct {
    disasm_ctx *dc;
    addr_t pc0;       /* instruction start offset */
    int len;          /* bytes consumed so far */
    int rex, rex_w, rex_r, rex_x, rex_b;
    int opsz;         /* 0x66 present */
    int rep, repne;   /* 0xf3 / 0xf2 present */
    int addr32;       /* 0x67 present */
    const char *seg;  /* segment override, or NULL */
    /* result of the last modrm decode */
    int reg;          /* /r register number (0..15, REX.R applied) */
    char rm[80];      /* r/m operand text (register or memory) */
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
static long long get32(Dis *d)
{
    unsigned v = get8(d);
    v |= (unsigned)get8(d) << 8;
    v |= (unsigned)get8(d) << 16;
    v |= (unsigned)get8(d) << 24;
    return (int)v; /* sign-extend */
}
static long long get64(Dis *d)
{
    unsigned long long lo = (unsigned)get32(d) & 0xffffffffu;
    unsigned long long hi = (unsigned)get32(d) & 0xffffffffu;
    return (long long)(lo | (hi << 32));
}

/* ---- register names -------------------------------------------------- */

static const char *reg64[16] = {
    "rax","rcx","rdx","rbx","rsp","rbp","rsi","rdi",
    "r8","r9","r10","r11","r12","r13","r14","r15" };
static const char *reg32[16] = {
    "eax","ecx","edx","ebx","esp","ebp","esi","edi",
    "r8d","r9d","r10d","r11d","r12d","r13d","r14d","r15d" };
static const char *reg16[16] = {
    "ax","cx","dx","bx","sp","bp","si","di",
    "r8w","r9w","r10w","r11w","r12w","r13w","r14w","r15w" };
static const char *reg8rex[16] = {
    "al","cl","dl","bl","spl","bpl","sil","dil",
    "r8b","r9b","r10b","r11b","r12b","r13b","r14b","r15b" };
static const char *reg8[8] = { "al","cl","dl","bl","ah","ch","dh","bh" };

/* opsize: 1,2,4,8 bytes.  n already includes REX.R/B expansion. */
static const char *gpr(Dis *d, int size, int n)
{
    static char buf[8];
    const char *nm;
    switch (size) {
    case 8: nm = reg64[n & 15]; break;
    case 4: nm = reg32[n & 15]; break;
    case 2: nm = reg16[n & 15]; break;
    default:
        nm = d->rex ? reg8rex[n & 15] : reg8[n & 7];
        break;
    }
    snprintf(buf, sizeof buf, "%%%s", nm);
    return buf;
}
static const char *xmm(int n)
{
    static char buf[8];
    snprintf(buf, sizeof buf, "%%xmm%d", n & 15);
    return buf;
}

/* ---- ModRM / SIB ----------------------------------------------------- */

/* Decode ModRM (and SIB/disp) for an operand of `size` bytes.  Fills d->reg
   and d->rm[].  `xmm_rm` selects xmm register naming for the r/m when it is a
   register.  Handles rip-relative and symbolic (relocated) displacements. */
static void modrm(Dis *d, int size, int xmm_rm)
{
    unsigned char m = get8(d);
    int mod = m >> 6, rm = m & 7, reg = (m >> 3) & 7;
    d->reg = reg | (d->rex_r ? 8 : 0);

    if (mod == 3) {
        int r = rm | (d->rex_b ? 8 : 0);
        d->rm_is_mem = 0;
        snprintf(d->rm, sizeof d->rm, "%s", xmm_rm ? xmm(r) : gpr(d, size, r));
        return;
    }
    d->rm_is_mem = 1;

    {
        char base[8] = "", index[16] = "";
        int have_base = 1, have_disp = 0, disp_size = 0;
        long long disp = 0;
        int scale = 0;
        addr_t disp_off = 0;

        if (rm == 4) { /* SIB */
            unsigned char sib = get8(d);
            int ss = sib >> 6, idx = (sib >> 3) & 7, bse = sib & 7;
            int ir = idx | (d->rex_x ? 8 : 0);
            int br = bse | (d->rex_b ? 8 : 0);
            scale = 1 << ss;
            if (ir != 4) /* index==4 (rsp) means no index */
                snprintf(index, sizeof index, ",%s,%d", gpr(d, 8, ir), scale);
            if (bse == 5 && mod == 0) {
                have_base = 0;
                have_disp = 1;
                disp_size = 4;
                disp_off = d->pc0 + d->len;
                disp = get32(d);
            } else {
                snprintf(base, sizeof base, "%s", gpr(d, 8, br));
            }
        } else if (rm == 5 && mod == 0) {
            /* rip-relative */
            int rtype = 0;
            const char *sym;
            disp_off = d->pc0 + d->len;
            disp = get32(d);
            sym = disasm_reloc(d->dc, disp_off, 4, &rtype);
            if (sym)
                snprintf(d->rm, sizeof d->rm, "%s(%%rip)", sym);
            else
                snprintf(d->rm, sizeof d->rm, "0x%llx(%%rip)", disp);
            return;
        } else {
            snprintf(base, sizeof base, "%s", gpr(d, 8, rm | (d->rex_b ? 8 : 0)));
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
            char dbuf[64];
            if (sym)
                snprintf(dbuf, sizeof dbuf, "%s", sym);
            else if (have_disp && disp)
                snprintf(dbuf, sizeof dbuf, disp < 0 ? "-0x%llx" : "0x%llx",
                         disp < 0 ? -disp : disp);
            else
                dbuf[0] = 0;

            snprintf(d->rm, sizeof d->rm, "%s%s%s%s%s",
                     dbuf,
                     (have_base || index[0]) ? "(" : "",
                     have_base ? base : "",
                     index,
                     (have_base || index[0]) ? ")" : "");
            if (!have_base && !index[0] && !dbuf[0])
                snprintf(d->rm, sizeof d->rm, "0x0");
        }
        (void)scale;
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
    return size == 8 ? 'q' : size == 4 ? 'l' : size == 2 ? 'w' : 'b';
}

/* operand size from prefixes for the "v" (word/dword/qword) class */
static int vsize(Dis *d) { return d->rex_w ? 8 : d->opsz ? 2 : 4; }

/* Immediate encoded in `size` bytes, sign-extended and displayed masked to the
   `opsize`-byte operand (so an imm8 of 0xff in a 32-bit op prints as
   $0xffffffff, matching objdump and re-assembling to the same encoding).
   Symbolic if the field is relocated. */
static void imm_ext(Dis *d, int size, int opsize, char *out, int outsz)
{
    addr_t off = d->pc0 + d->len;
    int rtype = 0;
    /* only a 4- or 8-byte immediate can be relocated; a narrow window on an
       imm8/imm16 could spuriously match a neighbouring instruction's reloc */
    const char *sym = size >= 4 ? disasm_reloc(d->dc, off, size, &rtype) : NULL;
    long long v;
    unsigned long long mask;
    if (size == 1) v = (signed char)get8(d);
    else if (size == 2) v = (short)get16(d);
    else if (size == 8) v = get64(d);
    else v = get32(d);
    if (sym) {
        snprintf(out, outsz, "$%s", sym);
        return;
    }
    mask = opsize >= 8 ? ~0ULL : opsize == 4 ? 0xffffffffULL
         : opsize == 2 ? 0xffffULL : 0xffULL;
    snprintf(out, outsz, "$0x%llx", (unsigned long long)v & mask);
}
/* immediate whose display width equals its encoded width */
static void imm(Dis *d, int size, char *out, int outsz)
{
    imm_ext(d, size, size, out, outsz);
}

/* ---- two-operand ALU (add/or/adc/sbb/and/sub/xor/cmp/mov/test) ------- */

static const char *alu8[8] =
    { "add","or","adc","sbb","and","sub","xor","cmp" };

/* print "op src, dst" honoring the direction bit */
static void alu_rm(Dis *d, const char *name, int size, int dbit)
{
    modrm(d, size, 0);
    if (dbit) /* Gv,Ev : reg is dest */
        P(d, "%s\t%s, %s", name, d->rm, gpr(d, size, d->reg));
    else      /* Ev,Gv : rm is dest */
        P(d, "%s\t%s, %s", name, gpr(d, size, d->reg), d->rm);
}

/* ---- jcc / setcc / cmovcc suffixes ----------------------------------- */

static const char *cc[16] = {
    "o","no","b","ae","e","ne","be","a",
    "s","ns","p","np","l","ge","le","g" };

/* ---- main decode ----------------------------------------------------- */

static int decode(Dis *d)
{
    unsigned char op;
    char i1[64], i2[64];
    int size;

    /* prefixes */
    for (;;) {
        unsigned char b = peek(d, 0);
        if (b == 0x66) { d->opsz = 1; d->len++; }
        else if (b == 0x67) { d->addr32 = 1; d->len++; }
        else if (b == 0xf2) { d->repne = 1; d->len++; }
        else if (b == 0xf3) { d->rep = 1; d->len++; }
        else if (b == 0xf0) { P(d, "lock "); d->len++; }
        else if (b == 0x2e || b == 0x36 || b == 0x3e || b == 0x26) { d->len++; }
        else if (b == 0x64) { d->seg = "%fs:"; d->len++; }
        else if (b == 0x65) { d->seg = "%gs:"; d->len++; }
        else break;
    }
    /* REX must be last prefix before opcode */
    {
        unsigned char b = peek(d, 0);
        if ((b & 0xf0) == 0x40) {
            d->rex = 1;
            d->rex_w = (b >> 3) & 1;
            d->rex_r = (b >> 2) & 1;
            d->rex_x = (b >> 1) & 1;
            d->rex_b = b & 1;
            d->len++;
        }
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
        imm_ext(d, size == 8 ? 4 : size, size, i1, sizeof i1);
        P(d, "%s\t%s, %s", alu8[grp], i1, gpr(d, size, 0));
        return d->len;
    }

    switch (op) {
    case 0x50: case 0x51: case 0x52: case 0x53:
    case 0x54: case 0x55: case 0x56: case 0x57:
        P(d, "push\t%s", gpr(d, 8, (op & 7) | (d->rex_b ? 8 : 0)));
        return d->len;
    case 0x58: case 0x59: case 0x5a: case 0x5b:
    case 0x5c: case 0x5d: case 0x5e: case 0x5f:
        P(d, "pop\t%s", gpr(d, 8, (op & 7) | (d->rex_b ? 8 : 0)));
        return d->len;
    case 0x63: /* movslq Ed,Gq */
        modrm(d, 4, 0);
        P(d, "movslq\t%s, %s", d->rm, gpr(d, 8, d->reg));
        return d->len;
    case 0x68: /* push imm32 */
        imm(d, 4, i1, sizeof i1);
        P(d, "push\t%s", i1);
        return d->len;
    case 0x6a: /* push imm8 */
        imm(d, 1, i1, sizeof i1);
        P(d, "push\t%s", i1);
        return d->len;
    case 0x69: /* imul Gv,Ev,imm32 */
        modrm(d, vsize(d), 0);
        imm_ext(d, vsize(d) == 8 ? 4 : vsize(d), vsize(d), i1, sizeof i1);
        P(d, "imul\t%s, %s, %s", i1, d->rm, gpr(d, vsize(d), d->reg));
        return d->len;
    case 0x6b: /* imul Gv,Ev,imm8 */
        modrm(d, vsize(d), 0);
        imm_ext(d, 1, vsize(d), i1, sizeof i1);
        P(d, "imul\t%s, %s, %s", i1, d->rm, gpr(d, vsize(d), d->reg));
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
        modrm(d, size, 0);
        nm = alu8[d->reg & 7];
        isz = (op == 0x81) ? (size == 8 ? 4 : size) : 1;
        imm_ext(d, isz, size, i1, sizeof i1);
        if (d->rm_is_mem)
            P(d, "%s%c\t%s, %s", nm, sfx(size), i1, d->rm);
        else
            P(d, "%s\t%s, %s", nm, i1, d->rm);
        return d->len;
    }
    case 0x84: case 0x85: /* test Ev,Gv */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size, 0);
        P(d, "test\t%s, %s", gpr(d, size, d->reg), d->rm);
        return d->len;
    case 0x86: case 0x87: /* xchg Ev,Gv */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size, 0);
        P(d, "xchg\t%s, %s", gpr(d, size, d->reg), d->rm);
        return d->len;
    case 0x88: case 0x89: case 0x8a: case 0x8b: /* mov */
        size = (op & 1) ? vsize(d) : 1;
        alu_rm(d, "mov", size, op & 2);
        return d->len;
    case 0x8d: /* lea */
        modrm(d, vsize(d), 0);
        P(d, "lea\t%s, %s", d->rm, gpr(d, vsize(d), d->reg));
        return d->len;
    case 0x90:
        if (d->rep) { P(d, "pause"); return d->len; }
        P(d, "nop");
        return d->len;
    case 0x98:
        P(d, d->rex_w ? "cltq" : d->opsz ? "cbtw" : "cwtl");
        return d->len;
    case 0x99:
        P(d, d->rex_w ? "cqto" : d->opsz ? "cwtd" : "cltd");
        return d->len;
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
        imm_ext(d, size == 8 ? 4 : size, size, i1, sizeof i1);
        P(d, "test\t%s, %s", i1, gpr(d, size, 0));
        return d->len;
    case 0xb0: case 0xb1: case 0xb2: case 0xb3:
    case 0xb4: case 0xb5: case 0xb6: case 0xb7: /* mov r8, imm8 */
        imm(d, 1, i1, sizeof i1);
        P(d, "mov\t%s, %s", i1, gpr(d, 1, (op & 7) | (d->rex_b ? 8 : 0)));
        return d->len;
    case 0xb8: case 0xb9: case 0xba: case 0xbb:
    case 0xbc: case 0xbd: case 0xbe: case 0xbf: /* mov rv, imm */
        size = vsize(d);
        imm(d, size, i1, sizeof i1);
        P(d, "%s\t%s, %s", size == 8 ? "movabs" : "mov", i1,
          gpr(d, size, (op & 7) | (d->rex_b ? 8 : 0)));
        return d->len;
    case 0xc0: case 0xc1: case 0xd0: case 0xd1:
    case 0xd2: case 0xd3: { /* group2 shifts */
        static const char *sh[8] =
            { "rol","ror","rcl","rcr","shl","shr","sal","sar" };
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size, 0);
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
        i2[0] = 0; { int v = get16(d); P(d, "ret\t$0x%x", v); }
        return d->len;
    case 0xc3:
        P(d, "ret");
        return d->len;
    case 0xc6: case 0xc7: /* mov Ev, imm */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size, 0);
        imm_ext(d, size == 8 ? 4 : size, size, i1, sizeof i1);
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
        long long rel = get32(d);
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
        modrm(d, size, 0);
        if ((d->reg & 7) < 2) { /* test Ev, imm */
            imm_ext(d, size == 8 ? 4 : size, size, i1, sizeof i1);
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
        /* call/jmp/push (/2../6) take a 64-bit operand in long mode regardless
           of REX.W; only inc/dec (/0,/1) follow the prefix size. */
        r = (peek(d, 0) >> 3) & 7;
        if (op == 0xfe) size = 1;
        else size = (r <= 1) ? vsize(d) : 8;
        modrm(d, size, 0);
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
            /* register / no-operand form: opcode + modrm (2 bytes total) */
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
                    default: P(d, "fnop"); break;
                }
                break;
            case 0xd8:
                if (mb <= 0xc7) P(d, "fadd\t%s", st[r]);
                else if (mb <= 0xcf) P(d, "fmul\t%s", st[r]);
                else if (mb <= 0xe7) P(d, "fsub\t%s", st[r]);
                else if (mb <= 0xef) P(d, "fsubr\t%s", st[r]);
                else if (mb <= 0xf7) P(d, "fdiv\t%s", st[r]);
                else P(d, "fdivr\t%s", st[r]);
                break;
            case 0xdc:
                if (mb <= 0xc7) P(d, "fadd\t%%st, %s", st[r]);
                else if (mb <= 0xcf) P(d, "fmul\t%%st, %s", st[r]);
                else if (mb <= 0xef) P(d, "fsubr\t%%st, %s", st[r]);
                else if (mb <= 0xf7) P(d, "fsub\t%%st, %s", st[r]);
                else if (mb <= 0xff) P(d, "fdivr\t%%st, %s", st[r]);
                else P(d, "fdiv\t%%st, %s", st[r]);
                break;
            case 0xdd:
                if (mb >= 0xd8 && mb <= 0xdf) P(d, "fstp\t%s", st[r]);
                else if (mb <= 0xc7) P(d, "ffree\t%s", st[r]);
                else P(d, "fucom\t%s", st[r]);
                break;
            case 0xde:
                if (mb <= 0xc7) P(d, "faddp\t%s", st[r]);
                else if (mb <= 0xcf) P(d, "fmulp\t%s", st[r]);
                else if (mb == 0xd9) P(d, "fcompp");
                else if (mb <= 0xef) P(d, "fsubrp\t%s", st[r]);
                else if (mb <= 0xf7) P(d, "fsubp\t%s", st[r]);
                else if (mb <= 0xff) P(d, "fdivrp\t%s", st[r]);
                else P(d, "fdivp\t%s", st[r]);
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
            modrm(d, 4, 0); /* decode length; size unused for x87 mem */
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
    case 0x05: P(d, "syscall"); return d->len;
    case 0x1e: /* endbr64 (f3 0f 1e fa) */
        if (d->rep && peek(d, 0) == 0xfa) { get8(d); P(d, "endbr64"); return d->len; }
        modrm(d, vsize(d), 0); P(d, "nop\t%s", d->rm); return d->len;
    case 0x1f: /* multi-byte nop */
        modrm(d, vsize(d), 0);
        P(d, "nop%c\t%s", sfx(vsize(d)), d->rm);
        return d->len;
    case 0x10: case 0x11: { /* movups/movupd/movss/movsd */
        const char *nm = d->rep ? "movss" : d->repne ? "movsd"
                        : d->opsz ? "movupd" : "movups";
        modrm(d, 8, 1);
        if (op == 0x10) P(d, "%s\t%s, %s", nm, d->rm, xmm(d->reg));
        else P(d, "%s\t%s, %s", nm, xmm(d->reg), d->rm);
        return d->len;
    }
    case 0x12: case 0x13: case 0x16: case 0x17: {
        const char *nm = (op < 0x16)
            ? (d->opsz ? "movlpd" : "movlps")
            : (d->opsz ? "movhpd" : "movhps");
        modrm(d, 8, 1);
        if (op & 1) P(d, "%s\t%s, %s", nm, xmm(d->reg), d->rm);
        else P(d, "%s\t%s, %s", nm, d->rm, xmm(d->reg));
        return d->len;
    }
    case 0x14: case 0x15: { /* unpcklps/unpckhps */
        const char *nm = op == 0x14 ? (d->opsz?"unpcklpd":"unpcklps")
                                    : (d->opsz?"unpckhpd":"unpckhps");
        modrm(d, 8, 1); P(d, "%s\t%s, %s", nm, d->rm, xmm(d->reg));
        return d->len;
    }
    case 0x28: case 0x29: { /* movaps/movapd */
        const char *nm = d->opsz ? "movapd" : "movaps";
        modrm(d, 8, 1);
        if (op == 0x28) P(d, "%s\t%s, %s", nm, d->rm, xmm(d->reg));
        else P(d, "%s\t%s, %s", nm, xmm(d->reg), d->rm);
        return d->len;
    }
    case 0x2a: { /* cvtsi2ss/sd */
        const char *nm = d->rep ? "cvtsi2ss" : d->repne ? "cvtsi2sd" : "cvtpi2ps";
        modrm(d, vsize(d), 0);
        P(d, "%s%s\t%s, %s", nm, d->rm_is_mem ? (char[2]){sfx(vsize(d)),0} : "",
          d->rm, xmm(d->reg));
        return d->len;
    }
    case 0x2c: case 0x2d: { /* cvttss2si / cvtss2si */
        const char *nm = op == 0x2c
            ? (d->rep ? "cvttss2si" : d->repne ? "cvttsd2si" : "cvttps2pi")
            : (d->rep ? "cvtss2si" : d->repne ? "cvtsd2si" : "cvtps2pi");
        modrm(d, 8, 1);
        P(d, "%s\t%s, %s", nm, d->rm, gpr(d, vsize(d), d->reg));
        return d->len;
    }
    case 0x2e: case 0x2f: { /* ucomiss/comiss */
        const char *nm = op == 0x2e ? (d->opsz?"ucomisd":"ucomiss")
                                    : (d->opsz?"comisd":"comiss");
        modrm(d, 8, 1); P(d, "%s\t%s, %s", nm, d->rm, xmm(d->reg));
        return d->len;
    }
    case 0x40: case 0x41: case 0x42: case 0x43:
    case 0x44: case 0x45: case 0x46: case 0x47:
    case 0x48: case 0x49: case 0x4a: case 0x4b:
    case 0x4c: case 0x4d: case 0x4e: case 0x4f: /* cmovcc */
        modrm(d, vsize(d), 0);
        P(d, "cmov%s\t%s, %s", cc[op & 15], d->rm, gpr(d, vsize(d), d->reg));
        return d->len;
    case 0x51: case 0x54: case 0x55: case 0x56: case 0x57:
    case 0x58: case 0x59: case 0x5c: case 0x5d: case 0x5e: case 0x5f: {
        /* sqrt/and/andn/or/xor/add/mul/sub/min/div/max */
        const char *base;
        char nm[16];
        switch (op) {
        case 0x51: base = "sqrt"; break;
        case 0x54: base = "and"; break;
        case 0x55: base = "andn"; break;
        case 0x56: base = "or"; break;
        case 0x57: base = "xor"; break;
        case 0x58: base = "add"; break;
        case 0x59: base = "mul"; break;
        case 0x5c: base = "sub"; break;
        case 0x5d: base = "min"; break;
        case 0x5e: base = "div"; break;
        default: base = "max"; break;
        }
        if (op == 0x54 || op == 0x55 || op == 0x56 || op == 0x57)
            snprintf(nm, sizeof nm, "%sp%c", base, d->opsz ? 'd' : 's');
        else
            snprintf(nm, sizeof nm, "%s%s", base,
                     d->rep ? "ss" : d->repne ? "sd" : d->opsz ? "pd" : "ps");
        modrm(d, 8, 1);
        P(d, "%s\t%s, %s", nm, d->rm, xmm(d->reg));
        return d->len;
    }
    case 0x5a: { /* cvtss2sd / cvtsd2ss / cvtps2pd / cvtpd2ps */
        const char *nm = d->rep ? "cvtss2sd" : d->repne ? "cvtsd2ss"
                        : d->opsz ? "cvtpd2ps" : "cvtps2pd";
        modrm(d, 8, 1); P(d, "%s\t%s, %s", nm, d->rm, xmm(d->reg));
        return d->len;
    }
    case 0x6e: /* movd/movq Ev, xmm */
        modrm(d, vsize(d), 0);
        P(d, "%s\t%s, %s", d->rex_w ? "movq" : "movd", d->rm, xmm(d->reg));
        return d->len;
    case 0x6f: /* movdqa/movdqu xmm */
        modrm(d, 8, 1);
        P(d, "%s\t%s, %s", d->rep ? "movdqu" : "movdqa", d->rm, xmm(d->reg));
        return d->len;
    case 0x7e: /* movd/movq xmm->Ev  (or f3: movq xmm,xmm) */
        if (d->rep) { modrm(d, 8, 1); P(d, "movq\t%s, %s", d->rm, xmm(d->reg)); return d->len; }
        modrm(d, vsize(d), 0);
        P(d, "%s\t%s, %s", d->rex_w ? "movq" : "movd", xmm(d->reg), d->rm);
        return d->len;
    case 0x7f: /* movdqa/movdqu store */
        modrm(d, 8, 1);
        P(d, "%s\t%s, %s", d->rep ? "movdqu" : "movdqa", xmm(d->reg), d->rm);
        return d->len;
    case 0xd6: /* movq xmm->Ev */
        modrm(d, 8, 1);
        P(d, "movq\t%s, %s", xmm(d->reg), d->rm);
        return d->len;
    case 0xef: /* pxor */
        modrm(d, 8, 1); P(d, "pxor\t%s, %s", d->rm, xmm(d->reg));
        return d->len;
    case 0x80: case 0x81: case 0x82: case 0x83:
    case 0x84: case 0x85: case 0x86: case 0x87:
    case 0x88: case 0x89: case 0x8a: case 0x8b:
    case 0x8c: case 0x8d: case 0x8e: case 0x8f: { /* jcc rel32 */
        int rtype = 0;
        const char *sym;
        addr_t off = d->pc0 + d->len;
        long long rel = get32(d);
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
        modrm(d, 1, 0);
        P(d, "set%s\t%s", cc[op & 15], d->rm);
        return d->len;
    case 0xa2: P(d, "cpuid"); return d->len;
    case 0xa3: modrm(d, vsize(d), 0); P(d, "bt\t%s, %s", gpr(d, vsize(d), d->reg), d->rm); return d->len;
    case 0xaf: /* imul Gv, Ev */
        modrm(d, vsize(d), 0);
        P(d, "imul\t%s, %s", d->rm, gpr(d, vsize(d), d->reg));
        return d->len;
    case 0xb0: case 0xb1: /* cmpxchg */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size, 0);
        P(d, "cmpxchg\t%s, %s", gpr(d, size, d->reg), d->rm);
        return d->len;
    case 0xb6: case 0xb7: /* movzx */
        modrm(d, op == 0xb6 ? 1 : 2, 0);
        P(d, "movz%c%c\t%s, %s", op == 0xb6 ? 'b' : 'w', sfx(vsize(d)),
          d->rm, gpr(d, vsize(d), d->reg));
        return d->len;
    case 0xbe: case 0xbf: /* movsx */
        modrm(d, op == 0xbe ? 1 : 2, 0);
        P(d, "movs%c%c\t%s, %s", op == 0xbe ? 'b' : 'w', sfx(vsize(d)),
          d->rm, gpr(d, vsize(d), d->reg));
        return d->len;
    case 0xc0: case 0xc1: /* xadd */
        size = (op & 1) ? vsize(d) : 1;
        modrm(d, size, 0);
        P(d, "xadd\t%s, %s", gpr(d, size, d->reg), d->rm);
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

#endif /* MCC_TARGET_X86_64 */
