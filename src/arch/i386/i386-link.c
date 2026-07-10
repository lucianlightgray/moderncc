#include "mcc.h"

ST_FUNC int code_reloc(int reloc_type) {
	switch (reloc_type) {
	case R_386_RELATIVE:
	case R_386_16:
	case R_386_32:
	case R_386_GOTPC:
	case R_386_GOTOFF:
	case R_386_GOT32:
	case R_386_GOT32X:
	case R_386_GLOB_DAT:
	case R_386_COPY:
	case R_386_TLS_GD:
	case R_386_TLS_LDM:
	case R_386_TLS_LDO_32:
	case R_386_TLS_LE:
		return 0;

	case R_386_PC16:
	case R_386_PC32:
	case R_386_PLT32:
	case R_386_JMP_SLOT:
		return 1;
	}
	return -1;
}

ST_FUNC int gotplt_entry_type(int reloc_type) {
	switch (reloc_type) {
	case R_386_RELATIVE:
	case R_386_16:
	case R_386_GLOB_DAT:
	case R_386_JMP_SLOT:
	case R_386_COPY:
		return NO_GOTPLT_ENTRY;

	case R_386_32:
		return AUTO_GOTPLT_ENTRY;

	case R_386_PC16:
	case R_386_PC32:
		return AUTO_GOTPLT_ENTRY;

	case R_386_GOTPC:
	case R_386_GOTOFF:
		return BUILD_GOT_ONLY;

	case R_386_GOT32:
	case R_386_GOT32X:
	case R_386_PLT32:
	case R_386_TLS_GD:
	case R_386_TLS_LDM:
	case R_386_TLS_LDO_32:
	case R_386_TLS_LE:
		return ALWAYS_GOTPLT_ENTRY;
	}
	return -1;
}

ST_FUNC unsigned create_plt_entry(MCCState *s1, unsigned got_offset, struct sym_attr *attr) {
	Section *plt = s1->plt;
	uint8_t *p;
	int modrm;
	unsigned plt_offset, relofs;

	if (s1->output_type & MCC_OUTPUT_DYN)
		modrm = 0xa3;
	else
		modrm = 0x25;

	if (plt->data_offset == 0) {
		p = section_ptr_add(plt, 16);
		p[0] = 0xff;
		p[1] = modrm + 0x10;
		write32le(p + 2, PTR_SIZE);
		p[6] = 0xff;
		p[7] = modrm;
		write32le(p + 8, PTR_SIZE * 2);
	}
	plt_offset = plt->data_offset;

	relofs = s1->plt->reloc ? s1->plt->reloc->data_offset : 0;

	p = section_ptr_add(plt, 16);
	p[0] = 0xff;
	p[1] = modrm;
	write32le(p + 2, got_offset);
	p[6] = 0x68;
	write32le(p + 7, relofs - sizeof(ElfW_Rel));
	p[11] = 0xe9;
	write32le(p + 12, -(plt->data_offset));
	return plt_offset;
}

ST_FUNC void relocate_plt(MCCState *s1) {
	uint8_t *p, *p_end;

	if (!s1->plt)
		return;

	p = s1->plt->data;
	p_end = p + s1->plt->data_offset;

	if (!(s1->output_type & MCC_OUTPUT_DYN) && p < p_end) {
		add32le(p + 2, s1->got->sh_addr);
		add32le(p + 8, s1->got->sh_addr);
		p += 16;
		while (p < p_end) {
			add32le(p + 2, s1->got->sh_addr);
			p += 16;
		}
	}

	if (s1->plt->reloc) {
		ElfW_Rel *rel;
		int x = s1->plt->sh_addr + 16 + 6;
		p = s1->got->data;
		for_each_elem(s1->plt->reloc, 0, rel, ElfW_Rel) {
			write32le(p + rel->r_offset, x);
			x += 16;
		}
	}
}

ST_FUNC void relocate(MCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val) {
	int sym_index, esym_index;

	sym_index = ELFW(R_SYM)(rel->r_info);

	switch (type) {
	case R_386_32:
		if (s1->output_type & MCC_OUTPUT_DYN) {
			esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
			qrel->r_offset = rel->r_offset;
			if (esym_index) {
				qrel->r_info = ELFW(R_INFO)(esym_index, R_386_32);
				qrel++;
				return;
			} else {
				qrel->r_info = ELFW(R_INFO)(0, R_386_RELATIVE);
				qrel++;
			}
		}
		add32le(ptr, val);
		return;
	case R_386_PC32:
		if (s1->output_type == MCC_OUTPUT_DLL) {
			esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
			if (esym_index) {
				qrel->r_offset = rel->r_offset;
				qrel->r_info = ELFW(R_INFO)(esym_index, R_386_PC32);
				qrel++;
				return;
			}
		}
		add32le(ptr, val - addr);
		return;
	case R_386_PLT32:
		add32le(ptr, val - addr);
		return;
	case R_386_GLOB_DAT:
	case R_386_JMP_SLOT:
		write32le(ptr, val);
		return;
	case R_386_GOTPC:
		add32le(ptr, s1->got->sh_addr - addr);
		return;
	case R_386_GOTOFF:
		add32le(ptr, val - s1->got->sh_addr);
		return;
	case R_386_GOT32:
	case R_386_GOT32X:
		add32le(ptr, get_sym_attr(s1, sym_index, 0)->got_offset);
		return;
	case R_386_16:
		if (s1->output_format != MCC_OUTPUT_FORMAT_BINARY) {
		output_file:
			mcc_error_noabort("can only produce 16-bit binary files");
		}
		write16le(ptr, read16le(ptr) + val);
		return;
	case R_386_PC16:
		if (s1->output_format != MCC_OUTPUT_FORMAT_BINARY)
			goto output_file;
		write16le(ptr, read16le(ptr) + val - addr);
		return;
	case R_386_RELATIVE:
#ifdef MCC_TARGET_PE
		add32le(ptr, val - s1->pe_imagebase);
#endif
		return;
	case R_386_COPY:
		return;
	case R_386_TLS_GD: {
		static const unsigned char expect[] = {
				0x8d, 0x04, 0x1d, 0x00, 0x00, 0x00, 0x00,
				0xe8, 0xfc, 0xff, 0xff, 0xff};
		static const unsigned char replace[] = {
				0x65, 0xa1, 0x00, 0x00, 0x00, 0x00,
				0x81, 0xe8, 0x00, 0x00, 0x00, 0x00};

		if (memcmp(ptr - 3, expect, sizeof(expect)) == 0) {
			ElfW(Sym) * sym;
			Section *sec;
			int32_t x;

			memcpy(ptr - 3, replace, sizeof(replace));
			rel[1].r_info = ELFW(R_INFO)(0, R_386_NONE);
			sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];
			sec = s1->sections[sym->st_shndx];
			x = sym->st_value - sec->sh_addr - sec->data_offset;
			add32le(ptr + 5, -x);
		} else
			mcc_error_noabort("unexpected R_386_TLS_GD pattern");
	}
		return;
	case R_386_TLS_LDM: {
		static const unsigned char expect[] = {
				0x8d, 0x83, 0x00, 0x00, 0x00, 0x00,
				0xe8, 0xfc, 0xff, 0xff, 0xff};
		static const unsigned char replace[] = {
				0x65, 0xa1, 0x00, 0x00, 0x00, 0x00,
				0x90,
				0x8d, 0x74, 0x26, 0x00};

		if (memcmp(ptr - 2, expect, sizeof(expect)) == 0) {
			memcpy(ptr - 2, replace, sizeof(replace));
			rel[1].r_info = ELFW(R_INFO)(0, R_386_NONE);
		} else
			mcc_error_noabort("unexpected R_386_TLS_LDM pattern");
	}
		return;
	case R_386_TLS_LDO_32: {
		ElfW(Sym) * sym;
		Section *sec;
		int32_t x;

		sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];
		sec = s1->sections[sym->st_shndx];
		x = val - sec->sh_addr - sec->data_offset;
		add32le(ptr, x);
	}
		return;
	case R_386_TLS_LE: {
		ElfW(Sym) * sym;
		Section *sec;
		int32_t x;
		addr_t tls_start = 0, tls_end = 0, tls_align = 1;

		sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];
		sec = s1->sections[sym->st_shndx];

		for (int i = 1; i < s1->nb_sections; i++) {
			Section *s = s1->sections[i];
			addr_t ssz = s->sh_size ? s->sh_size : s->data_offset;
			if (s->sh_flags & SHF_TLS && ssz) {
				if (!tls_start || s->sh_addr < tls_start)
					tls_start = s->sh_addr;
				if (s->sh_addr + ssz > tls_end)
					tls_end = s->sh_addr + ssz;
				if (s->sh_addralign > tls_align)
					tls_align = s->sh_addralign;
			}
		}
		if (tls_end > tls_start) {
			addr_t tls_size = tls_end - tls_start;
			addr_t aligned_size = (tls_size + tls_align - 1) & ~(tls_align - 1);
#ifdef MCC_TARGET_PE
			(void)aligned_size;
			x = val - tls_start;
#else
			x = val - (tls_start + aligned_size);
#endif
		} else {
			x = val - sec->sh_addr - sec->data_offset;
		}
		add32le(ptr, x);
	}
		return;
	case R_386_NONE:
		return;
	default:
		mcc_error_noabort("unhandled relocation type %d at 0x%x (value 0x%x)",
											type, (unsigned)addr, (unsigned)val);
		return;
	}
}

