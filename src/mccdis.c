#include "mcc.h"

#ifdef MCC_TARGET_ARM
#define TYPE_PFX "%"
#else
#define TYPE_PFX "@"
#endif

#define g_uniq (s1->disasm_uniq)

static void build_unique_names(MCCState *s1) { MCC_TRACE("enter\n");
	int n = s1->symtab->data_offset / sizeof(ElfW(Sym));
	ElfW(Sym) *syms = (ElfW(Sym) *)s1->symtab->data;
	const char *strtab = (char *)s1->symtab->link->data;
	g_uniq = mcc_mallocz(n * sizeof *g_uniq);
	for (int i = 1; i < n; i++) { MCC_TRACE("br\n");
		const char *name = strtab + syms[i].st_name;
		int dup = 0;
		if (!name[0])
			{ MCC_TRACE("br\n"); continue; }
		for (int j = 1; j < i; j++)
			{ MCC_TRACE("br\n"); if (syms[j].st_name && !strcmp(strtab + syms[j].st_name, name)) { MCC_TRACE("br\n");
				dup = 1;
				break;
			} }
		if (dup) { MCC_TRACE("br\n");
			char buf[256];
			snprintf(buf, sizeof buf, "%s.%d", name, i);
			g_uniq[i] = mcc_strdup(buf);
		}
	}
}

static void free_unique_names(MCCState *s1) { MCC_TRACE("enter\n");
	int n = s1->symtab->data_offset / sizeof(ElfW(Sym));
	for (int i = 0; i < n; i++)
		{ MCC_TRACE("br\n"); mcc_free(g_uniq[i]); }
	mcc_free(g_uniq);
	g_uniq = NULL;
}

static const char *sym_name(MCCState *s1, int sym_index) { MCC_TRACE("enter\n");
	ElfW(Sym) *sym = &((ElfW(Sym) *)s1->symtab->data)[sym_index];
	const char *name = (char *)s1->symtab->link->data + sym->st_name;
	if (g_uniq && g_uniq[sym_index])
		{ MCC_TRACE("br\n"); return g_uniq[sym_index]; }
	if (name && name[0])
		{ MCC_TRACE("br\n"); return name; }

	if (sym->st_shndx > 0 && sym->st_shndx < s1->nb_sections)
		{ MCC_TRACE("br\n"); return s1->sections[sym->st_shndx]->name; }
	return ".L.anon";
}

static int reloc_field_size(int type) { MCC_TRACE("enter\n");
#ifdef MCC_HAVE_DISASM
	return mcc_disasm_reloc_size(type);
#else
	(void)type;
	return 0;
#endif
}

ST_FUNC const char *disasm_reloc(disasm_ctx *dc, addr_t off, int size, int *ptype) { MCC_TRACE("enter\n");
	char *buf = dc->relocbuf;
	Section *sr = dc->sec->reloc;
	ElfW_Rel *rel;
	if (!sr)
		{ MCC_TRACE("br\n"); return NULL; }
	for_each_elem(sr, 0, rel, ElfW_Rel) {
		if (rel->r_offset < off || rel->r_offset >= off + size)
			{ MCC_TRACE("br\n"); continue; }
		{
			int type = ELFW(R_TYPE)(rel->r_info);
			int idx = ELFW(R_SYM)(rel->r_info);
			const char *name = sym_name(dc->s1, idx);
			long long addend;
			if (ptype)
				{ MCC_TRACE("br\n"); *ptype = type; }
			if (SHT_RELX == SHT_RELA) { MCC_TRACE("br\n");
				addend = ELFW_R_ADDEND(rel);
			} else { MCC_TRACE("br\n");
				addend = 0;
				if (reloc_field_size(type) == 4 && rel->r_offset + 4 <= dc->size)
					{ MCC_TRACE("br\n"); addend = (int32_t)read32le(dc->data + rel->r_offset); }
			}
#ifdef MCC_HAVE_DISASM

			addend += mcc_disasm_reloc_addend_bias(type, size);
#endif
			if (addend == 0)
				{ MCC_TRACE("br\n"); snprintf(buf, sizeof dc->relocbuf, "%s", name); }
			else if (addend > 0)
				{ MCC_TRACE("br\n"); snprintf(buf, sizeof dc->relocbuf, "%s+%lld", name, addend); }
			else
				{ MCC_TRACE("br\n"); snprintf(buf, sizeof dc->relocbuf, "%s-%lld", name, -addend); }
			return buf;
		}
	}
	return NULL;
}

#ifdef MCC_HAVE_DISASM
ST_FUNC const char *disasm_label(disasm_ctx *dc, addr_t target) { MCC_TRACE("enter\n");
	char *buf = dc->labelbuf;
	if (dc->collect && target < dc->size) { MCC_TRACE("br\n");
		int i;
		for (i = 0; i < dc->nlabels; i++)
			{ MCC_TRACE("br\n"); if (dc->labels[i] == target)
				{ MCC_TRACE("br\n"); break; } }
		if (i == dc->nlabels) { MCC_TRACE("br\n");
			if (dc->nlabels == dc->labels_cap) { MCC_TRACE("br\n");
				dc->labels_cap = dc->labels_cap ? dc->labels_cap * 2 : 16;
				dc->labels = mcc_realloc(dc->labels,
																 dc->labels_cap * sizeof *dc->labels);
			}
			dc->labels[dc->nlabels++] = target;
		}
	}
	snprintf(buf, sizeof dc->labelbuf, ".L%llx", (unsigned long long)target);
	return buf;
}

static int addr_cmp(const void *a, const void *b) { MCC_TRACE("enter\n");
	addr_t x = *(const addr_t *)a, y = *(const addr_t *)b;
	return x < y
						 ? -1
				 : x > y
						 ? 1
						 : 0;
}
#endif

struct secsym {
	addr_t value;
	ElfW(Sym) * sym;
	const char *name;
};

static int secsym_cmp(const void *a, const void *b) { MCC_TRACE("enter\n");
	const struct secsym *x = a, *y = b;
	if (x->value < y->value)
		{ MCC_TRACE("br\n"); return -1; }
	if (x->value > y->value)
		{ MCC_TRACE("br\n"); return 1; }

	return 0;
}

static struct secsym *collect_syms(MCCState *s1, int shndx, int *pn) { MCC_TRACE("enter\n");
	ElfW(Sym) *sym0 = (ElfW(Sym) *)s1->symtab->data;
	ElfW(Sym) * sym;
	int n = 0, cap = 0;
	struct secsym *v = NULL;
	for_each_elem(s1->symtab, 1, sym, ElfW(Sym)) {
		const char *name;
		if (sym->st_shndx != shndx)
			{ MCC_TRACE("br\n"); continue; }
		if (ELFW(ST_TYPE)(sym->st_info) == STT_SECTION || ELFW(ST_TYPE)(sym->st_info) == STT_FILE)
			{ MCC_TRACE("br\n"); continue; }
		name = sym_name(s1, (int)(sym - sym0));
		if (!name || !name[0])
			{ MCC_TRACE("br\n"); continue; }

		if (!strncmp(name, "<no name>", 9))
			{ MCC_TRACE("br\n"); continue; }
#if defined(MCC_TARGET_ARM) || defined(MCC_TARGET_ARM64)

		if (name[0] == '$')
			{ MCC_TRACE("br\n"); continue; }
#endif
		if (n == cap) { MCC_TRACE("br\n");
			cap = cap ? cap * 2 : 16;
			v = mcc_realloc(v, cap * sizeof *v);
		}
		v[n].value = sym->st_value;
		v[n].sym = sym;
		v[n].name = name;
		n++;
	}
	if (v)
		{ MCC_TRACE("br\n"); qsort(v, n, sizeof *v, secsym_cmp); }
	*pn = n;
	return v;
}

static void emit_sym_decl(FILE *f, ElfW(Sym) * sym, const char *name) { MCC_TRACE("enter\n");
	int bind = ELFW(ST_BIND)(sym->st_info);
	int type = ELFW(ST_TYPE)(sym->st_info);
	if (bind == STB_GLOBAL)
		{ MCC_TRACE("br\n"); fprintf(f, "\t.globl\t%s\n", name); }
	else if (bind == STB_WEAK)
		{ MCC_TRACE("br\n"); fprintf(f, "\t.weak\t%s\n", name); }
	if (type == STT_FUNC)
		{ MCC_TRACE("br\n"); fprintf(f, "\t.type\t%s, %sfunction\n", name, TYPE_PFX); }
	else if (type == STT_OBJECT)
		{ MCC_TRACE("br\n"); fprintf(f, "\t.type\t%s, %sobject\n", name, TYPE_PFX); }
}

#ifndef MCC_FDE_ENCODING
#define MCC_FDE_ENCODING (DW_EH_PE_udata4 | DW_EH_PE_signed | DW_EH_PE_pcrel)
#endif

struct cfi_note {
	addr_t pc;
	char text[64];
};

struct cfi_fde {
	addr_t start, end;
	int shndx;
	int note0, nnotes;
};

struct cfi_info {
	struct cfi_note *notes;
	int nnotes, notes_cap;
	struct cfi_fde *fdes;
	int nfdes, fdes_cap;
};

static void cfi_add_note(struct cfi_info *ci, addr_t pc, const char *fmt, ...) { MCC_TRACE("enter\n");
	struct cfi_note *n;
	va_list ap;
	if (ci->nnotes == ci->notes_cap) { MCC_TRACE("br\n");
		ci->notes_cap = ci->notes_cap ? ci->notes_cap * 2 : 32;
		ci->notes = mcc_realloc(ci->notes, ci->notes_cap * sizeof *ci->notes);
	}
	n = &ci->notes[ci->nnotes++];
	n->pc = pc;
	va_start(ap, fmt);
	vsnprintf(n->text, sizeof n->text, fmt, ap);
	va_end(ap);
}

static int cfi_fde_shndx(MCCState *s1, Section *eh, addr_t off) { MCC_TRACE("enter\n");
	Section *sr = eh->reloc;
	ElfW_Rel *rel;
	if (!sr)
		{ MCC_TRACE("br\n"); return 0; }
	for_each_elem(sr, 0, rel, ElfW_Rel) {
		if (rel->r_offset == off) { MCC_TRACE("br\n");
			int idx = ELFW(R_SYM)(rel->r_info);
			return ((ElfW(Sym) *)s1->symtab->data)[idx].st_shndx;
		}
	}
	return 0;
}

static int cfi_parse(MCCState *s1, struct cfi_info *ci) { MCC_TRACE("enter\n");
	Section *eh = eh_frame_section;
	unsigned char *base, *end, *p;
	unsigned long long code_align = 1;
	long long data_align = -1;
	int have_cie = 0;

	if (!eh || !eh->data_offset || !eh->data)
		{ MCC_TRACE("br\n"); return 0; }
	base = eh->data;
	end = base + eh->data_offset;
	p = base;
	while (end - p >= 4) { MCC_TRACE("br\n");
		unsigned char *q = p;
		unsigned int length = dwarf_read_4(q, end);
		unsigned char *eend;
		unsigned int id;

		if (length == 0) { MCC_TRACE("br\n");
			p = q;
			continue;
		}
		if (length > (size_t)(end - q))
			{ MCC_TRACE("br\n"); return 0; }
		eend = q + length;
		id = dwarf_read_4(q, eend);
		if (id == 0) { MCC_TRACE("br\n");
			unsigned int version = dwarf_read_1(q, eend);
			if (version != 1 && version != 3)
				{ MCC_TRACE("br\n"); return 0; }
			if (dwarf_read_1(q, eend) != 'z' || dwarf_read_1(q, eend) != 'R' || dwarf_read_1(q, eend) != 0)
				{ MCC_TRACE("br\n"); return 0; }
			code_align = dwarf_read_uleb128(&q, eend);
			data_align = dwarf_read_sleb128(&q, eend);
			dwarf_read_uleb128(&q, eend);
			if (dwarf_read_uleb128(&q, eend) != 1 || dwarf_read_1(q, eend) != MCC_FDE_ENCODING)
				{ MCC_TRACE("br\n"); return 0; }
			if (!code_align || !data_align)
				{ MCC_TRACE("br\n"); return 0; }
			have_cie = 1;
		} else { MCC_TRACE("br\n");
			addr_t start, off = 0;
			unsigned int range;
			unsigned long long auglen, u1, u2;
			int shndx, note0 = ci->nnotes;

			if (!have_cie)
				{ MCC_TRACE("br\n"); return 0; }
			shndx = cfi_fde_shndx(s1, eh, q - base);
			if (shndx <= 0)
				{ MCC_TRACE("br\n"); return 0; }
			start = dwarf_read_4(q, eend);
			range = dwarf_read_4(q, eend);
			auglen = dwarf_read_uleb128(&q, eend);
			if (auglen > (size_t)(eend - q))
				{ MCC_TRACE("br\n"); return 0; }
			q += auglen;
			while (q < eend) { MCC_TRACE("br\n");
				unsigned int op = dwarf_read_1(q, eend);
				switch (op >> 6) { MCC_TRACE("br\n");
				case 1:
					off += (addr_t)(op & 0x3f) * code_align;
					continue;
				case 2:
					u1 = dwarf_read_uleb128(&q, eend);
					cfi_add_note(ci, start + off, ".cfi_offset %u, %lld",
											 op & 0x3f, (long long)u1 * data_align);
					continue;
				case 3:
					cfi_add_note(ci, start + off, ".cfi_restore %u",
											 op & 0x3f);
					continue;
				}
				switch (op) { MCC_TRACE("br\n");
				case DW_CFA_nop:
					continue;
				case DW_CFA_advance_loc1:
					off += (addr_t)dwarf_read_1(q, eend) * code_align;
					continue;
				case DW_CFA_advance_loc2:
					off += (addr_t)dwarf_read_2(q, eend) * code_align;
					continue;
				case DW_CFA_advance_loc4:
					off += (addr_t)dwarf_read_4(q, eend) * code_align;
					continue;
				case DW_CFA_def_cfa:
					u1 = dwarf_read_uleb128(&q, eend);
					u2 = dwarf_read_uleb128(&q, eend);
					cfi_add_note(ci, start + off, ".cfi_def_cfa %llu, %llu",
											 u1, u2);
					continue;
				case DW_CFA_def_cfa_offset:
					u1 = dwarf_read_uleb128(&q, eend);
					cfi_add_note(ci, start + off, ".cfi_def_cfa_offset %llu",
											 u1);
					continue;
				case DW_CFA_def_cfa_register:
					u1 = dwarf_read_uleb128(&q, eend);
					cfi_add_note(ci, start + off,
											 ".cfi_def_cfa_register %llu", u1);
					continue;
				case DW_CFA_offset_extended:
					u1 = dwarf_read_uleb128(&q, eend);
					u2 = dwarf_read_uleb128(&q, eend);
					cfi_add_note(ci, start + off, ".cfi_offset %llu, %lld",
											 u1, (long long)u2 * data_align);
					continue;
				case DW_CFA_restore_extended:
					u1 = dwarf_read_uleb128(&q, eend);
					cfi_add_note(ci, start + off, ".cfi_restore %llu", u1);
					continue;
				default:
					return 0;
				}
			}
			if (off > range)
				{ MCC_TRACE("br\n"); return 0; }
			if (ci->nfdes == ci->fdes_cap) { MCC_TRACE("br\n");
				ci->fdes_cap = ci->fdes_cap ? ci->fdes_cap * 2 : 16;
				ci->fdes = mcc_realloc(ci->fdes,
															 ci->fdes_cap * sizeof *ci->fdes);
			}
			ci->fdes[ci->nfdes].start = start;
			ci->fdes[ci->nfdes].end = start + range;
			ci->fdes[ci->nfdes].shndx = shndx;
			ci->fdes[ci->nfdes].note0 = note0;
			ci->fdes[ci->nfdes].nnotes = ci->nnotes - note0;
			ci->nfdes++;
		}
		p = eend;
	}
	return ci->nfdes > 0;
}

static void emit_directive_header(FILE *f, Section *s) { MCC_TRACE("enter\n");
	const char *n = s->name;

	if (!strcmp(n, ".data.ro") || !strcmp(n, ".rdata"))
		{ MCC_TRACE("br\n"); n = ".rodata"; }
	if (!strcmp(n, ".text"))
		{ MCC_TRACE("br\n"); fprintf(f, "\t.text\n"); }
	else if (!strcmp(n, ".data"))
		{ MCC_TRACE("br\n"); fprintf(f, "\t.data\n"); }
	else if (!strcmp(n, ".bss"))
		{ MCC_TRACE("br\n"); fprintf(f, "\t.bss\n"); }
	else if (!strcmp(n, ".rodata"))
		{ MCC_TRACE("br\n"); fprintf(f, "\t.section\t.rodata,\"a\",%sprogbits\n", TYPE_PFX); }
	else { MCC_TRACE("br\n");
		fprintf(f, "\t.section\t%s", n);
		if (s->sh_type == SHT_NOBITS)
			{ MCC_TRACE("br\n"); fprintf(f, ",\"aw\",%snobits", TYPE_PFX); }
		else if (s->sh_flags & SHF_EXECINSTR)
			{ MCC_TRACE("br\n"); fprintf(f, ",\"ax\",%sprogbits", TYPE_PFX); }
		else if (s->sh_flags & SHF_WRITE)
			{ MCC_TRACE("br\n"); fprintf(f, ",\"aw\",%sprogbits", TYPE_PFX); }
		else
			{ MCC_TRACE("br\n"); fprintf(f, ",\"a\",%sprogbits", TYPE_PFX); }
		fprintf(f, "\n");
	}
	if (s->sh_addralign > 1)
		{ MCC_TRACE("br\n"); fprintf(f, "\t.align\t%d\n", s->sh_addralign); }
}

static void emit_text(disasm_ctx *dc, struct secsym *syms, int nsym,
											struct cfi_info *ci) { MCC_TRACE("enter\n");
	FILE *f = dc->out;
	addr_t pc, end = dc->size;
	int si, li;
	int fi = 0, ni = 0, in_proc = 0;

#ifdef MCC_HAVE_DISASM

	dc->collect = 1;
	for (pc = 0; pc < end;) { MCC_TRACE("br\n");
		int len;
		dc->pc = pc;
		len = mcc_disasm_insn(dc);
		pc += len < 1 ? 1 : len;
	}
	dc->collect = 0;
	if (dc->nlabels)
		{ MCC_TRACE("br\n"); qsort(dc->labels, dc->nlabels, sizeof *dc->labels, addr_cmp); }
#endif

	si = li = 0;
	for (pc = 0; pc < end;) { MCC_TRACE("br\n");
		if (ci) { MCC_TRACE("br\n");
			if (in_proc && pc >= ci->fdes[fi].end) { MCC_TRACE("br\n");
				while (ni < ci->fdes[fi].note0 + ci->fdes[fi].nnotes)
					{ MCC_TRACE("br\n"); fprintf(f, "\t%s\n", ci->notes[ni++].text); }
				fprintf(f, "\t.cfi_endproc\n");
				in_proc = 0;
				fi++;
			}
			if (!in_proc)
				{ MCC_TRACE("br\n"); while (fi < ci->nfdes && (ci->fdes[fi].shndx != dc->sec->sh_num || ci->fdes[fi].start < pc))
					{ MCC_TRACE("br\n"); fi++; } }
		}
		while (si < nsym && syms[si].value == pc) { MCC_TRACE("br\n");
			emit_sym_decl(f, syms[si].sym, syms[si].name);
			fprintf(f, "%s:\n", syms[si].name);
			si++;
		}
		while (li < dc->nlabels && dc->labels[li] == pc) { MCC_TRACE("br\n");
			fprintf(f, ".L%llx:\n", (unsigned long long)pc);
			li++;
		}
		while (li < dc->nlabels && dc->labels[li] < pc)
			{ MCC_TRACE("br\n"); li++; }
		if (ci) { MCC_TRACE("br\n");
			if (!in_proc && fi < ci->nfdes && ci->fdes[fi].shndx == dc->sec->sh_num && ci->fdes[fi].start == pc) { MCC_TRACE("br\n");
				fprintf(f, "\t.cfi_startproc\n");
				in_proc = 1;
				ni = ci->fdes[fi].note0;
			}
			if (in_proc)
				{ MCC_TRACE("br\n"); while (ni < ci->fdes[fi].note0 + ci->fdes[fi].nnotes && ci->notes[ni].pc <= pc)
					{ MCC_TRACE("br\n"); fprintf(f, "\t%s\n", ci->notes[ni++].text); } }
		}
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
	if (ci && in_proc) { MCC_TRACE("br\n");
		while (ni < ci->fdes[fi].note0 + ci->fdes[fi].nnotes)
			{ MCC_TRACE("br\n"); fprintf(f, "\t%s\n", ci->notes[ni++].text); }
		fprintf(f, "\t.cfi_endproc\n");
	}
}

static void emit_data(disasm_ctx *dc, struct secsym *syms, int nsym) { MCC_TRACE("enter\n");
	FILE *f = dc->out;
	addr_t pc = 0, end = dc->size;
	int si = 0;
	while (pc < end) { MCC_TRACE("br\n");
		int rtype = 0, sz;
		const char *r;
		while (si < nsym && syms[si].value == pc) { MCC_TRACE("br\n");
			emit_sym_decl(f, syms[si].sym, syms[si].name);
			fprintf(f, "%s:\n", syms[si].name);
			si++;
		}

		r = disasm_reloc(dc, pc, 1, &rtype);
		sz = r ? reloc_field_size(rtype) : 0;
		if (r && sz) { MCC_TRACE("br\n");
			fprintf(f, "\t%s\t%s\n", sz == 8 ? ".quad" : ".long", r);
			pc += sz;
			continue;
		}

		{
			addr_t run = pc;
			fprintf(f, "\t.byte\t");
			while (run < end) { MCC_TRACE("br\n");
				if (run != pc) { MCC_TRACE("br\n");
					if (si < nsym && syms[si].value == run)
						{ MCC_TRACE("br\n"); break; }
					if (dc->sec->reloc && disasm_reloc(dc, run, 1, &rtype))
						{ MCC_TRACE("br\n"); break; }
					fprintf(f, ", ");
				}
				fprintf(f, "0x%02x", dc->data[run]);
				run++;
				if (run - pc >= 16)
					{ MCC_TRACE("br\n"); break; }
			}
			fprintf(f, "\n");
			pc = run;
		}
	}
}

static void emit_sizes(FILE *f, struct secsym *syms, int nsym) { MCC_TRACE("enter\n");
	for (int i = 0; i < nsym; i++) { MCC_TRACE("br\n");
		int type = ELFW(ST_TYPE)(syms[i].sym->st_info);
		if ((type == STT_FUNC || type == STT_OBJECT) && syms[i].sym->st_size)
			{ MCC_TRACE("br\n"); fprintf(f, "\t.size\t%s, %llu\n", syms[i].name,
							(unsigned long long)syms[i].sym->st_size); }
	}
}

static int section_is_output(Section *s) { MCC_TRACE("enter\n");
	static const char *skip[] = {

			".eh_frame", ".note", ".comment", ".debug", ".stab", ".gnu", 0};
	int i;
	if (!(s->sh_flags & SHF_ALLOC))
		{ MCC_TRACE("br\n"); return 0; }
	if (s->sh_type != SHT_PROGBITS && s->sh_type != SHT_NOBITS)
		{ MCC_TRACE("br\n"); return 0; }
	if (s->data_offset == 0 && s->sh_type != SHT_NOBITS)
		{ MCC_TRACE("br\n"); return 0; }
	for (i = 0; skip[i]; i++)
		{ MCC_TRACE("br\n"); if (!strncmp(s->name, skip[i], strlen(skip[i])))
			{ MCC_TRACE("br\n"); return 0; } }
	return 1;
}

ST_FUNC int asm_output_file(MCCState *s1, const char *filename) { MCC_TRACE("enter\n");
	FILE *f;
	int i;
	struct cfi_info ci_data = {0}, *ci = NULL;

	if (!filename || !strcmp(filename, "-"))
		{ MCC_TRACE("br\n"); f = stdout; }
	else if (!(f = host_fopen(filename, "wb")))
		{ MCC_TRACE("br\n"); return mcc_error_noabort("could not write '%s'", filename); }

	fprintf(f, "\t.file\t\"%s\"\n",
					s1->nb_files ? mcc_basename(s1->files[0]->name) : "mcc");

	build_unique_names(s1);

	if (cfi_parse(s1, &ci_data))
		{ MCC_TRACE("br\n"); ci = &ci_data; }

	for (i = 1; i < s1->nb_sections; i++) { MCC_TRACE("br\n");
		Section *s = s1->sections[i];
		struct secsym *syms;
		int nsym;
		disasm_ctx dc = {0};

		if (!section_is_output(s))
			{ MCC_TRACE("br\n"); continue; }

		syms = collect_syms(s1, s->sh_num, &nsym);

		emit_directive_header(f, s);

		dc.s1 = s1;
		dc.sec = s;
		dc.symtab = s1->symtab;
		dc.data = s->data;
		dc.size = (s->sh_type == SHT_NOBITS) ? 0 : s->data_offset;
		dc.out = f;

		if (s->sh_type == SHT_NOBITS) { MCC_TRACE("br\n");
			int si = 0;
			addr_t pc = 0, end = s->data_offset;
			while (si < nsym) { MCC_TRACE("br\n");
				if (syms[si].value > pc && syms[si].value <= end) { MCC_TRACE("br\n");
					fprintf(f, "\t.skip\t%llu\n",
									(unsigned long long)(syms[si].value - pc));
					pc = syms[si].value;
				}
				emit_sym_decl(f, syms[si].sym, syms[si].name);
				fprintf(f, "%s:\n", syms[si].name);
				si++;
			}
			if (end > pc)
				{ MCC_TRACE("br\n"); fprintf(f, "\t.skip\t%llu\n", (unsigned long long)(end - pc)); }
		} else if (s->sh_flags & SHF_EXECINSTR) { MCC_TRACE("br\n");
			emit_text(&dc, syms, nsym, ci);
		} else { MCC_TRACE("br\n");
			emit_data(&dc, syms, nsym);
		}

		emit_sizes(f, syms, nsym);
		mcc_free(syms);
		mcc_free(dc.labels);
	}

	fprintf(f, "\t.ident\t\"mcc " MCC_VERSION_STR "\"\n");
	fprintf(f, "\t.section\t.note.GNU-stack,\"\",%sprogbits\n", TYPE_PFX);

	mcc_free(ci_data.notes);
	mcc_free(ci_data.fdes);
	free_unique_names(s1);
	if (f != stdout)
		{ MCC_TRACE("br\n"); fclose(f); }
	return 0;
}
