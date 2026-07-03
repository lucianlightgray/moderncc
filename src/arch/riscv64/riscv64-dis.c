/*
 * riscv64-dis.c - RISC-V (RV64GC subset) instruction disassembler for `-S`.
 *
 * Decodes exactly the encodings riscv64-gen.c emits and prints them in a
 * form mcc's own integrated assembler (riscv64-asm.c) re-assembles to the
 * SAME bytes.  That assembler dictates some unusual spellings:
 *
 *  - Branch/jump targets are printed as numeric byte offsets relative to the
 *    instruction ("beq a0, a1, 8"): riscv64-asm.c encodes a plain immediate
 *    operand as the raw offset field, while a symbolic operand is expanded
 *    into an 8-byte far-branch (bne+auipc) that would change the bytes.
 *    Branch targets are still registered via disasm_label() so `.L<off>:`
 *    anchors appear in the listing for the reader.
 *  - `auipc rd, sym` relies on parse_operand() emitting the same
 *    R_RISCV_PCREL_HI20/GOT_HI20 @insn + R_RISCV_PCREL_LO12_I @insn+4
 *    (anonymous label) reloc pair the code generator uses; the dependent
 *    load/addi that follows is then printed with plain operands.  A
 *    ".globl sym" is emitted inline first when the original reloc was
 *    GOT_HI20, because parse_operand() picks GOT vs PCREL by the symbol's
 *    VT_STATIC flag (cleared only by .globl).
 *  - Calls (R_RISCV_CALL_PLT covering an auipc+jalr pair) also print as
 *    `auipc ra, sym` + plain `jalr ra, 0(ra)`: the regenerated
 *    PCREL_HI20+PCREL_LO12_I pair patches the two instructions exactly
 *    like CALL_PLT did.  mcc's own `call` pseudo would not match (it
 *    encodes jalr with rd=zero).  Callees already labelled earlier in the
 *    listing are referenced through a `.set` alias, and symbols that
 *    collide with register/CSR names are spelled `0+sym`.
 *
 * Anything the integrated assembler cannot reproduce byte-exactly (flw/fsw,
 * fmv.[wxd].*, fcvt.d.s with rm=0, sraw/sraiw, PCREL_LO12_S store pairs,
 * PCREL_HI20 with addend, TLS) falls back to a length-correct `.long`
 * (mcc's `.word` is 2 bytes, so `.long` is the 4-byte directive).
 */
#include "mcc.h"

#ifdef MCC_TARGET_RISCV64

static const char * const xrn[32] = {
    "zero","ra","sp","gp","tp","t0","t1","t2",
    "s0","s1","a0","a1","a2","a3","a4","a5",
    "a6","a7","s2","s3","s4","s5","s6","s7",
    "s8","s9","s10","s11","t3","t4","t5","t6"
};
static const char * const frn[32] = {
    "ft0","ft1","ft2","ft3","ft4","ft5","ft6","ft7",
    "fs0","fs1","fa0","fa1","fa2","fa3","fa4","fa5",
    "fa6","fa7","fs2","fs3","fs4","fs5","fs6","fs7",
    "fs8","fs9","fs10","fs11","ft8","ft9","ft10","ft11"
};

static void P(disasm_ctx *dc, const char *fmt, ...)
{
    va_list ap;
    if (dc->collect) /* pass 1 only gathers branch targets */
        return;
    va_start(ap, fmt);
    vfprintf(dc->out, fmt, ap);
    va_end(ap);
}

/* length-correct fallback for anything the integrated assembler cannot
   re-assemble byte-exactly (mcc's `.word` is 2 bytes; `.long` is 4) */
static int raw32(disasm_ctx *dc, uint32_t w)
{
    P(dc, ".long\t0x%08x", w);
    return 4;
}

/* ---- immediate field extraction --------------------------------------- */

static int32_t imm_i(uint32_t w) { return (int32_t)w >> 20; }
static int32_t imm_s(uint32_t w)
{
    return (((int32_t)w >> 25) << 5) | ((w >> 7) & 0x1f);
}
static int32_t imm_b(uint32_t w)
{
    int32_t v = (((w >> 31) & 1) << 12) | (((w >> 7) & 1) << 11)
              | (((w >> 25) & 0x3f) << 5) | (((w >> 8) & 0xf) << 1);
    return (v << 19) >> 19;
}
static int32_t imm_j(uint32_t w)
{
    int32_t v = (((w >> 31) & 1) << 20) | (((w >> 12) & 0xff) << 12)
              | (((w >> 20) & 1) << 11) | (((w >> 21) & 0x3ff) << 1);
    return (v << 11) >> 11;
}

/* "sym" only; "sym+N"/"sym-N" would lose its addend in riscv64-asm.c's
   parse_operand(), which hardcodes a zero reloc addend */
static int plain_sym(const char *s)
{
    return s && !strchr(s, '+') && !strchr(s, '-');
}

/* parse_operand() checks the operand's first token against register and CSR
   names before trying an expression, so a symbol that happens to be called
   "gp"/"time"/... must be spelled "0+sym" (asm_expr itself has no register
   namespace).  Returns the "0+" prefix to prepend, or "". */
static const char *sym_esc(const char *s)
{
    static const char * const csr[] = {
        "cycle", "fcsr", "fflags", "frm", "instret", "time",
        "cycleh", "instreth", "timeh", "pc", NULL
    };
    int i;
    if ((s[0] == 'x' || s[0] == 'f') && s[1] >= '0' && s[1] <= '9')
        return "0+";
    for (i = 0; i < 32; i++)
        if (!strcmp(s, xrn[i]) || !strcmp(s, frn[i]))
            return "0+";
    for (i = 0; csr[i]; i++)
        if (!strcmp(s, csr[i]))
            return "0+";
    return "";
}

/* ELF symbol a reloc of `type` at `off` points to, for definedness checks */
static ElfW(Sym) *reloc_sym_at(disasm_ctx *dc, addr_t off, int type)
{
    Section *sr = dc->sec->reloc;
    ElfW_Rel *rel;
    if (!sr)
        return NULL;
    for_each_elem(sr, 0, rel, ElfW_Rel) {
        if (rel->r_offset == off
            && (int)ELFW(R_TYPE)(rel->r_info) == type)
            return &((ElfW(Sym) *)dc->symtab->data)[ELFW(R_SYM)(rel->r_info)];
    }
    return NULL;
}

/* Symbols already labelled earlier in the listing have lost both VT_EXTERN
   (definition) and, if global, VT_STATIC (.globl): riscv64-asm.c's
   parse_operand() then rejects them as instruction operands. */
static int defined_before(disasm_ctx *dc, ElfW(Sym) *s)
{
    return s && s->st_shndx == dc->sec->sh_num && s->st_value < dc->pc;
}

/* ---- auipc: reloc-carrying address formation -------------------------- */

/* auipc is where all pc-relative reloc pairs start.  The dependent insn at
   pc+4 (ld/addi/jalr, PCREL_LO12_I towards an anonymous label at pc) is
   printed with plain operands: re-assembling `auipc rd, sym` regenerates
   both relocations of the pair. */
static int dis_auipc(disasm_ctx *dc, uint32_t w)
{
    int rd = (w >> 7) & 0x1f;
    int rtype = 0, lotype = 0;
    char relbuf[256];
    const char *rel = disasm_reloc(dc, dc->pc, 4, &rtype);
    const char *lo;

    if (rel) { /* disasm_reloc returns a static buffer: save before requerying */
        snprintf(relbuf, sizeof relbuf, "%s", rel);
        rel = relbuf;
    }
    lo = dc->pc + 4 < dc->size
       ? disasm_reloc(dc, dc->pc + 4, 4, &lotype) : NULL;

    if (!rel) {
        P(dc, "auipc\t%s, 0x%x", xrn[rd], (w >> 12) & 0xfffff);
        return 4;
    }
    if (rtype == R_RISCV_CALL || rtype == R_RISCV_CALL_PLT) {
        /* auipc tr + jalr tr, 0(tr): a CALL_PLT covering both insns links
           exactly like PCREL_HI20 @auipc + PCREL_LO12_I @jalr, which is
           what re-assembling `auipc tr, sym` regenerates (the jalr then
           decodes as a plain instruction).  mcc's own `call` pseudo would
           not match: it encodes jalr with rd=zero. */
        ElfW(Sym) *s = reloc_sym_at(dc, dc->pc, rtype);
        if (plain_sym(rel)) {
            if (defined_before(dc, s)
                && ELFW(ST_BIND)(s->st_info) != STB_LOCAL) {
                /* already-.globl'd label: alias it via .set, which yields a
                   fresh assembler symbol at the same address that
                   parse_operand() still accepts as a PCREL_HI20 operand */
                P(dc, ".set\t.Lcs%llx, %s%s\n\tauipc\t%s, .Lcs%llx",
                  (unsigned long long)dc->pc, sym_esc(rel), rel, xrn[rd],
                  (unsigned long long)dc->pc);
            } else {
                P(dc, "auipc\t%s, %s%s", xrn[rd], sym_esc(rel), rel);
            }
            return 4;
        }
        return raw32(dc, w); /* sym+addend: parse_operand drops addends */
    }
    if (rtype == R_RISCV_GOT_HI20 && plain_sym(rel)
        && lo && lotype == R_RISCV_PCREL_LO12_I
        && !defined_before(dc, reloc_sym_at(dc, dc->pc, rtype))) {
        /* parse_operand() picks GOT_HI20 only for non-VT_STATIC symbols;
           .globl (idempotent, valid for undefined syms too) clears the
           VT_STATIC every assembler-level symbol starts with. */
        P(dc, ".globl\t%s\n\tauipc\t%s, %s%s", rel, xrn[rd], sym_esc(rel), rel);
        return 4;
    }
    if (rtype == R_RISCV_PCREL_HI20 && plain_sym(rel)
        && lo && lotype == R_RISCV_PCREL_LO12_I) {
        P(dc, "auipc\t%s, %s%s", xrn[rd], sym_esc(rel), rel);
        return 4;
    }
    /* PCREL_HI20 with addend, store pairs (PCREL_LO12_S: parse_operand can
       only emit LO12_I), TPREL, or a stray reloc: not expressible. */
    return raw32(dc, w);
}

/* ---- main decode ------------------------------------------------------- */

static int dis_op_fp(disasm_ctx *dc, uint32_t w)
{
    int rd = (w >> 7) & 0x1f, rm = (w >> 12) & 7;
    int rs1 = (w >> 15) & 0x1f, rs2 = (w >> 20) & 0x1f;
    int f7 = w >> 25;
    char fc = (f7 & 1) ? 'd' : 's';
    static const char * const cvt[4] = { "w", "wu", "l", "lu" };

    switch (f7 & ~1) {
    case 0x00: case 0x04: case 0x08: case 0x0c: { /* fadd/fsub/fmul/fdiv */
        static const char * const nm[4] = { "fadd", "fsub", "fmul", "fdiv" };
        if (rm != 7) /* riscv64-asm.c hardcodes rm=dyn for these */
            break;
        P(dc, "%s.%c\t%s, %s, %s", nm[(f7 >> 2) & 3], fc,
          frn[rd], frn[rs1], frn[rs2]);
        return 4;
    }
    case 0x10: /* fsgnj/fsgnjn/fsgnjx */
        if (rm == 0 && rs1 == rs2)
            P(dc, "fmv.%c\t%s, %s", fc, frn[rd], frn[rs1]);
        else if (rm == 0)
            P(dc, "fsgnj.%c\t%s, %s, %s", fc, frn[rd], frn[rs1], frn[rs2]);
        else if (rm == 1 && rs1 == rs2)
            P(dc, "fneg.%c\t%s, %s", fc, frn[rd], frn[rs1]);
        else if (rm == 2 && rs1 == rs2)
            P(dc, "fabs.%c\t%s, %s", fc, frn[rd], frn[rs1]);
        else
            break; /* fsgnjn/fsgnjx with rs1 != rs2: no assembler spelling */
        return 4;
    case 0x14: /* fmin/fmax */
        if (rm > 1)
            break;
        P(dc, "%s.%c\t%s, %s, %s", rm ? "fmax" : "fmin", fc,
          frn[rd], frn[rs1], frn[rs2]);
        return 4;
    case 0x20: /* fcvt.s.d / fcvt.d.s */
        /* gen emits fcvt.d.s with rm=0 but the assembler hardcodes rm=dyn,
           so only the rm=7 encodings round-trip */
        if (rm == 7 && f7 == 0x20 && rs2 == 1)
            P(dc, "fcvt.s.d\t%s, %s", frn[rd], frn[rs1]);
        else if (rm == 7 && f7 == 0x21 && rs2 == 0)
            P(dc, "fcvt.d.s\t%s, %s", frn[rd], frn[rs1]);
        else
            break;
        return 4;
    case 0x2c: /* fsqrt */
        if (rm != 7 || rs2 != 0)
            break;
        P(dc, "fsqrt.%c\t%s, %s", fc, frn[rd], frn[rs1]);
        return 4;
    case 0x50: { /* fle/flt/feq: int rd */
        static const char * const nm[3] = { "fle", "flt", "feq" };
        if (rm > 2)
            break;
        P(dc, "%s.%c\t%s, %s, %s", nm[rm], fc,
          xrn[rd], frn[rs1], frn[rs2]);
        return 4;
    }
    case 0x60: /* fcvt.{w,wu,l,lu}.{s,d}: rm=rtz (gen) or dyn */
        if (rs2 > 3 || (rm != 1 && rm != 7))
            break;
        P(dc, "fcvt.%s.%c\t%s, %s%s", cvt[rs2], fc, xrn[rd], frn[rs1],
          rm == 1 ? ", rtz" : "");
        return 4;
    case 0x68: /* fcvt.{s,d}.{w,wu,l,lu} */
        if (rs2 > 3 || rm != 7)
            break;
        P(dc, "fcvt.%c.%s\t%s, %s", fc, cvt[rs2], frn[rd], xrn[rs1]);
        return 4;
    case 0x70: /* fmv.x.[wd] (rm 0: no assembler support) / fclass */
        if (rm == 1 && rs2 == 0) {
            P(dc, "fclass.%c\t%s, %s", fc, xrn[rd], frn[rs1]);
            return 4;
        }
        break;
    case 0x78: /* fmv.[wd].x: no assembler support */
        break;
    }
    return raw32(dc, w);
}

static int dis_insn(disasm_ctx *dc)
{
    addr_t pc = dc->pc;
    uint32_t w;
    int rd, f3, rs1, rs2, rtype;
    const char *rel;

    if (pc + 2 > dc->size) {
        P(dc, ".byte\t0x%02x", dc->data[pc]);
        return 1;
    }
    w = read16le(dc->data + pc);
    if ((w & 3) != 3 || pc + 4 > dc->size) {
        /* compressed (only bounds-checking helpers emit these) or a
           truncated tail: dump the 16-bit unit */
        P(dc, ".short\t0x%04x", w);
        return 2;
    }
    w = read32le(dc->data + pc);
    rd = (w >> 7) & 0x1f;
    f3 = (w >> 12) & 7;
    rs1 = (w >> 15) & 0x1f;
    rs2 = (w >> 20) & 0x1f;

    switch (w & 0x7f) {
    case 0x37: /* lui (a reloc here would be TPREL_HI20: unsupported) */
        if (disasm_reloc(dc, pc, 4, &rtype))
            return raw32(dc, w);
        P(dc, "lui\t%s, 0x%x", xrn[rd], (w >> 12) & 0xfffff);
        return 4;

    case 0x17:
        return dis_auipc(dc, w);

    case 0x6f: { /* jal: numeric offset (symbolic would emit R_RISCV_JAL) */
        int32_t off = imm_j(w);
        disasm_label(dc, pc + off);
        if (rd == 0)
            P(dc, "j\t%d", off);
        else if (rd == 1)
            P(dc, "jal\t%d", off);
        else
            P(dc, "jal\t%s, %d", xrn[rd], off);
        return 4;
    }

    case 0x67: /* jalr */
        if (f3 != 0 || disasm_reloc(dc, pc, 4, &rtype))
            return raw32(dc, w);
        if (rd == 0 && rs1 == 1 && imm_i(w) == 0)
            P(dc, "ret");
        else
            P(dc, "jalr\t%s, %d(%s)", xrn[rd], imm_i(w), xrn[rs1]);
        return 4;

    case 0x63: { /* branches: numeric offset (symbolic operands become an
                    8-byte far-branch in riscv64-asm.c) */
        static const char * const nm[8] =
            { "beq", "bne", 0, 0, "blt", "bge", "bltu", "bgeu" };
        int32_t off = imm_b(w);
        if (!nm[f3] || disasm_reloc(dc, pc, 4, &rtype))
            return raw32(dc, w);
        disasm_label(dc, pc + off);
        P(dc, "%s\t%s, %s, %d", nm[f3], xrn[rs1], xrn[rs2], off);
        return 4;
    }

    case 0x03: { /* integer loads */
        static const char * const nm[8] =
            { "lb", "lh", "lw", "ld", "lbu", "lhu", "lwu", 0 };
        /* a PCREL_LO12_I here belongs to the preceding auipc's pair and is
           regenerated by it; print the raw (zero) immediate */
        if (!nm[f3] || (disasm_reloc(dc, pc, 4, &rtype)
                        && rtype != R_RISCV_PCREL_LO12_I))
            return raw32(dc, w);
        P(dc, "%s\t%s, %d(%s)", nm[f3], xrn[rd], imm_i(w), xrn[rs1]);
        return 4;
    }

    case 0x23: { /* integer stores */
        static const char * const nm[8] = { "sb", "sh", "sw", "sd" };
        /* a PCREL_LO12_S here belongs to the preceding auipc, which had to
           fall back (parse_operand only emits LO12_I): the pair's relocs
           are lost either way, so keep the store itself readable */
        if (f3 > 3 || (disasm_reloc(dc, pc, 4, &rtype)
                       && rtype != R_RISCV_PCREL_LO12_S))
            return raw32(dc, w);
        P(dc, "%s\t%s, %d(%s)", nm[f3], xrn[rs2], imm_s(w), xrn[rs1]);
        return 4;
    }

    case 0x07: /* fp loads: flw exists as a token but is not dispatched by
                  riscv64-asm.c, so only fld round-trips */
        if (f3 != 3 || (disasm_reloc(dc, pc, 4, &rtype)
                        && rtype != R_RISCV_PCREL_LO12_I))
            return raw32(dc, w);
        P(dc, "fld\t%s, %d(%s)", frn[rd], imm_i(w), xrn[rs1]);
        return 4;

    case 0x27: /* fp stores: fsw not dispatched either */
        if (f3 != 3 || (disasm_reloc(dc, pc, 4, &rtype)
                        && rtype != R_RISCV_PCREL_LO12_S))
            return raw32(dc, w);
        P(dc, "fsd\t%s, %d(%s)", frn[rs2], imm_s(w), xrn[rs1]);
        return 4;

    case 0x13: { /* op-imm */
        static const char * const nm[8] =
            { "addi", 0, "slti", "sltiu", "xori", 0, "ori", "andi" };
        int32_t imm = imm_i(w);
        rel = disasm_reloc(dc, pc, 4, &rtype);
        if (rel && rtype != R_RISCV_PCREL_LO12_I
                && rtype != R_RISCV_TPREL_LO12_I)
            return raw32(dc, w);
        if (f3 == 1 || f3 == 5) { /* shifts, 6-bit shamt */
            int top6 = w >> 26;
            if (f3 == 1 && top6 == 0)
                P(dc, "slli\t%s, %s, %d", xrn[rd], xrn[rs1], imm & 63);
            else if (f3 == 5 && top6 == 0)
                P(dc, "srli\t%s, %s, %d", xrn[rd], xrn[rs1], imm & 63);
            else if (f3 == 5 && top6 == 0x10)
                P(dc, "srai\t%s, %s, %d", xrn[rd], xrn[rs1], imm & 63);
            else
                return raw32(dc, w);
            return 4;
        }
        if (f3 == 0 && w == 0x13)
            P(dc, "nop");
        else if (f3 == 0 && imm == 0 && !rel)
            P(dc, "mv\t%s, %s", xrn[rd], xrn[rs1]);
        else
            P(dc, "%s\t%s, %s, %d", nm[f3], xrn[rd], xrn[rs1], imm);
        return 4;
    }

    case 0x1b: /* op-imm-32 */
        if (disasm_reloc(dc, pc, 4, &rtype))
            return raw32(dc, w);
        if (f3 == 0)
            P(dc, "addiw\t%s, %s, %d", xrn[rd], xrn[rs1], imm_i(w));
        else if (f3 == 1 && (w >> 25) == 0)
            P(dc, "slliw\t%s, %s, %d", xrn[rd], xrn[rs1], rs2);
        else if (f3 == 5 && (w >> 25) == 0)
            P(dc, "srliw\t%s, %s, %d", xrn[rd], xrn[rs1], rs2);
        else /* incl. sraiw: riscv64-asm.c mis-encodes it as srliw */
            return raw32(dc, w);
        return 4;

    case 0x33: case 0x3b: { /* op / op-32 */
        static const char * const base[8] =
            { "add", "sll", "slt", "sltu", "xor", "srl", "or", "and" };
        static const char * const muldiv[8] =
            { "mul", "mulh", "mulhsu", "mulhu", "div", "divu", "rem", "remu" };
        int w32 = (w & 0x7f) == 0x3b;
        int f7 = w >> 25;
        const char *nm = NULL;
        if (f7 == 0x00)
            nm = base[f3];
        else if (f7 == 0x20 && f3 == 0)
            nm = "sub";
        else if (f7 == 0x20 && f3 == 5)
            nm = "sra";
        else if (f7 == 0x01)
            nm = muldiv[f3];
        if (!nm)
            return raw32(dc, w);
        if (w32) {
            /* only these have w-forms; riscv64-asm.c mis-encodes sraw */
            if (strcmp(nm, "add") && strcmp(nm, "sub") && strcmp(nm, "sll")
                && strcmp(nm, "srl") && strcmp(nm, "mul") && strcmp(nm, "div")
                && strcmp(nm, "divu") && strcmp(nm, "rem")
                && strcmp(nm, "remu"))
                return raw32(dc, w);
            P(dc, "%sw\t%s, %s, %s", nm, xrn[rd], xrn[rs1], xrn[rs2]);
        } else {
            P(dc, "%s\t%s, %s, %s", nm, xrn[rd], xrn[rs1], xrn[rs2]);
        }
        return 4;
    }

    case 0x53:
        return dis_op_fp(dc, w);

    case 0x0f: /* gen_clear_cache */
        if (w == 0x0ff0000f)
            P(dc, "fence");
        else if (w == 0x0000100f)
            P(dc, "fence.i");
        else
            return raw32(dc, w);
        return 4;

    case 0x73:
        if (w == 0x00000073)
            P(dc, "ecall");
        else if (w == 0x00100073)
            P(dc, "ebreak");
        else
            return raw32(dc, w);
        return 4;
    }
    return raw32(dc, w);
}

ST_FUNC int mcc_disasm_insn(disasm_ctx *dc)
{
    return dis_insn(dc);
}

/* ---- reloc metadata for the arch-independent driver ------------------- */

ST_FUNC int mcc_disasm_reloc_size(int type)
{
    switch (type) {
    case R_RISCV_64:
        return 8;
    case R_RISCV_32:
        return 4;
    }
    /* instruction-field relocs are handled inside mcc_disasm_insn; sizing
       them here would make emit_data() try to splice them as data */
    return 0;
}

ST_FUNC int mcc_disasm_reloc_addend_bias(int type, int size)
{
    /* RISC-V RELA addends read back verbatim; nothing to fold out */
    (void)type; (void)size;
    return 0;
}

#endif /* MCC_TARGET_RISCV64 */
