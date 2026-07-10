#ifdef TARGET_DEFS_ONLY

#define EM_MCC_TARGET EM_AARCH64

#define R_DATA_32 R_AARCH64_ABS32
#define R_DATA_PTR R_AARCH64_ABS64
#define R_JMP_SLOT R_AARCH64_JUMP_SLOT
#define R_GLOB_DAT R_AARCH64_GLOB_DAT
#define R_COPY R_AARCH64_COPY
#define R_RELATIVE R_AARCH64_RELATIVE

#define R_NUM R_AARCH64_NUM

#define ELF_START_ADDR 0x00400000
#define ELF_PAGE_SIZE 0x10000

#define PCRELATIVE_DLLPLT 1
#define RELOCATE_DLLPLT 1

#else

#include "mcc.h"

#define AARCH64_TLS_TCB_SIZE 16

#ifdef NEED_RELOC_TYPE
ST_FUNC int code_reloc(int reloc_type) {
	switch (reloc_type) {
	case R_AARCH64_ABS32:
	case R_AARCH64_ABS64:
	case R_AARCH64_PREL32:
	case R_AARCH64_MOVW_UABS_G0_NC:
	case R_AARCH64_MOVW_UABS_G1_NC:
	case R_AARCH64_MOVW_UABS_G2_NC:
	case R_AARCH64_MOVW_UABS_G3:
	case R_AARCH64_ADR_PREL_PG_HI21:
	case R_AARCH64_ADD_ABS_LO12_NC:
	case R_AARCH64_ADR_GOT_PAGE:
	case R_AARCH64_LD64_GOT_LO12_NC:
	case R_AARCH64_LDST128_ABS_LO12_NC:
	case R_AARCH64_LDST64_ABS_LO12_NC:
	case R_AARCH64_LDST32_ABS_LO12_NC:
	case R_AARCH64_LDST16_ABS_LO12_NC:
	case R_AARCH64_LDST8_ABS_LO12_NC:
	case R_AARCH64_TLSLE_ADD_TPREL_HI12:
	case R_AARCH64_TLSLE_ADD_TPREL_LO12:
	case R_AARCH64_GLOB_DAT:
	case R_AARCH64_COPY:
		return 0;

	case R_AARCH64_JUMP26:
	case R_AARCH64_CALL26:
	case R_AARCH64_JUMP_SLOT:
	case R_AARCH64_CONDBR19:
	case R_AARCH64_TSTBR14:
		return 1;
	}
	return -1;
}

ST_FUNC int gotplt_entry_type(int reloc_type) {
	switch (reloc_type) {
	case R_AARCH64_PREL32:
	case R_AARCH64_MOVW_UABS_G0_NC:
	case R_AARCH64_MOVW_UABS_G1_NC:
	case R_AARCH64_MOVW_UABS_G2_NC:
	case R_AARCH64_MOVW_UABS_G3:
	case R_AARCH64_ADR_PREL_PG_HI21:
	case R_AARCH64_ADD_ABS_LO12_NC:
	case R_AARCH64_LDST128_ABS_LO12_NC:
	case R_AARCH64_LDST64_ABS_LO12_NC:
	case R_AARCH64_LDST32_ABS_LO12_NC:
	case R_AARCH64_LDST16_ABS_LO12_NC:
	case R_AARCH64_LDST8_ABS_LO12_NC:
	case R_AARCH64_GLOB_DAT:
	case R_AARCH64_JUMP_SLOT:
	case R_AARCH64_COPY:
	case R_AARCH64_CONDBR19:
	case R_AARCH64_TSTBR14:
	case R_AARCH64_TLSLE_ADD_TPREL_HI12:
	case R_AARCH64_TLSLE_ADD_TPREL_LO12:
		return NO_GOTPLT_ENTRY;

	case R_AARCH64_ABS32:
	case R_AARCH64_ABS64:
	case R_AARCH64_JUMP26:
	case R_AARCH64_CALL26:
		return AUTO_GOTPLT_ENTRY;

	case R_AARCH64_ADR_GOT_PAGE:
	case R_AARCH64_LD64_GOT_LO12_NC:
		return ALWAYS_GOTPLT_ENTRY;
	}
	return -1;
}

#ifdef NEED_BUILD_GOT
ST_FUNC unsigned create_plt_entry(MCCState *s1, unsigned got_offset, struct sym_attr *attr) {
	Section *plt = s1->plt;
	uint8_t *p;
	unsigned plt_offset;

	if (plt->data_offset == 0) {
		section_ptr_add(plt, 32);
	}
	plt_offset = plt->data_offset;

	p = section_ptr_add(plt, 16);
	write32le(p, got_offset);
	write32le(p + 4, (uint64_t)got_offset >> 32);
	return plt_offset;
}

ST_FUNC void relocate_plt(MCCState *s1) {
	uint8_t *p, *p_end;

	if (!s1->plt)
		return;

	p = s1->plt->data;
	p_end = p + s1->plt->data_offset;

	if (p < p_end) {
		uint64_t plt = s1->plt->sh_addr;
		uint64_t got = s1->got->sh_addr + 16;
		uint64_t off = (got >> 12) - (plt >> 12);
		if ((off + ((uint32_t)1 << 20)) >> 21)
			mcc_error_noabort("Failed relocating PLT (off=0x%lx, got=0x%lx, plt=0x%lx)", (long)off, (long)got,
												(long)plt);
		write32le(p, ARM64_STP_X_PRE | ARM64_RT(16) | ARM64_RT2(30) |
										 ARM64_RN(31) | ARM64_IMM7(-2));
		write32le(p + 4, (ARM64_ADRP | ARM64_RD(16) |
											(off & 0x1ffffc) << 3 | (off & 3) << 29));
		write32le(p + 8, (ARM64_LDR_X | ARM64_RT(17) | ARM64_RN(16) |
											(got & 0xff8) << 7));
		write32le(p + 12, (ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RD(16) | ARM64_RN(16) |
											 (got & 0xfff) << 10));
		write32le(p + 16, ARM64_BR | ARM64_RN(17));
		write32le(p + 20, ARM64_NOP);
		write32le(p + 24, ARM64_NOP);
		write32le(p + 28, ARM64_NOP);
		p += 32;
		got = s1->got->sh_addr;
		while (p < p_end) {
			uint64_t pc = plt + (p - s1->plt->data);
			uint64_t addr = got + read64le(p);
			uint64_t off = (addr >> 12) - (pc >> 12);
			if ((off + ((uint32_t)1 << 20)) >> 21)
				mcc_error_noabort("Failed relocating PLT (off=0x%lx, addr=0x%lx, pc=0x%lx)", (long)off, (long)addr,
													(long)pc);
			write32le(p, (ARM64_ADRP | ARM64_RD(16) |
										(off & 0x1ffffc) << 3 | (off & 3) << 29));
			write32le(p + 4, (ARM64_LDR_X | ARM64_RT(17) | ARM64_RN(16) |
												(addr & 0xff8) << 7));
			write32le(p + 8, (ARM64_ADD_IMM | ARM64_SF(1) | ARM64_RD(16) | ARM64_RN(16) |
												(addr & 0xfff) << 10));
			write32le(p + 12, ARM64_BR | ARM64_RN(17));
			p += 16;
		}
	}

	if (s1->plt->reloc) {
		ElfW_Rel *rel;
		p = s1->got->data;
		for_each_elem(s1->plt->reloc, 0, rel, ElfW_Rel) {
			write64le(p + rel->r_offset, s1->plt->sh_addr);
		}
	}
}
#endif
#endif

ST_FUNC void relocate(MCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr, addr_t addr, addr_t val) {
	int sym_index = ELFW(R_SYM)(rel->r_info), esym_index;
	ElfW(Sym) *sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];

	switch (type) {
	case R_AARCH64_ABS64:
		if ((s1->output_type & MCC_OUTPUT_DYN)) {
			esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
			qrel->r_offset = rel->r_offset;
			if (esym_index) {
				qrel->r_info = ELFW(R_INFO)(esym_index, R_AARCH64_ABS64);
				qrel->r_addend = rel->r_addend;
				qrel++;
				break;
			} else {
				qrel->r_info = ELFW(R_INFO)(0, R_AARCH64_RELATIVE);
				qrel->r_addend = read64le(ptr) + val;
				qrel++;
			}
		}
		add64le(ptr, val);
		return;
	case R_AARCH64_ABS32:
		if (s1->output_type & MCC_OUTPUT_DYN) {
			qrel->r_offset = rel->r_offset;
			qrel->r_info = ELFW(R_INFO)(0, R_AARCH64_RELATIVE);
			qrel->r_addend = (int)read32le(ptr) + val;
			qrel++;
		}
		add32le(ptr, val);
		return;
	case R_AARCH64_PREL32:
		if (s1->output_type == MCC_OUTPUT_DLL) {
			esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
			if (esym_index) {
				qrel->r_offset = rel->r_offset;
				qrel->r_info = ELFW(R_INFO)(esym_index, R_AARCH64_PREL32);
				qrel->r_addend = (int)read32le(ptr) + rel->r_addend;
				qrel++;
				break;
			}
		}
		add32le(ptr, val - addr);
		return;
	case R_AARCH64_MOVW_UABS_G0_NC:
		write32le(ptr, ((read32le(ptr) & 0xffe0001f) |
										(val & 0xffff) << 5));
		return;
	case R_AARCH64_MOVW_UABS_G1_NC:
		write32le(ptr, ((read32le(ptr) & 0xffe0001f) |
										(val >> 16 & 0xffff) << 5));
		return;
	case R_AARCH64_MOVW_UABS_G2_NC:
		write32le(ptr, ((read32le(ptr) & 0xffe0001f) |
										(val >> 32 & 0xffff) << 5));
		return;
	case R_AARCH64_MOVW_UABS_G3:
		write32le(ptr, ((read32le(ptr) & 0xffe0001f) |
										(val >> 48 & 0xffff) << 5));
		return;
	case R_AARCH64_ADR_PREL_PG_HI21: {
		uint64_t off = (val >> 12) - (addr >> 12);
#ifdef MCC_TARGET_PE
		if ((off + ((uint64_t)1 << 20)) >> 21) {
			if (sym->st_shndx == SHN_UNDEF && ELFW(ST_BIND)(sym->st_info) == STB_WEAK) {
				write32le(ptr, 0xd2800000 | (read32le(ptr) & 0x1f));
				return;
			}
			mcc_error_noabort("R_AARCH64_ADR_PREL_PG_HI21 relocation failed");
		}
#else
		if ((off + ((uint64_t)1 << 20)) >> 21)
			mcc_error_noabort("R_AARCH64_ADR_PREL_PG_HI21 relocation failed");
#endif
		write32le(ptr, ((read32le(ptr) & 0x9f00001f) |
										(off & 0x1ffffc) << 3 | (off & 3) << 29));
		return;
	}
	case R_AARCH64_ADD_ABS_LO12_NC:
	case R_AARCH64_LDST8_ABS_LO12_NC:
		write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
										(val & 0xfff) << 10));
		return;
	case R_AARCH64_LDST16_ABS_LO12_NC:
		write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
										(val & 0xffe) << 9));
		return;
	case R_AARCH64_LDST32_ABS_LO12_NC:
		write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
										(val & 0xffc) << 8));
		return;
	case R_AARCH64_LDST64_ABS_LO12_NC:
		write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
										(val & 0xff8) << 7));
		return;
	case R_AARCH64_LDST128_ABS_LO12_NC:
		write32le(ptr, ((read32le(ptr) & 0xffc003ff) |
										(val & 0xff0) << 6));
		return;
	case R_AARCH64_CONDBR19:
		if (g_debug & MCC_DBG_RELOC)
			printf("reloc %d @ 0x%lx: val=0x%lx name=%s\n", type, (long)addr, (long)val,
						 (char *)symtab_section->link->data + sym->st_name);
		if (((val - addr) + ((uint64_t)1 << 20)) & ~(uint64_t)0x1ffffc)
			mcc_error_noabort("R_AARCH64_CONDBR19 relocation failed"
												" (val=%lx, addr=%lx)",
												(long)val, (long)addr);
		write32le(ptr, ((read32le(ptr) & 0xff00001f) |
										(((val - addr) >> 2 & 0x7ffff) << 5)));
		return;
	case R_AARCH64_TSTBR14:
		if (g_debug & MCC_DBG_RELOC)
			printf("reloc %d @ 0x%lx: val=0x%lx name=%s\n", type, (long)addr, (long)val,
						 (char *)symtab_section->link->data + sym->st_name);
		if (((val - addr) + ((uint64_t)1 << 15)) & ~(uint64_t)0xfffc)
			mcc_error_noabort("R_AARCH64_TSTBR14 relocation failed"
												" (val=%lx, addr=%lx)",
												(long)val, (long)addr);
		write32le(ptr, ((read32le(ptr) & 0xfff8001f) |
										(((val - addr) >> 2 & 0x3fff) << 5)));
		return;
	case R_AARCH64_JUMP26:
	case R_AARCH64_CALL26: {
		const char *name;
		if (g_debug & MCC_DBG_RELOC)
			printf("reloc %d @ 0x%lx: val=0x%lx name=%s\n", type, (long)addr, (long)val,
						 (char *)symtab_section->link->data + sym->st_name);
		if (((val - addr) + ((uint64_t)1 << 27)) & ~(uint64_t)0xffffffc) {
#ifdef MCC_TARGET_PE
			if (sym->st_shndx == SHN_UNDEF && ELFW(ST_BIND)(sym->st_info) == STB_WEAK) {
				write32le(ptr, ARM64_NOP);
				return;
			}
#endif
			name = (char *)symtab_section->link->data +
						 ((ElfW(Sym) *)symtab_section->data)[sym_index].st_name;
			mcc_error_noabort("R_AARCH64_(JUMP|CALL)26 relocation failed"
												" for '%s' (val=%lx, addr=%lx)",
												name, (long)val, (long)addr);
		}
		write32le(ptr, (0x14000000 |
										(uint32_t)(type == R_AARCH64_CALL26) << 31 |
										((val - addr) >> 2 & 0x3ffffff)));
		return;
	}
	case R_AARCH64_ADR_GOT_PAGE: {
		uint64_t off =
				(((s1->got->sh_addr +
					 get_sym_attr(s1, sym_index, 0)->got_offset) >>
					12) -
				 (addr >> 12));
		if ((off + ((uint64_t)1 << 20)) >> 21)
			mcc_error_noabort("R_AARCH64_ADR_GOT_PAGE relocation failed");
		write32le(ptr, ((read32le(ptr) & 0x9f00001f) |
										(off & 0x1ffffc) << 3 | (off & 3) << 29));
		return;
	}
	case R_AARCH64_LD64_GOT_LO12_NC:
		write32le(ptr,
							((read32le(ptr) & 0xfff803ff) |
							 ((s1->got->sh_addr +
								 get_sym_attr(s1, sym_index, 0)->got_offset) &
								0xff8)
									 << 7));
		return;
	case R_AARCH64_COPY:
		return;
	case R_AARCH64_GLOB_DAT:
	case R_AARCH64_JUMP_SLOT:
		if (g_debug & MCC_DBG_RELOC)
			printf("reloc %d @ 0x%lx: val=0x%lx name=%s\n", type, (long)addr,
						 (long)(val - rel->r_addend),
						 (char *)symtab_section->link->data + sym->st_name);
		write64le(ptr, val - rel->r_addend);
		return;
	case R_AARCH64_TLSLE_ADD_TPREL_HI12:
	case R_AARCH64_TLSLE_ADD_TPREL_LO12: {
		addr_t tls_start = 0;
		for (int i = 1; i < s1->nb_sections; i++) {
			Section *s = s1->sections[i];

			addr_t ssz = s->sh_size ? s->sh_size : s->data_offset;
			if (s->sh_flags & SHF_TLS && ssz) {
				if (!tls_start || s->sh_addr < tls_start)
					tls_start = s->sh_addr;
			}
		}
#ifdef MCC_TARGET_PE

		int64_t tp_offset = val - tls_start;
#else

		int64_t tp_offset = val - tls_start + AARCH64_TLS_TCB_SIZE;
#endif
		int64_t imm;
		if (type == R_AARCH64_TLSLE_ADD_TPREL_HI12)
			imm = (tp_offset >> 12) & 0xfff;
		else
			imm = tp_offset & 0xfff;
		write32le(ptr, ((read32le(ptr) & 0xffc003ff) | (imm << 10)));
		return;
	}
	case R_AARCH64_RELATIVE:
#ifdef MCC_TARGET_PE
		add32le(ptr, val - s1->pe_imagebase);
#endif
		return;
	default:
		mcc_error_noabort("unhandled relocation type 0x%x at 0x%x (value 0x%x)",
											(unsigned)type, (unsigned)addr, (unsigned)val);
		return;
	}
}

#endif
