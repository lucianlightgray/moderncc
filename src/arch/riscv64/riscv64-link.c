#include "mcc.h"

ST_FUNC int code_reloc(int reloc_type) { MCC_TRACE("enter\n");
	switch (reloc_type) { MCC_TRACE("br\n");
	case R_RISCV_BRANCH:
	case R_RISCV_CALL:
	case R_RISCV_JAL:
		return 1;

	case R_RISCV_GOT_HI20:
	case R_RISCV_PCREL_HI20:
	case R_RISCV_PCREL_LO12_I:
	case R_RISCV_PCREL_LO12_S:
	case R_RISCV_32_PCREL:
	case R_RISCV_SET6:
	case R_RISCV_SET8:
	case R_RISCV_SET16:
	case R_RISCV_SUB6:
	case R_RISCV_ADD16:
	case R_RISCV_ADD32:
	case R_RISCV_ADD64:
	case R_RISCV_SUB8:
	case R_RISCV_SUB16:
	case R_RISCV_SUB32:
	case R_RISCV_SUB64:
	case R_RISCV_32:
	case R_RISCV_64:
	case R_RISCV_SET_ULEB128:
	case R_RISCV_SUB_ULEB128:
	case R_RISCV_TPREL_HI20:
	case R_RISCV_TPREL_LO12_I:
		return 0;

	case R_RISCV_CALL_PLT:
		return 1;
	}
	return -1;
}

ST_FUNC int gotplt_entry_type(int reloc_type) { MCC_TRACE("enter\n");
	switch (reloc_type) { MCC_TRACE("br\n");
	case R_RISCV_ALIGN:
	case R_RISCV_RELAX:
	case R_RISCV_RVC_BRANCH:
	case R_RISCV_RVC_JUMP:
	case R_RISCV_JUMP_SLOT:
	case R_RISCV_SET6:
	case R_RISCV_SET8:
	case R_RISCV_SET16:
	case R_RISCV_SUB6:
	case R_RISCV_ADD16:
	case R_RISCV_SUB8:
	case R_RISCV_SUB16:
	case R_RISCV_SET_ULEB128:
	case R_RISCV_SUB_ULEB128:
		return NO_GOTPLT_ENTRY;

	case R_RISCV_BRANCH:
	case R_RISCV_CALL:
	case R_RISCV_PCREL_HI20:
	case R_RISCV_PCREL_LO12_I:
	case R_RISCV_PCREL_LO12_S:
	case R_RISCV_32_PCREL:
	case R_RISCV_ADD32:
	case R_RISCV_ADD64:
	case R_RISCV_SUB32:
	case R_RISCV_SUB64:
	case R_RISCV_32:
	case R_RISCV_64:
	case R_RISCV_JAL:
	case R_RISCV_CALL_PLT:
		return AUTO_GOTPLT_ENTRY;

	case R_RISCV_GOT_HI20:
		return ALWAYS_GOTPLT_ENTRY;

	case R_RISCV_TPREL_HI20:
	case R_RISCV_TPREL_LO12_I:
		return NO_GOTPLT_ENTRY;
	}
	return -1;
}

ST_FUNC unsigned create_plt_entry(MCCState *s1, unsigned got_offset, struct sym_attr *attr) { MCC_TRACE("enter\n");
	Section *plt = s1->plt;
	uint8_t *p;
	unsigned plt_offset;

	if (plt->data_offset == 0)
		{ MCC_TRACE("br\n"); section_ptr_add(plt, 32); }
	plt_offset = plt->data_offset;

	p = section_ptr_add(plt, 16);
	write64le(p, got_offset);
	return plt_offset;
}

ST_FUNC void relocate_plt(MCCState *s1) { MCC_TRACE("enter\n");
	uint8_t *p, *p_end;

	if (!s1->plt)
		{ MCC_TRACE("br\n"); return; }

	p = s1->plt->data;
	p_end = p + s1->plt->data_offset;

	if (p < p_end) { MCC_TRACE("br\n");
		uint64_t plt = s1->plt->sh_addr;
		uint64_t got = s1->got->sh_addr;
		uint64_t off = (got - plt + 0x800) >> 12;
		if ((off + ((uint32_t)1 << 20)) >> 21)
			{ MCC_TRACE("br\n"); mcc_error_noabort("Failed relocating PLT (off=0x%lx, got=0x%lx, plt=0x%lx)", (long)off, (long)got,
												(long)plt); }
		write32le(p, 0x397 | (off << 12));
		write32le(p + 4, 0x41c30333);
		write32le(p + 8, 0x0003be03 | (((got - plt) & 0xfff) << 20));
		write32le(p + 12, 0xfd430313);
		write32le(p + 16, 0x00038293 | (((got - plt) & 0xfff) << 20));
		write32le(p + 20, 0x00135313);
		write32le(p + 24, 0x0082b283);
		write32le(p + 28, 0x000e0067);
		p += 32;
		while (p < p_end) { MCC_TRACE("br\n");
			uint64_t pc = plt + (p - s1->plt->data);
			uint64_t addr = got + read64le(p);
			uint64_t off = (addr - pc + 0x800) >> 12;
			if ((off + ((uint32_t)1 << 20)) >> 21)
				{ MCC_TRACE("br\n"); mcc_error_noabort("Failed relocating PLT (off=0x%lx, addr=0x%lx, pc=0x%lx)", (long)off, (long)addr,
													(long)pc); }
			write32le(p, 0xe17 | (off << 12));
			write32le(p + 4, 0x000e3e03 | (((addr - pc) & 0xfff) << 20));
			write32le(p + 8, 0x000e0367);
			write32le(p + 12, 0x00000013);
			p += 16;
		}
	}

	if (s1->plt->reloc) { MCC_TRACE("br\n");
		ElfW_Rel *rel;
		p = s1->got->data;
		for_each_elem(s1->plt->reloc, 0, rel, ElfW_Rel) {
			write64le(p + rel->r_offset, s1->plt->sh_addr);
		}
	}
}

static void riscv64_record_pcrel_hi(MCCState *s1, addr_t addr, addr_t val) { MCC_TRACE("enter\n");
	struct pcrel_hi *entry = mcc_malloc(sizeof *entry);
	entry->addr = addr;
	entry->val = val;
	dynarray_add(&s1->pcrel_hi_entries, &s1->nb_pcrel_hi_entries, entry);
}

static int riscv64_lookup_pcrel_hi(MCCState *s1, addr_t hi_addr, addr_t *hi_val) { MCC_TRACE("enter\n");
	for (int i = s1->nb_pcrel_hi_entries; i > 0;) { MCC_TRACE("br\n");
		struct pcrel_hi *entry = s1->pcrel_hi_entries[--i];
		if (entry->addr == hi_addr) { MCC_TRACE("br\n");
			*hi_val = entry->val;
			return 0;
		}
	}
	return mcc_error_noabort("unsupported hi/lo pcrel reloc scheme");
}

ST_FUNC void relocate(MCCState *s1, ElfW_Rel *rel, int type, unsigned char *ptr,
											addr_t addr, addr_t val) { MCC_TRACE("enter\n");
	uint64_t off64;
	uint32_t off32;
	int sym_index = ELFW(R_SYM)(rel->r_info), esym_index;
	ElfW(Sym) *sym = &((ElfW(Sym) *)symtab_section->data)[sym_index];

	switch (type) { MCC_TRACE("br\n");
	case R_RISCV_ALIGN:
	case R_RISCV_RELAX:
		return;

	case R_RISCV_BRANCH:
		off64 = val - addr;
		if ((off64 + (1 << 12)) & ~(uint64_t)0x1ffe)
			{ MCC_TRACE("br\n"); mcc_error_noabort("R_RISCV_BRANCH relocation failed"
												" (val=%lx, addr=%lx)",
												(long)val, (long)addr); }
		off32 = off64 >> 1;
		write32le(
				ptr, (read32le(ptr) & ~0xfe000f80) | ((off32 & 0x800) << 20) | ((off32 & 0x3f0) << 21) | ((off32 & 0x00f) << 8) | ((off32 & 0x400) >> 3));
		return;
	case R_RISCV_JAL:
		off64 = val - addr;
		if ((off64 + (1 << 21)) & ~(((uint64_t)1 << 22) - 2))
			{ MCC_TRACE("br\n"); mcc_error_noabort("R_RISCV_JAL relocation failed"
												" (val=%lx, addr=%lx)",
												(long)val, (long)addr); }
		off32 = off64;
		write32le(
				ptr, (read32le(ptr) & 0xfff) | (((off32 >> 12) & 0xff) << 12) | (((off32 >> 11) & 1) << 20) | (((off32 >> 1) & 0x3ff) << 21) | (((off32 >> 20) & 1) << 31));
		return;
	case R_RISCV_CALL:
	case R_RISCV_CALL_PLT:
		write32le(ptr, (read32le(ptr) & 0xfff) | ((val - addr + 0x800) & ~0xfff));
		write32le(ptr + 4, (read32le(ptr + 4) & 0xfffff) | (((val - addr) & 0xfff) << 20));
		return;
	case R_RISCV_PCREL_HI20:
		if (g_debug & MCC_DBG_RELOC)
			{ MCC_TRACE("br\n"); printf("PCREL_HI20: val=%lx addr=%lx\n", (long)val, (long)addr); }
		off64 = (int64_t)(val - addr + 0x800) >> 12;
		if ((off64 + ((uint64_t)1 << 20)) >> 21)
			{ MCC_TRACE("br\n"); mcc_error_noabort("R_RISCV_PCREL_HI20 relocation failed: off=%lx cond=%lx sym=%s",
												(long)off64, (long)((int64_t)(off64 + ((uint64_t)1 << 20)) >> 21),
												symtab_section->link->data + sym->st_name); }
		write32le(ptr, (read32le(ptr) & 0xfff) | ((off64 & 0xfffff) << 12));
		riscv64_record_pcrel_hi(s1, addr, val);
		return;
	case R_RISCV_GOT_HI20:
		val = s1->got->sh_addr + get_sym_attr(s1, sym_index, 0)->got_offset;
		off64 = (int64_t)(val - addr + 0x800) >> 12;
		if ((off64 + ((uint64_t)1 << 20)) >> 21)
			{ MCC_TRACE("br\n"); mcc_error_noabort("R_RISCV_GOT_HI20 relocation failed"); }
		write32le(ptr, (read32le(ptr) & 0xfff) | ((off64 & 0xfffff) << 12));
		riscv64_record_pcrel_hi(s1, addr, val);
		return;
	case R_RISCV_PCREL_LO12_I:
		if (g_debug & MCC_DBG_RELOC)
			{ MCC_TRACE("br\n"); printf("PCREL_LO12_I: val=%lx addr=%lx\n", (long)val, (long)addr); }
		addr = val;
		riscv64_lookup_pcrel_hi(s1, addr, &val);
		write32le(ptr, (read32le(ptr) & 0xfffff) | (((val - addr) & 0xfff) << 20));
		return;
	case R_RISCV_PCREL_LO12_S:
		addr = val;
		riscv64_lookup_pcrel_hi(s1, addr, &val);
		off32 = val - addr;
		write32le(ptr, (read32le(ptr) & ~0xfe000f80) | ((off32 & 0xfe0) << 20) | ((off32 & 0x01f) << 7));
		return;

	case R_RISCV_RVC_BRANCH:
		off64 = (val - addr);
		if ((off64 + (1 << 8)) & ~(uint64_t)0x1fe)
			{ MCC_TRACE("br\n"); mcc_error_noabort("R_RISCV_RVC_BRANCH relocation failed"
												" (val=%lx, addr=%lx)",
												(long)val, (long)addr); }
		off32 = off64;
		write16le(
				ptr, (read16le(ptr) & 0xe383) | (((off32 >> 5) & 1) << 2) | (((off32 >> 1) & 3) << 3) | (((off32 >> 6) & 3) << 5) | (((off32 >> 3) & 3) << 10) | (((off32 >> 8) & 1) << 12));
		return;
	case R_RISCV_RVC_JUMP:
		off64 = (val - addr);
		if ((off64 + (1 << 11)) & ~(uint64_t)0xffe)
			{ MCC_TRACE("br\n"); mcc_error_noabort("R_RISCV_RVC_BRANCH relocation failed"
												" (val=%lx, addr=%lx)",
												(long)val, (long)addr); }
		off32 = off64;
		write16le(
				ptr, (read16le(ptr) & 0xe003) | (((off32 >> 5) & 1) << 2) | (((off32 >> 1) & 7) << 3) | (((off32 >> 7) & 1) << 6) | (((off32 >> 6) & 1) << 7) | (((off32 >> 10) & 1) << 8) | (((off32 >> 8) & 3) << 9) | (((off32 >> 4) & 1) << 11) | (((off32 >> 11) & 1) << 12));
		return;

	case R_RISCV_32:
		if (s1->output_type & MCC_OUTPUT_DYN) { MCC_TRACE("br\n");
			qrel->r_offset = rel->r_offset;
			qrel->r_info = ELFW(R_INFO)(0, R_RISCV_RELATIVE);
			qrel->r_addend = (int)read32le(ptr) + val;
			qrel++;
		}
		add32le(ptr, val);
		return;
	case R_RISCV_64:
		if (s1->output_type & MCC_OUTPUT_DYN) { MCC_TRACE("br\n");
			esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
			qrel->r_offset = rel->r_offset;
			if (esym_index) { MCC_TRACE("br\n");
				qrel->r_info = ELFW(R_INFO)(esym_index, R_RISCV_64);
				qrel->r_addend = rel->r_addend;
				qrel++;
				break;
			} else { MCC_TRACE("br\n");
				qrel->r_info = ELFW(R_INFO)(0, R_RISCV_RELATIVE);
				qrel->r_addend = read64le(ptr) + val;
				qrel++;
			}
		}
	case R_RISCV_JUMP_SLOT:
		add64le(ptr, val);
		return;
	case R_RISCV_ADD64:
		write64le(ptr, read64le(ptr) + val);
		return;
	case R_RISCV_ADD32:
		write32le(ptr, read32le(ptr) + val);
		return;
	case R_RISCV_SUB64:
		write64le(ptr, read64le(ptr) - val);
		return;
	case R_RISCV_SUB32:
		write32le(ptr, read32le(ptr) - val);
		return;
	case R_RISCV_ADD16:
		write16le(ptr, read16le(ptr) + val);
		return;
	case R_RISCV_SUB8:
		*ptr -= val;
		return;
	case R_RISCV_SUB16:
		write16le(ptr, read16le(ptr) - val);
		return;
	case R_RISCV_SET6:
		*ptr = (*ptr & ~0x3f) | (val & 0x3f);
		return;
	case R_RISCV_SET8:
		*ptr = (*ptr & ~0xff) | (val & 0xff);
		return;
	case R_RISCV_SET16:
		write16le(ptr, val);
		return;
	case R_RISCV_SUB6:
		*ptr = (*ptr & ~0x3f) | ((*ptr - val) & 0x3f);
		return;
	case R_RISCV_32_PCREL:
		if (s1->output_type & MCC_OUTPUT_DYN) { MCC_TRACE("br\n");
			esym_index = get_sym_attr(s1, sym_index, 0)->dyn_index;
			if (esym_index) { MCC_TRACE("br\n");
				qrel->r_offset = rel->r_offset;
				qrel->r_info = ELFW(R_INFO)(esym_index, R_RISCV_32_PCREL);
				qrel->r_addend = (int)read32le(ptr) + rel->r_addend;
				qrel++;
				break;
			}
		}
		add32le(ptr, val - addr);
		return;
	case R_RISCV_SET_ULEB128:
	case R_RISCV_SUB_ULEB128:
		return;
	case R_RISCV_COPY:
		return;
	case R_RISCV_RELATIVE:
		return;

	case R_RISCV_TPREL_HI20:
	case R_RISCV_TPREL_LO12_I: {
		addr_t tls_start = 0;
		int64_t tp_offset;
		for (int i = 1; i < s1->nb_sections; i++) { MCC_TRACE("br\n");
			Section *s = s1->sections[i];
			if (s->sh_flags & SHF_TLS && s->sh_size) { MCC_TRACE("br\n");
				if (!tls_start || s->sh_addr < tls_start)
					{ MCC_TRACE("br\n"); tls_start = s->sh_addr; }
			}
		}
		tp_offset = val - tls_start;
		if (type == R_RISCV_TPREL_HI20) { MCC_TRACE("br\n");
			off64 = (int64_t)(tp_offset + 0x800) >> 12;
			if ((off64 + ((uint64_t)1 << 20)) >> 21)
				{ MCC_TRACE("br\n"); mcc_error_noabort("R_RISCV_TPREL_HI20 relocation failed"); }
			write32le(ptr, (read32le(ptr) & 0xfff) | ((off64 & 0xfffff) << 12));
		} else { MCC_TRACE("br\n");
			write32le(ptr, (read32le(ptr) & 0xfffff) | (((tp_offset) & 0xfff) << 20));
		}
		return;
	}

	default:
		mcc_error_noabort("unhandled relocation type 0x%x at 0x%x (value 0x%x)",
											(unsigned)type, (unsigned)addr, (unsigned)val);
		return;
	}
}
