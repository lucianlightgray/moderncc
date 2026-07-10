#include "mcc.h"

ST_FUNC int code_reloc(int reloc_type) {
	switch (reloc_type) {
	case R_ARM_MOVT_ABS:
	case R_ARM_MOVW_ABS_NC:
	case R_ARM_THM_MOVT_ABS:
	case R_ARM_THM_MOVW_ABS_NC:
	case R_ARM_ABS32:
	case R_ARM_REL32:
	case R_ARM_GOTPC:
	case R_ARM_GOTOFF:
	case R_ARM_GOT32:
	case R_ARM_GOT_PREL:
	case R_ARM_COPY:
	case R_ARM_GLOB_DAT:
	case R_ARM_NONE:
	case R_ARM_TARGET1:
	case R_ARM_MOVT_PREL:
	case R_ARM_MOVW_PREL_NC:
	case R_ARM_TLS_LE32:
		return 0;

	case R_ARM_PC24:
	case R_ARM_CALL:
	case R_ARM_JUMP24:
	case R_ARM_PLT32:
	case R_ARM_THM_PC22:
	case R_ARM_THM_JUMP24:
	case R_ARM_PREL31:
	case R_ARM_V4BX:
	case R_ARM_JUMP_SLOT:
		return 1;
	}
	return -1;
}

ST_FUNC int gotplt_entry_type(int reloc_type) {
	switch (reloc_type) {
	case R_ARM_NONE:
	case R_ARM_COPY:
	case R_ARM_GLOB_DAT:
	case R_ARM_JUMP_SLOT:
	case R_ARM_TLS_LE32:
		return NO_GOTPLT_ENTRY;

	case R_ARM_PC24:
	case R_ARM_CALL:
	case R_ARM_JUMP24:
	case R_ARM_PLT32:
	case R_ARM_THM_PC22:
	case R_ARM_THM_JUMP24:
	case R_ARM_MOVT_ABS:
	case R_ARM_MOVW_ABS_NC:
	case R_ARM_THM_MOVT_ABS:
	case R_ARM_THM_MOVW_ABS_NC:
	case R_ARM_PREL31:
	case R_ARM_ABS32:
	case R_ARM_REL32:
	case R_ARM_V4BX:
	case R_ARM_TARGET1:
	case R_ARM_MOVT_PREL:
	case R_ARM_MOVW_PREL_NC:
		return AUTO_GOTPLT_ENTRY;

	case R_ARM_GOTPC:
	case R_ARM_GOTOFF:
		return BUILD_GOT_ONLY;

	case R_ARM_GOT32:
	case R_ARM_GOT_PREL:
		return ALWAYS_GOTPLT_ENTRY;
	}
	return -1;
}

ST_FUNC unsigned create_plt_entry(MCCState *s1, unsigned got_offset, struct sym_attr *attr) {
	Section *plt = s1->plt;
	uint8_t *p;
	unsigned plt_offset;

	if (plt->data_offset == 0) {
		p = section_ptr_add(plt, 20);
		write32le(p, 0xe52de004);
		write32le(p + 4, 0xe59fe004);
		write32le(p + 8, 0xe08fe00e);
		write32le(p + 12, 0xe5bef008);
	}
	plt_offset = plt->data_offset;

	if (attr->plt_thumb_stub) {
		p = section_ptr_add(plt, 4);
		write32le(p, 0x4778);
		write32le(p + 2, 0x46c0);
	}
	p = section_ptr_add(plt, 16);
	write32le(p + 4, got_offset);
	return plt_offset;
}

ST_FUNC void relocate_plt(MCCState *s1) {
	uint8_t *p, *p_end;

	if (!s1->plt)
		return;

	p = s1->plt->data;
	p_end = p + s1->plt->data_offset;

	if (p < p_end) {
		int x = s1->got->sh_addr - s1->plt->sh_addr - 12;
		write32le(s1->plt->data + 16, x - 4);
		p += 20;
		while (p < p_end) {
			unsigned off = x + read32le(p + 4) + (s1->plt->data - p) + 4;
			if (read32le(p) == 0x46c04778)
				p += 4;
			write32le(p, 0xe28fc200 | ((off >> 28) & 0xf));
			write32le(p + 4, 0xe28cc600 | ((off >> 20) & 0xff));
			write32le(p + 8, 0xe28cca00 | ((off >> 12) & 0xff));
			write32le(p + 12, 0xe5bcf000 | (off & 0xfff));
			p += 16;
		}
	}

	if (s1->plt->reloc) {
		ElfW_Rel *rel;
		p = s1->got->data;
		for_each_elem(s1->plt->reloc, 0, rel, ElfW_Rel) {
			write32le(p + rel->r_offset, s1->plt->sh_addr);
		}
	}
}

ST_FUNC void relocate(MCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val) {
	ElfW(Sym) * sym;
	int sym_index, esym_index;

	sym_index = ELFW(R_SYM)(rel->r_info);
	sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];

	switch (type) {
	case R_ARM_PC24:
	case R_ARM_CALL:
	case R_ARM_JUMP24:
	case R_ARM_PLT32: {
		int x, is_thumb, is_call, h, blx_avail, is_bl, th_ko;
		unsigned code = read32le(ptr);
		x = code & 0x00ffffff;
		if (g_debug & MCC_DBG_RELOC)
			printf("reloc %d: x=0x%x val=0x%x ", type, x, (unsigned)val);
		code &= 0xff000000;
		x <<= 2;
		if (x & 0x2000000)
			x -= 0x4000000;
		blx_avail = (CONFIG_MCC_CPUVER >= 5);
		is_thumb = val & 1;
		is_bl = code == 0xeb000000;
		is_call = (type == R_ARM_CALL || (type == R_ARM_PC24 && is_bl));
		x += val - addr;
		if (g_debug & MCC_DBG_RELOC)
			printf(" newx=0x%x name=%s\n", x,
						 (char *)symtab_section->link->data + sym->st_name);
		h = x & 2;
		th_ko = (x & 3) && (!blx_avail || !is_call);
		if (th_ko || x >= 0x2000000 || x < -0x2000000)
			mcc_error_noabort("can't relocate value at %x,%d", addr, type);
		x >>= 2;
		x &= 0xffffff;
		if (is_thumb) {
			x |= h << 24;
			code = 0xfa000000;
		}
		write32le(ptr, code | x);
	}
		return;
	case R_ARM_THM_PC22:
	case R_ARM_THM_JUMP24: {
		int x, hi, lo, s, j1, j2, i1, i2, imm10, imm11;
		int to_thumb, is_call, to_plt, blx_bit = 1 << 12;
		Section *plt;

		if (sym->st_shndx == SHN_UNDEF &&
				ELFW(ST_BIND)(sym->st_info) == STB_WEAK)
			return;

		hi = read16le(ptr);
		lo = read16le(ptr + 2);
		s = (hi >> 10) & 1;
		j1 = (lo >> 13) & 1;
		j2 = (lo >> 11) & 1;
		i1 = (j1 ^ s) ^ 1;
		i2 = (j2 ^ s) ^ 1;
		imm10 = hi & 0x3ff;
		imm11 = lo & 0x7ff;
		x = (s << 24) | (i1 << 23) | (i2 << 22) |
				(imm10 << 12) | (imm11 << 1);
		if (x & 0x01000000)
			x -= 0x02000000;

		to_thumb = val & 1;
		plt = s1->plt;
		to_plt = (val >= plt->sh_addr) &&
						 (val < plt->sh_addr + plt->data_offset);
		is_call = (type == R_ARM_THM_PC22);

		if (!to_thumb && !to_plt && !is_call) {
			int index;
			uint8_t *p;
			char *name, buf[1024];
			Section *text;

			name = (char *)symtab_section->link->data + sym->st_name;
			text = s1->sections[sym->st_shndx];
			snprintf(buf, sizeof(buf), "%s_from_thumb", name);
			index = put_elf_sym(symtab_section,
													text->data_offset + 1,
													sym->st_size, sym->st_info, 0,
													sym->st_shndx, buf);
			to_thumb = 1;
			val = text->data_offset + 1;
			rel->r_info = ELFW(R_INFO)(index, type);
			put_elf_reloc(symtab_section, text,
										text->data_offset + 4, R_ARM_JUMP24,
										sym_index);
			p = section_ptr_add(text, 8);
			write32le(p, 0x4778);
			write32le(p + 2, 0x46c0);
			write32le(p + 4, 0xeafffffe);
		}

		x += val - addr;
		if (!to_thumb && is_call) {
			blx_bit = 0;
			x = (x + 3) & -4;
		}

		if (!to_thumb || x >= 0x1000000 || x < -0x1000000)
			if (to_thumb || (val & 2) || (!is_call && !to_plt))
				mcc_error_noabort("can't relocate value at %x,%d", addr, type);

		s = (x >> 24) & 1;
		i1 = (x >> 23) & 1;
		i2 = (x >> 22) & 1;
		j1 = s ^ (i1 ^ 1);
		j2 = s ^ (i2 ^ 1);
		imm10 = (x >> 12) & 0x3ff;
		imm11 = (x >> 1) & 0x7ff;
		write16le(ptr, (hi & 0xf800) |
											 (s << 10) | imm10);
		write16le(ptr + 2, (lo & 0xc000) |
													 (j1 << 13) | blx_bit | (j2 << 11) |
													 imm11);
	}
		return;
	case R_ARM_MOVT_ABS:
	case R_ARM_MOVW_ABS_NC: {
		int x, imm4, imm12;
		if (type == R_ARM_MOVT_ABS)
			val >>= 16;
		imm12 = val & 0xfff;
		imm4 = (val >> 12) & 0xf;
		x = (imm4 << 16) | imm12;
		if (type == R_ARM_THM_MOVT_ABS)
			write32le(ptr, read32le(ptr) | x);
		else
			add32le(ptr, x);
	}
		return;
	case R_ARM_MOVT_PREL:
	case R_ARM_MOVW_PREL_NC: {
		int insn = read32le(ptr);
		int addend = ((insn >> 4) & 0xf000) | (insn & 0xfff);

		addend = (addend ^ 0x8000) - 0x8000;
		val += addend - addr;
		if (type == R_ARM_MOVT_PREL)
			val >>= 16;
		write32le(ptr, (insn & 0xfff0f000) |
											 ((val & 0xf000) << 4) | (val & 0xfff));
	}
		return;
	case R_ARM_THM_MOVT_ABS:
	case R_ARM_THM_MOVW_ABS_NC: {
		int x, i, imm4, imm3, imm8;
		if (type == R_ARM_THM_MOVT_ABS)
			val >>= 16;
		imm8 = val & 0xff;
		imm3 = (val >> 8) & 0x7;
		i = (val >> 11) & 1;
		imm4 = (val >> 12) & 0xf;
		x = (imm3 << 28) | (imm8 << 16) | (i << 10) | imm4;
		if (type == R_ARM_THM_MOVT_ABS)
			write32le(ptr, read32le(ptr) | x);
		else
			add32le(ptr, x);
	}
		return;
	case R_ARM_PREL31: {
		int x;
		x = read32le(ptr) & 0x7fffffff;
		write32le(ptr, read32le(ptr) & 0x80000000);
		x = (x * 2) / 2;
		x += val - addr;
		if ((x ^ (x >> 1)) & 0x40000000)
			mcc_error_noabort("can't relocate value at %x,%d", addr, type);
		write32le(ptr, read32le(ptr) | (x & 0x7fffffff));
	}
		return;
	case R_ARM_ABS32:
	case R_ARM_TARGET1:
		if (s1->output_type & MCC_OUTPUT_DYN) {
			esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
			qrel->r_offset = rel->r_offset;
			if (esym_index) {
				qrel->r_info = ELFW(R_INFO)(esym_index, R_ARM_ABS32);
				qrel++;
				return;
			} else {
				qrel->r_info = ELFW(R_INFO)(0, R_ARM_RELATIVE);
				qrel++;
			}
		}
		add32le(ptr, val);
		return;
	case R_ARM_REL32:
		add32le(ptr, val - addr);
		return;
	case R_ARM_GOTPC:
		add32le(ptr, s1->got->sh_addr - addr);
		return;
	case R_ARM_GOTOFF:
		add32le(ptr, val - s1->got->sh_addr);
		return;
	case R_ARM_GOT32:
		add32le(ptr, get_sym_attr(s1, sym_index, 0)->got_offset);
		return;
	case R_ARM_GOT_PREL:
		add32le(ptr, s1->got->sh_addr +
										 get_sym_attr(s1, sym_index, 0)->got_offset -
										 addr);
		return;
	case R_ARM_COPY:
		return;
	case R_ARM_V4BX:
		if ((0x0ffffff0 & read32le(ptr)) == 0x012FFF10)
			write32le(ptr, read32le(ptr) ^ 0xE12FFF10 ^ 0xE1A0F000);
		return;
	case R_ARM_GLOB_DAT:
	case R_ARM_JUMP_SLOT:
		write32le(ptr, val);
		return;
	case R_ARM_NONE:
		return;
	case R_ARM_RELATIVE:
#ifdef MCC_TARGET_PE
		add32le(ptr, val - s1->pe_imagebase);
#endif
		return;
	case R_ARM_TLS_LE32: {
		ElfW(Sym) * sym;
		Section *sec;
		int32_t x;
		addr_t tls_start = 0, tls_end = 0, tls_align = 1;

		sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];
		sec = s1->sections[sym->st_shndx];

		for (int i = 1; i < s1->nb_sections; i++) {
			Section *s = s1->sections[i];
			if (s->sh_flags & SHF_TLS && s->sh_size) {
				if (!tls_start || s->sh_addr < tls_start)
					tls_start = s->sh_addr;
				if (s->sh_addr + s->sh_size > tls_end)
					tls_end = s->sh_addr + s->sh_size;
				if (s->sh_addralign > tls_align)
					tls_align = s->sh_addralign;
			}
		}
		if (tls_end > tls_start) {
			x = val - tls_start + 8;
		} else {
			x = val - sec->sh_addr - sec->data_offset + 8;
		}
		add32le(ptr, x);
	}
		return;
	default:
		mcc_error_noabort("unhandled relocation type %d at 0x%x (value 0x%x)",
											type, (unsigned)addr, (unsigned)val);
		return;
	}
}

