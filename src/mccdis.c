/*
 * mccdis.c - assembly listing output for `-S`.
 *
 * mcc is TCC-derived: the backend emits raw machine-code bytes straight into
 * the section data with no textual-assembly IR.  `-S` therefore runs a normal
 * object-style compile (relocations kept symbolic, no link) and then turns the
 * populated sections back into a gas/AT&T-syntax listing here.  The .text
 * section is decoded by the per-arch disassembler (mcc_disasm_insn); data
 * sections are emitted directly as .byte/.short/.long/.quad/.string directives
 * with symbolic relocations spliced in.
 */
#include "mcc.h"

/* ---- symbol / relocation helpers ------------------------------------- */

/* Unique display name per symtab index.  Function-local `static` objects in
   different functions share a name (e.g. two `static char buf[]`); emitting
   both as `buf:` would make gas reject a duplicate definition.  We compute a
   collision-free name once and use it for BOTH labels and reloc operands so
   references (which are by symbol index) stay consistent. */
static char **g_uniq;

static void build_unique_names(MCCState *s1)
{
    int n = s1->symtab->data_offset / sizeof(ElfW(Sym));
    ElfW(Sym) *syms = (ElfW(Sym) *)s1->symtab->data;
    const char *strtab = (char *)s1->symtab->link->data;
    g_uniq = mcc_mallocz(n * sizeof *g_uniq);
    for (int i = 1; i < n; i++) {
        const char *name = strtab + syms[i].st_name;
        int dup = 0;
        if (!name[0])
            continue;
        for (int j = 1; j < i; j++)
            if (syms[j].st_name && !strcmp(strtab + syms[j].st_name, name)) {
                dup = 1;
                break;
            }
        if (dup) {
            char buf[256];
            snprintf(buf, sizeof buf, "%s.%d", name, i);
            g_uniq[i] = mcc_strdup(buf);
        }
    }
}

static void free_unique_names(MCCState *s1)
{
    int n = s1->symtab->data_offset / sizeof(ElfW(Sym));
    for (int i = 0; i < n; i++)
        mcc_free(g_uniq[i]);
    mcc_free(g_uniq);
    g_uniq = NULL;
}

static const char *sym_name(MCCState *s1, int sym_index)
{
    ElfW(Sym) *sym = &((ElfW(Sym) *)s1->symtab->data)[sym_index];
    const char *name = (char *)s1->symtab->link->data + sym->st_name;
    if (g_uniq && g_uniq[sym_index])
        return g_uniq[sym_index];
    if (name && name[0])
        return name;
    /* Local label with no name (e.g. a section symbol): synthesise one from
       the section it points into so the operand is still re-assemblable. */
    if (sym->st_shndx > 0 && sym->st_shndx < s1->nb_sections)
        return s1->sections[sym->st_shndx]->name;
    return ".L.anon";
}

/* Find a relocation in dc->sec whose r_offset lies in [off, off+size).
   Returns a static "sym", "sym+N" or "sym-N" string (with PC-relative
   addends normalised so a following (%rip) / call target re-assembles), and
   sets *ptype to the ELF relocation type.  NULL if no reloc covers the field. */
/* Natural byte width of an x86-64 relocation type (the size of the field it
   patches), or 0 if unknown. */
static int reloc_field_size(int type)
{
    (void)type;
#if defined(MCC_TARGET_X86_64)
    switch (type) {
    case R_X86_64_64:
    case R_X86_64_TPOFF64:
    case R_X86_64_GOTPCREL64:
        return 8;
    case R_X86_64_PC32:
    case R_X86_64_PLT32:
    case R_X86_64_GOTPCREL:
    case R_X86_64_GOTPCRELX:
    case R_X86_64_REX_GOTPCRELX:
    case R_X86_64_32:
    case R_X86_64_32S:
    case R_X86_64_TPOFF32:
        return 4;
    }
#endif
    return 0;
}

ST_FUNC const char *disasm_reloc(disasm_ctx *dc, addr_t off, int size, int *ptype)
{
    static char buf[256];
    Section *sr = dc->sec->reloc;
    ElfW(Rela) *rel;
    if (!sr)
        return NULL;
    for_each_elem(sr, 0, rel, ElfW(Rela)) {
        if (rel->r_offset < off || rel->r_offset >= off + size)
            continue;
        {
            int type = ELFW(R_TYPE)(rel->r_info);
            int idx = ELFW(R_SYM)(rel->r_info);
            const char *name = sym_name(dc->s1, idx);
            addr_t addend = rel->r_addend;
            if (ptype)
                *ptype = type;
#if defined(MCC_TARGET_X86_64)
            /* PC-relative relocs (call/jmp/rip) carry -(size) as their addend
               to bias from the field to the insn end; gas re-derives that, so
               fold it out for a clean "sym" / "sym+disp". */
            if (type == R_X86_64_PC32 || type == R_X86_64_PLT32
             || type == R_X86_64_GOTPCREL || type == R_X86_64_GOTPCRELX
             || type == R_X86_64_REX_GOTPCRELX)
                addend += size;
#endif
            if (addend == 0)
                snprintf(buf, sizeof buf, "%s", name);
            else if (addend > 0)
                snprintf(buf, sizeof buf, "%s+%lld", name, (long long)addend);
            else
                snprintf(buf, sizeof buf, "%s-%lld", name, -(long long)addend);
            return buf;
        }
    }
    return NULL;
}

#ifdef MCC_HAVE_DISASM
ST_FUNC const char *disasm_label(disasm_ctx *dc, addr_t target)
{
    static char buf[32];
    if (dc->collect && target < dc->size) {
        int i;
        for (i = 0; i < dc->nlabels; i++)
            if (dc->labels[i] == target)
                break;
        if (i == dc->nlabels) {
            if (dc->nlabels == dc->labels_cap) {
                dc->labels_cap = dc->labels_cap ? dc->labels_cap * 2 : 16;
                dc->labels = mcc_realloc(dc->labels,
                                         dc->labels_cap * sizeof *dc->labels);
            }
            dc->labels[dc->nlabels++] = target;
        }
    }
    snprintf(buf, sizeof buf, ".L%llx", (unsigned long long)target);
    return buf;
}

static int addr_cmp(const void *a, const void *b)
{
    addr_t x = *(const addr_t *)a, y = *(const addr_t *)b;
    return x < y ? -1 : x > y ? 1 : 0;
}
#endif /* MCC_HAVE_DISASM */

/* ---- per-section symbol table --------------------------------------- */

/* Symbols defined in a given section, sorted by value, for label placement. */
struct secsym { addr_t value; ElfW(Sym) *sym; const char *name; };

static int secsym_cmp(const void *a, const void *b)
{
    const struct secsym *x = a, *y = b;
    if (x->value < y->value) return -1;
    if (x->value > y->value) return 1;
    /* function/object symbols before the size-marker ordering is irrelevant;
       keep stable-ish by name */
    return 0;
}

static struct secsym *collect_syms(MCCState *s1, int shndx, int *pn)
{
    ElfW(Sym) *sym0 = (ElfW(Sym) *)s1->symtab->data;
    ElfW(Sym) *sym;
    int n = 0, cap = 0;
    struct secsym *v = NULL;
    for_each_elem(s1->symtab, 1, sym, ElfW(Sym)) {
        const char *name;
        if (sym->st_shndx != shndx)
            continue;
        if (ELFW(ST_TYPE)(sym->st_info) == STT_SECTION
         || ELFW(ST_TYPE)(sym->st_info) == STT_FILE)
            continue;
        name = sym_name(s1, (int)(sym - sym0));
        if (!name || !name[0])
            continue;
        if (n == cap) {
            cap = cap ? cap * 2 : 16;
            v = mcc_realloc(v, cap * sizeof *v);
        }
        v[n].value = sym->st_value;
        v[n].sym = sym;
        v[n].name = name;
        n++;
    }
    if (v)
        qsort(v, n, sizeof *v, secsym_cmp);
    *pn = n;
    return v;
}

/* Emit .globl / .weak / .type for a symbol about to be labelled. */
static void emit_sym_decl(FILE *f, ElfW(Sym) *sym, const char *name)
{
    int bind = ELFW(ST_BIND)(sym->st_info);
    int type = ELFW(ST_TYPE)(sym->st_info);
    if (bind == STB_GLOBAL)
        fprintf(f, "\t.globl\t%s\n", name);
    else if (bind == STB_WEAK)
        fprintf(f, "\t.weak\t%s\n", name);
    if (type == STT_FUNC)
        fprintf(f, "\t.type\t%s, @function\n", name);
    else if (type == STT_OBJECT)
        fprintf(f, "\t.type\t%s, @object\n", name);
}

/* ---- section body emitters ------------------------------------------ */

static void emit_directive_header(FILE *f, Section *s)
{
    const char *n = s->name;
    /* mcc names its read-only-data section internally as ".data.ro" (or
       ".rdata" on PE); emit the conventional ".rodata" gcc/clang use, which
       also avoids a dotted-name parse quirk in mcc's own assembler. */
    if (!strcmp(n, ".data.ro") || !strcmp(n, ".rdata"))
        n = ".rodata";
    if (!strcmp(n, ".text"))
        fprintf(f, "\t.text\n");
    else if (!strcmp(n, ".data"))
        fprintf(f, "\t.data\n");
    else if (!strcmp(n, ".bss"))
        fprintf(f, "\t.bss\n");
    else if (!strcmp(n, ".rodata"))
        fprintf(f, "\t.section\t.rodata,\"a\",@progbits\n");
    else {
        /* .rodata and any other named section: full .section directive. */
        fprintf(f, "\t.section\t%s", n);
        if (s->sh_type == SHT_NOBITS)
            fprintf(f, ",\"aw\",@nobits");
        else if (s->sh_flags & SHF_EXECINSTR)
            fprintf(f, ",\"ax\",@progbits");
        else if (s->sh_flags & SHF_WRITE)
            fprintf(f, ",\"aw\",@progbits");
        else
            fprintf(f, ",\"a\",@progbits");
        fprintf(f, "\n");
    }
    if (s->sh_addralign > 1)
        fprintf(f, "\t.align\t%d\n", s->sh_addralign);
}

/* .text: label each function, then disassemble its byte range.  A first pass
   collects intra-section branch targets so they can be emitted as local
   labels (.L<off>) that re-assemble, the way gcc -S does. */
static void emit_text(disasm_ctx *dc, struct secsym *syms, int nsym)
{
    FILE *f = dc->out;
    addr_t pc, end = dc->size;
    int si, li;

#ifdef MCC_HAVE_DISASM
    /* pass 1: gather branch targets (no output) */
    dc->collect = 1;
    for (pc = 0; pc < end; ) {
        int len;
        dc->pc = pc;
        len = mcc_disasm_insn(dc);
        pc += len < 1 ? 1 : len;
    }
    dc->collect = 0;
    if (dc->nlabels)
        qsort(dc->labels, dc->nlabels, sizeof *dc->labels, addr_cmp);
#endif

    si = li = 0;
    for (pc = 0; pc < end; ) {
        while (si < nsym && syms[si].value == pc) {
            emit_sym_decl(f, syms[si].sym, syms[si].name);
            fprintf(f, "%s:\n", syms[si].name);
            si++;
        }
        while (li < dc->nlabels && dc->labels[li] == pc) {
            fprintf(f, ".L%llx:\n", (unsigned long long)pc);
            li++;
        }
        while (li < dc->nlabels && dc->labels[li] < pc)
            li++; /* target that fell mid-instruction: skip (shouldn't happen) */
#ifdef MCC_HAVE_DISASM
        {
            int len;
            dc->pc = pc;
            fprintf(f, "\t");
            len = mcc_disasm_insn(dc);
            fprintf(f, "\n");
            pc += len < 1 ? 1 : len;
        }
#else
        fprintf(f, "\t.byte\t0x%02x\n", dc->data[pc]);
        pc++;
#endif
    }
}

/* .data / .rodata: emit initialised bytes with labels and symbolic relocs. */
static void emit_data(disasm_ctx *dc, struct secsym *syms, int nsym)
{
    FILE *f = dc->out;
    addr_t pc = 0, end = dc->size;
    int si = 0;
    while (pc < end) {
        int rtype = 0, sz;
        const char *r;
        while (si < nsym && syms[si].value == pc) {
            emit_sym_decl(f, syms[si].sym, syms[si].name);
            fprintf(f, "%s:\n", syms[si].name);
            si++;
        }
        /* A relocation starting at pc becomes a symbolic .long/.quad, sized by
           the relocation type (never guessed — a 4-byte field must stay 4). */
        r = disasm_reloc(dc, pc, 1, &rtype);
        sz = r ? reloc_field_size(rtype) : 0;
        if (r && sz) {
            fprintf(f, "\t%s\t%s\n", sz == 8 ? ".quad" : ".long", r);
            pc += sz;
            continue;
        }
        /* Otherwise a run of literal bytes up to the next label/reloc. */
        {
            addr_t run = pc;
            fprintf(f, "\t.byte\t");
            while (run < end) {
                if (run != pc) {
                    if (si < nsym && syms[si].value == run) break;
                    if (dc->sec->reloc && disasm_reloc(dc, run, 1, &rtype)) break;
                    fprintf(f, ", ");
                }
                fprintf(f, "0x%02x", dc->data[run]);
                run++;
                if (run - pc >= 16) break;
            }
            fprintf(f, "\n");
            pc = run;
        }
    }
}

/* Emit a .size directive for every function/object symbol with a size. */
static void emit_sizes(FILE *f, struct secsym *syms, int nsym)
{
    for (int i = 0; i < nsym; i++) {
        int type = ELFW(ST_TYPE)(syms[i].sym->st_info);
        if ((type == STT_FUNC || type == STT_OBJECT) && syms[i].sym->st_size)
            fprintf(f, "\t.size\t%s, %llu\n", syms[i].name,
                    (unsigned long long)syms[i].sym->st_size);
    }
}

/* ---- driver --------------------------------------------------------- */

static int section_is_output(Section *s)
{
    static const char *skip[] = {
        /* toolchain-generated metadata: gcc -S regenerates unwind info from
           .cfi_* directives rather than dumping bytes, and note/debug/comment
           sections are not part of an assembly listing.  Emitting them as raw
           .byte blobs is both unreadable and, for the PC-relative .eh_frame
           FDE pointers, not faithfully expressible — so leave them out. */
        ".eh_frame", ".note", ".comment", ".debug", ".stab", ".gnu", 0 };
    int i;
    if (!(s->sh_flags & SHF_ALLOC))
        return 0;
    if (s->sh_type != SHT_PROGBITS && s->sh_type != SHT_NOBITS)
        return 0;
    if (s->data_offset == 0 && s->sh_type != SHT_NOBITS)
        return 0;
    for (i = 0; skip[i]; i++)
        if (!strncmp(s->name, skip[i], strlen(skip[i])))
            return 0;
    return 1;
}

ST_FUNC int asm_output_file(MCCState *s1, const char *filename)
{
    FILE *f;
    int i;

    if (!filename || !strcmp(filename, "-"))
        f = stdout;
    else if (!(f = host_fopen(filename, "wb")))
        return mcc_error_noabort("could not write '%s'", filename);

    fprintf(f, "\t.file\t\"%s\"\n",
            s1->nb_files ? mcc_basename(s1->files[0]->name) : "mcc");

    build_unique_names(s1);

    for (i = 1; i < s1->nb_sections; i++) {
        Section *s = s1->sections[i];
        struct secsym *syms;
        int nsym;
        disasm_ctx dc = {0};

        if (!section_is_output(s))
            continue;

        syms = collect_syms(s1, s->sh_num, &nsym);

        emit_directive_header(f, s);

        dc.s1 = s1;
        dc.sec = s;
        dc.symtab = s1->symtab;
        dc.data = s->data;
        dc.size = (s->sh_type == SHT_NOBITS) ? 0 : s->data_offset;
        dc.out = f;

        if (s->sh_type == SHT_NOBITS) {
            /* .bss: labels + .skip for the whole span. */
            int si = 0;
            addr_t pc = 0, end = s->data_offset;
            while (si < nsym) {
                if (syms[si].value > pc && syms[si].value <= end) {
                    fprintf(f, "\t.skip\t%llu\n",
                            (unsigned long long)(syms[si].value - pc));
                    pc = syms[si].value;
                }
                emit_sym_decl(f, syms[si].sym, syms[si].name);
                fprintf(f, "%s:\n", syms[si].name);
                si++;
            }
            if (end > pc)
                fprintf(f, "\t.skip\t%llu\n", (unsigned long long)(end - pc));
        } else if (s->sh_flags & SHF_EXECINSTR) {
            emit_text(&dc, syms, nsym);
        } else {
            emit_data(&dc, syms, nsym);
        }

        emit_sizes(f, syms, nsym);
        mcc_free(syms);
        mcc_free(dc.labels);
    }

    fprintf(f, "\t.ident\t\"mcc " MCC_VERSION "\"\n");
    fprintf(f, "\t.section\t.note.GNU-stack,\"\",@progbits\n");

    free_unique_names(s1);
    if (f != stdout)
        fclose(f);
    return 0;
}
