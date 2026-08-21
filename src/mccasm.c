#define USING_GLOBALS
#include "mcc.h"
#include "mccrir.h"

#define last_text_section (mcc_state->last_text_section)
#define asmgoto_n (mcc_state->asmgoto_n)

static int mcc_assemble_internal(MCCState *s1, int do_preprocess, int global);
static Sym *asm_new_label(MCCState *s1, int label, int is_local);
static Sym *asm_new_label1(MCCState *s1, int label, int is_local, int sh_num, int value);

#define ASM_MACRO_MAXPARAM 30
typedef struct AsmMacro {
	int name;
	int nparams;
	int params[ASM_MACRO_MAXPARAM];
	TokenString *body;
	struct AsmMacro *next;
} AsmMacro;
static AsmMacro *asm_macros;
static int asm_standalone;

static AsmMacro *asm_macro_find(int name) { MCC_TRACE("enter\n");
	AsmMacro *m;
	for (m = asm_macros; m; m = m->next)
		if (m->name == name)
			{ MCC_TRACE("br\n"); return m; }
	return NULL;
}

static void asm_macros_free(void) { MCC_TRACE("enter\n");
	while (asm_macros) { MCC_TRACE("br\n");
		AsmMacro *m = asm_macros;
		asm_macros = m->next;
		tok_str_free(m->body);
		mcc_free(m);
	}
}

static TokenString *asm_macro_subst(const int *params, int nparams,
																		TokenString **args, int nargs,
																		const int *body, int errname) { MCC_TRACE("enter\n");
	TokenString *subst = tok_str_alloc();
	const int *p = body;
	while (*p && *p != TOK_EOF) { MCC_TRACE("br\n");
		int t;
		CValue cv;
		TOK_GET(&t, &p, &cv);
		if (t != '\\') { MCC_TRACE("br\n");
			tok_str_add2(subst, t, &cv);
			continue;
		}
		if (!*p)
			{ MCC_TRACE("br\n"); break; }
		{
			int t2;
			CValue cv2;
			TOK_GET(&t2, &p, &cv2);
			if (t2 == '(') { MCC_TRACE("br\n");
				if (*p) { MCC_TRACE("br\n");
					int t3;
					CValue cv3;
					const int *q = p;
					TOK_GET(&t3, &q, &cv3);
					if (t3 == ')')
						{ MCC_TRACE("br\n"); p = q; }
				}
			} else if (t2 >= TOK_IDENT) { MCC_TRACE("br\n");
				int found = -1, k;
				for (k = 0; k < nparams; k++)
					if (params[k] == t2)
						{ MCC_TRACE("br\n"); found = k; break; }
				if (found < 0) { MCC_TRACE("br\n");
					mcc_error("undefined macro parameter '%s' in %s",
										get_tok_str(t2, NULL), get_tok_str(errname, NULL));
				} else if (found < nargs && args[found]) { MCC_TRACE("br\n");
					const int *ap = args[found]->str;
					while (*ap && *ap != TOK_EOF) { MCC_TRACE("br\n");
						int at;
						CValue acv;
						TOK_GET(&at, &ap, &acv);
						tok_str_add2(subst, at, &acv);
					}
				}
			} else { MCC_TRACE("br\n");
				tok_str_add2(subst, t2, &cv2);
			}
		}
	}
	tok_str_add(subst, TOK_EOF);
	return subst;
}

static void asm_macro_expand(MCCState *s1, AsmMacro *m, int global) { MCC_TRACE("enter\n");
	TokenString *args[ASM_MACRO_MAXPARAM];
	TokenString *subst;
	int nargs = 0, i, saved = parse_flags;

	for (i = 0; i < ASM_MACRO_MAXPARAM; i++)
		args[i] = NULL;
	parse_flags |= PARSE_FLAG_ACCEPT_STRAYS;
	if (tok != TOK_LINEFEED && tok != TOK_EOF && tok != ';') { MCC_TRACE("br\n");
		args[0] = tok_str_alloc();
		for (;;) { MCC_TRACE("br\n");
			if (tok == TOK_LINEFEED || tok == TOK_EOF || tok == ';')
				{ MCC_TRACE("br\n"); break; }
			if (tok == ',') { MCC_TRACE("br\n");
				next();
				if (nargs < ASM_MACRO_MAXPARAM - 1)
					{ MCC_TRACE("br\n"); nargs++; args[nargs] = tok_str_alloc(); }
				continue;
			}
			tok_str_add_tok(args[nargs]);
			next();
		}
		nargs++;
	}
	for (i = 0; i < nargs; i++)
		{ MCC_TRACE("br\n"); tok_str_add(args[i], TOK_EOF); }

	subst = asm_macro_subst(m->params, m->nparams, args, nargs, m->body->str,
													m->name);
	parse_flags = saved;

	begin_macro(subst, 1);
	mcc_assemble_internal(s1, (parse_flags & PARSE_FLAG_PREPROCESS), global);
	end_macro();
	tok = TOK_LINEFEED;

	for (i = 0; i < nargs; i++)
		if (args[i])
			{ MCC_TRACE("br\n"); tok_str_free(args[i]); }
}

#if MCC_EH_FRAME

ST_FUNC int mcc_cfi_uleb(unsigned char *p, unsigned long long value);
ST_FUNC int mcc_cfi_advance(unsigned char *p, unsigned long delta);
ST_FUNC void mcc_eh_frame_fde(MCCState *s1, Section *code_sec,
															unsigned long func_start,
															unsigned long func_size,
															const unsigned char *ops, int nops);
#endif

#if MCC_PTR_SIZE == 8
ST_FUNC void gen_addr64(int r, Sym *sym, int64_t c) { MCC_TRACE("enter\n");
	if (r & VT_SYM)
		{ MCC_TRACE("br\n"); greloca(cur_text_section, sym, ind, R_DATA_PTR, c), c = 0; }
	gen_le32(c);
	gen_le32(c >> 32);
}

ST_FUNC void gen_expr64(ExprValue *pe) { MCC_TRACE("enter\n");
	gen_addr64(pe->sym ? VT_SYM : 0, pe->sym, pe->v);
}
#endif

static int asm_get_prefix_name(MCCState *s1, const char *prefix, unsigned int n) { MCC_TRACE("enter\n");
	char buf[64];
	snprintf(buf, sizeof(buf), "%s%u", prefix, n);
	return tok_alloc_const(buf);
}

ST_FUNC int asm_get_local_label_name(MCCState *s1, unsigned int n) { MCC_TRACE("enter\n");
	return asm_get_prefix_name(s1, "L..", n);
}

static int asm2cname(int v, int *addeddot) { MCC_TRACE("enter\n");
	const char *name;
	*addeddot = 0;
	if (!mcc_state->leading_underscore)
		{ MCC_TRACE("br\n"); return v; }
	name = get_tok_str(v, NULL);
	if (!name)
		{ MCC_TRACE("br\n"); return v; }
	if (name[0] == '_') { MCC_TRACE("br\n");
		v = tok_alloc_const(name + 1);
	} else if (!strchr(name, '.')) { MCC_TRACE("br\n");
		char newname[256];
		snprintf(newname, sizeof newname, ".%s", name);
		v = tok_alloc_const(newname);
		*addeddot = 1;
	}
	return v;
}

static Sym *asm_label_find(int v) { MCC_TRACE("enter\n");
	Sym *sym;
	int addeddot;
	v = asm2cname(v, &addeddot);
	sym = sym_find(v);
	while (sym && sym->sym_scope && !(sym->type.t & VT_STATIC))
		{ MCC_TRACE("br\n"); sym = sym->prev_tok; }
	return sym;
}

static Sym *asm_label_push(int v) { MCC_TRACE("enter\n");
	int addeddot, v2 = asm2cname(v, &addeddot);
	Sym *sym = global_identifier_push(v2, VT_ASM | VT_EXTERN | VT_STATIC, 0);
	if (addeddot)
		{ MCC_TRACE("br\n"); sym->asm_label = v; }
	return sym;
}

ST_FUNC Sym *get_asm_sym(int name, Sym *csym) { MCC_TRACE("enter\n");
	Sym *sym = asm_label_find(name);
	if (!sym) { MCC_TRACE("br\n");
		sym = asm_label_push(name);
		if (csym)
			{ MCC_TRACE("br\n"); sym->c = csym->c; }
	}
	return sym;
}

static Sym *asm_section_sym(MCCState *s1, Section *sec) { MCC_TRACE("enter\n");
	char buf[100];
	int label;
	Sym *sym;
	snprintf(buf, sizeof buf, "L.%s", sec->name);
	label = tok_alloc_const(buf);
	sym = asm_label_find(label);
	return sym ? sym : asm_new_label1(s1, label, 1, sec->sh_num, 0);
}

static void asm_expr_unary_nested(MCCState *s1, ExprValue *pe);

static void asm_expr_unary(MCCState *s1, ExprValue *pe) { MCC_TRACE("enter\n");
	mcc_parse_depth_enter();
	asm_expr_unary_nested(s1, pe);
	mcc_parse_depth_leave();
}

static void asm_expr_unary_nested(MCCState *s1, ExprValue *pe) { MCC_TRACE("enter\n");
	Sym *sym;
	int op, label;
	uint64_t n;
	const char *p;

	switch (tok) { MCC_TRACE("br\n");
	case TOK_PPNUM:
		p = tokc.str.data;
		if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B') && (p[2] == '0' || p[2] == '1')) { MCC_TRACE("br\n");
			n = strtoull(p + 2, (char **)&p, 2);
		} else { MCC_TRACE("br\n");
			n = strtoull(p, (char **)&p, 0);
		}
		if (*p == 'b' || *p == 'f') { MCC_TRACE("br\n");
			label = asm_get_local_label_name(s1, n);
			sym = asm_label_find(label);
			if (*p == 'b') { MCC_TRACE("br\n");
				if (sym && (!sym->c || elfsym(sym)->st_shndx == SHN_UNDEF))
					{ MCC_TRACE("br\n"); sym = sym->prev_tok; }
				if (!sym)
					{ MCC_TRACE("br\n"); mcc_error("local label '%d' not found backward", (int)n); }
			} else { MCC_TRACE("br\n");
				if (!sym || (sym->c && elfsym(sym)->st_shndx != SHN_UNDEF)) { MCC_TRACE("br\n");
					sym = asm_label_push(label);
				}
			}
			pe->v = 0;
			pe->sym = sym;
			pe->pcrel = 0;
		} else if (*p == '\0') { MCC_TRACE("br\n");
			pe->v = n;
			pe->sym = NULL;
			pe->pcrel = 0;
		} else { MCC_TRACE("br\n");
			mcc_error("invalid number syntax");
		}
		next();
		break;
	case '+':
		next();
		asm_expr_unary(s1, pe);
		break;
	case '-':
	case '~':
		op = tok;
		next();
		asm_expr_unary(s1, pe);
		if (pe->sym)
			{ MCC_TRACE("br\n"); mcc_error("invalid operation with label"); }
		if (op == '-')
			{ MCC_TRACE("br\n"); pe->v = -pe->v; }
		else
			{ MCC_TRACE("br\n"); pe->v = ~pe->v; }
		break;
	case TOK_CCHAR:
	case TOK_LCHAR:
		pe->v = tokc.i;
		pe->sym = NULL;
		pe->pcrel = 0;
		next();
		break;
	case '(':
		next();
		asm_expr(s1, pe);
		skip(')');
		break;
	case '.':
		pe->v = ind;
		pe->sym = asm_section_sym(s1, cur_text_section);
		pe->pcrel = 0;
		next();
		break;
	default:
		if (tok >= TOK_IDENT) { MCC_TRACE("br\n");
			ElfSym *esym;
			sym = get_asm_sym(tok, NULL);
			esym = elfsym(sym);
			if (esym && esym->st_shndx == SHN_ABS) { MCC_TRACE("br\n");
				pe->v = esym->st_value;
				pe->sym = NULL;
				pe->pcrel = 0;
			} else { MCC_TRACE("br\n");
				pe->v = 0;
				pe->sym = sym;
				pe->pcrel = 0;
			}
			next();
		} else { MCC_TRACE("br\n");
			mcc_error("bad expression syntax [%s]", get_tok_str(tok, &tokc));
		}
		break;
	}
}

static void asm_expr_prod(MCCState *s1, ExprValue *pe) { MCC_TRACE("enter\n");
	int op;
	ExprValue e2;

	asm_expr_unary(s1, pe);
	for (;;) { MCC_TRACE("br\n");
		op = tok;
		if (op != '*' && op != '/' && op != '%' &&
				op != TOK_SHL && op != TOK_SAR)
			{ MCC_TRACE("br\n"); break; }
		next();
		asm_expr_unary(s1, &e2);
		if (pe->sym || e2.sym)
			{ MCC_TRACE("br\n"); mcc_error("invalid operation with label"); }
		switch (op) { MCC_TRACE("br\n");
		case '*':
			pe->v *= e2.v;
			break;
		case '/':
			if (e2.v == 0) { MCC_TRACE("br\n");
			div_error:
				mcc_error("division by zero");
			}
			pe->v /= e2.v;
			break;
		case '%':
			if (e2.v == 0)
				{ MCC_TRACE("br\n"); goto div_error; }
			pe->v %= e2.v;
			break;
		case TOK_SHL:
			pe->v <<= e2.v;
			break;
		default:
		case TOK_SAR:
			pe->v >>= e2.v;
			break;
		}
	}
}

static void asm_expr_logic(MCCState *s1, ExprValue *pe) { MCC_TRACE("enter\n");
	int op;
	ExprValue e2;

	asm_expr_prod(s1, pe);
	for (;;) { MCC_TRACE("br\n");
		op = tok;
		if (op != '&' && op != '|' && op != '^')
			{ MCC_TRACE("br\n"); break; }
		next();
		asm_expr_prod(s1, &e2);
		if (pe->sym || e2.sym)
			{ MCC_TRACE("br\n"); mcc_error("invalid operation with label"); }
		switch (op) { MCC_TRACE("br\n");
		case '&':
			pe->v &= e2.v;
			break;
		case '|':
			pe->v |= e2.v;
			break;
		default:
		case '^':
			pe->v ^= e2.v;
			break;
		}
	}
}

static inline void asm_expr_sum(MCCState *s1, ExprValue *pe) { MCC_TRACE("enter\n");
	int op;
	ExprValue e2;

	asm_expr_logic(s1, pe);
	for (;;) { MCC_TRACE("br\n");
		op = tok;
		if (op != '+' && op != '-')
			{ MCC_TRACE("br\n"); break; }
		next();
		asm_expr_logic(s1, &e2);
		if (op == '+') { MCC_TRACE("br\n");
			if (pe->sym != NULL && e2.sym != NULL)
				{ MCC_TRACE("br\n"); goto cannot_relocate; }
			pe->v += e2.v;
			if (pe->sym == NULL && e2.sym != NULL)
				{ MCC_TRACE("br\n"); pe->sym = e2.sym; }
		} else { MCC_TRACE("br\n");
			pe->v -= e2.v;
			if (!e2.sym) { MCC_TRACE("br\n");
			} else if (pe->sym == e2.sym) { MCC_TRACE("br\n");
				pe->sym = NULL;
			} else { MCC_TRACE("br\n");
				ElfSym *esym1, *esym2;
				esym1 = elfsym(pe->sym);
				esym2 = elfsym(e2.sym);
				if (!esym2)
					{ MCC_TRACE("br\n"); goto cannot_relocate; }
				if (esym1 && esym1->st_shndx == esym2->st_shndx && esym1->st_shndx != SHN_UNDEF) { MCC_TRACE("br\n");
					pe->v += (int)(esym1->st_value - esym2->st_value);
					pe->sym = NULL;
				} else if (esym2->st_shndx == cur_text_section->sh_num) { MCC_TRACE("br\n");
					pe->v += (int)(0 - esym2->st_value);
					pe->pcrel = 1;
					e2.sym = NULL;
				} else { MCC_TRACE("br\n");
				cannot_relocate:
					mcc_error("invalid operation with label");
				}
			}
		}
	}
}

static inline void asm_expr_cmp(MCCState *s1, ExprValue *pe) { MCC_TRACE("enter\n");
	int op;
	ExprValue e2;

	asm_expr_sum(s1, pe);
	for (;;) { MCC_TRACE("br\n");
		op = tok;
		if (op != TOK_EQ && op != TOK_NE && (op > TOK_GT || op < TOK_ULE))
			{ MCC_TRACE("br\n"); break; }
		next();
		asm_expr_sum(s1, &e2);
		if (pe->sym || e2.sym)
			{ MCC_TRACE("br\n"); mcc_error("invalid operation with label"); }
		switch (op) { MCC_TRACE("br\n");
		case TOK_EQ:
			pe->v = pe->v == e2.v;
			break;
		case TOK_NE:
			pe->v = pe->v != e2.v;
			break;
		case TOK_LT:
			pe->v = (int64_t)pe->v < (int64_t)e2.v;
			break;
		case TOK_GE:
			pe->v = (int64_t)pe->v >= (int64_t)e2.v;
			break;
		case TOK_LE:
			pe->v = (int64_t)pe->v <= (int64_t)e2.v;
			break;
		case TOK_GT:
			pe->v = (int64_t)pe->v > (int64_t)e2.v;
			break;
		default:
			break;
		}
		pe->v = -(int64_t)pe->v;
	}
}

ST_FUNC void asm_expr(MCCState *s1, ExprValue *pe) { MCC_TRACE("enter\n");
	asm_expr_cmp(s1, pe);
}

ST_FUNC int asm_int_expr(MCCState *s1) { MCC_TRACE("enter\n");
	ExprValue e;
	asm_expr(s1, &e);
	if (e.sym)
		{ MCC_TRACE("br\n"); expect("constant"); }
	if ((int)e.v != e.v)
		{ MCC_TRACE("br\n"); mcc_error("integer out of range %lld", (long long)e.v); }
	return e.v;
}

static Sym *asm_new_label1(MCCState *s1, int label, int is_local,
													 int sh_num, int value) { MCC_TRACE("enter\n");
	Sym *sym;
	ElfSym *esym;

	sym = asm_label_find(label);
	if (sym) { MCC_TRACE("br\n");
		esym = elfsym(sym);
		if (esym && esym->st_shndx != SHN_UNDEF) { MCC_TRACE("br\n");
			if (IS_ASM_SYM(sym) && (is_local == 1 || (sym->type.t & VT_EXTERN)))
				{ MCC_TRACE("br\n"); goto new_label; }
			if (!(sym->type.t & VT_EXTERN))
				{ MCC_TRACE("br\n"); mcc_error("assembler label '%s' already defined",
									get_tok_str(label, NULL)); }
		}
	} else { MCC_TRACE("br\n");
	new_label:
		sym = asm_label_push(label);
	}
	if (!sym->c)
		{ MCC_TRACE("br\n"); put_extern_sym2(sym, SHN_UNDEF, 0, 0, 1); }
	esym = elfsym(sym);
	esym->st_shndx = sh_num;
	esym->st_value = value;
	if (is_local != 2)
		{ MCC_TRACE("br\n"); sym->type.t &= ~VT_EXTERN; }
	return sym;
}

static Sym *asm_new_label(MCCState *s1, int label, int is_local) { MCC_TRACE("enter\n");
	return asm_new_label1(s1, label, is_local, cur_text_section->sh_num, ind);
}

static Sym *set_symbol(MCCState *s1, int label) { MCC_TRACE("enter\n");
	long n;
	ExprValue e;
	Sym *sym;
	ElfSym *esym;
	next();
	asm_expr(s1, &e);
	n = e.v;
	esym = elfsym(e.sym);
	if (esym)
		{ MCC_TRACE("br\n"); n += esym->st_value; }
	sym = asm_new_label1(s1, label, 2, esym ? esym->st_shndx : SHN_ABS, n);
	elfsym(sym)->st_other |= ST_ASM_SET;
	return sym;
}

static void use_section1(MCCState *s1, Section *sec) { MCC_TRACE("enter\n");
	cur_text_section->data_offset = ind;
	cur_text_section = sec;
	ind = cur_text_section->data_offset;
	if (asm_standalone && sec->asm_base < 0)
		{ MCC_TRACE("br\n"); sec->asm_base = ind; }
}

static void use_section(MCCState *s1, const char *name) { MCC_TRACE("enter\n");
	Section *sec;
	sec = find_section(s1, name);
	use_section1(s1, sec);
}

static void push_section(MCCState *s1, const char *name) { MCC_TRACE("enter\n");
	Section *sec = find_section(s1, name);
	sec->prev = cur_text_section;
	use_section1(s1, sec);
}

static void pop_section(MCCState *s1) { MCC_TRACE("enter\n");
	Section *prev = cur_text_section->prev;
	if (!prev)
		{ MCC_TRACE("br\n"); mcc_error(".popsection without .pushsection"); }
	cur_text_section->prev = NULL;
	use_section1(s1, prev);
}

static void asm_skip_to_eol(void) { MCC_TRACE("enter\n");
	while (tok != TOK_LINEFEED && tok != ';' && tok != TOK_EOF && tok != CH_EOF)
		{ MCC_TRACE("br\n"); next(); }
}

static void asm_skip_conditional(MCCState *s1, int stop_at_else) { MCC_TRACE("enter\n");
	int nest = 0;
	for (;;) { MCC_TRACE("br\n");
		next();
		if (tok == TOK_EOF || tok == CH_EOF)
			{ MCC_TRACE("br\n"); mcc_error(".endif expected but end of file reached"); }
		if (tok >= TOK_ASMDIR_if && tok <= TOK_ASMDIR_ifnb) { MCC_TRACE("br\n");
			nest++;
		} else if (tok == TOK_ASMDIR_endif) { MCC_TRACE("br\n");
			if (nest == 0)
				{ MCC_TRACE("br\n"); break; }
			nest--;
		} else if (tok == TOK_ASMDIR_else && nest == 0 && stop_at_else) { MCC_TRACE("br\n");
			break;
		}
	}
	/* leave tok at the terminating directive's end-of-line (main loop needs it) */
	asm_skip_to_eol();
}

/* skip (nesting-aware) to the next .elseif/.else/.endif at this level; leave tok
   AT that directive (no eol consume) and return which one it is. */
static int asm_skip_to_cond_branch(MCCState *s1) { MCC_TRACE("enter\n");
	int nest = 0;
	for (;;) { MCC_TRACE("br\n");
		next();
		if (tok == TOK_EOF || tok == CH_EOF)
			{ MCC_TRACE("br\n"); mcc_error(".endif expected but end of file reached"); }
		if (tok >= TOK_ASMDIR_if && tok <= TOK_ASMDIR_ifnb) { MCC_TRACE("br\n");
			nest++;
		} else if (tok == TOK_ASMDIR_endif) { MCC_TRACE("br\n");
			if (nest == 0)
				{ MCC_TRACE("br\n"); return TOK_ASMDIR_endif; }
			nest--;
		} else if (nest == 0 && (tok == TOK_ASMDIR_else ||
														 tok == TOK_ASMDIR_elseif)) { MCC_TRACE("br\n");
			return tok;
		}
	}
}

/* the false-condition path shared by .if/.ifdef/.ifeq/...: walk .elseif branches
   until one is taken (assemble it) or .else/.endif is reached. */
static void asm_conditional_false(MCCState *s1) { MCC_TRACE("enter\n");
	for (;;) { MCC_TRACE("br\n");
		int b = asm_skip_to_cond_branch(s1);
		if (b == TOK_ASMDIR_elseif) { MCC_TRACE("br\n");
			next();
			if (asm_int_expr(s1) != 0)
				{ MCC_TRACE("br\n"); asm_skip_to_eol(); return; }
		} else { MCC_TRACE("br\n");
			asm_skip_to_eol();
			return;
		}
	}
}

static void asm_parse_directive(MCCState *s1, int global) { MCC_TRACE("enter\n");
	int n, offset, v, size, tok1, c, is_align, maxskip;
	Section *sec;
	uint8_t *ptr;

	sec = cur_text_section;
	switch (tok) { MCC_TRACE("br\n");
	case TOK_ASMDIR_align:
	case TOK_ASMDIR_balign:
	case TOK_ASMDIR_p2align:
	case TOK_ASMDIR_skip:
	case TOK_ASMDIR_space:
	case TOK_ASMDIR_zero:
		tok1 = tok;
		is_align = (tok1 == TOK_ASMDIR_align || tok1 == TOK_ASMDIR_balign ||
								tok1 == TOK_ASMDIR_p2align);
		maxskip = -1;
		next();
		n = asm_int_expr(s1);
#ifdef MCC_TARGET_ARM64
		if (tok1 == TOK_ASMDIR_align)
			{ MCC_TRACE("br\n"); tok1 = TOK_ASMDIR_p2align; }
#endif
		if (tok1 == TOK_ASMDIR_p2align) { MCC_TRACE("br\n");
			if (n < 0 || n > 30)
				{ MCC_TRACE("br\n"); mcc_error("invalid p2align, must be between 0 and 30"); }
			n = 1 << n;
			tok1 = TOK_ASMDIR_align;
		}
		if (tok1 == TOK_ASMDIR_align || tok1 == TOK_ASMDIR_balign) { MCC_TRACE("br\n");
			int ab;
			if (n <= 0 || (n & (n - 1)) != 0)
				{ MCC_TRACE("br\n"); mcc_error("alignment must be a positive power of two"); }
			ab = (asm_standalone && sec->asm_base >= 0) ? sec->asm_base : 0;
			offset = ab + ((((ind - ab) + n - 1) & -n));
			size = offset - ind;
			if (sec->sh_addralign < n)
				{ MCC_TRACE("br\n"); sec->sh_addralign = n; }
			c = sec->sh_flags & SHF_EXECINSTR;
		} else { MCC_TRACE("br\n");
			if (n < 0)
				{ MCC_TRACE("br\n"); n = 0; }
			size = n, c = 0;
		}
		v = 0;
		if (tok == ',') { MCC_TRACE("br\n");
			next();
			if (is_align && tok == ',') { MCC_TRACE("br\n");
				/* .p2align exp,,max_skip -- fill argument omitted */
			} else { MCC_TRACE("br\n");
				v = asm_int_expr(s1), c = 0;
			}
			if (is_align && tok == ',') { MCC_TRACE("br\n");
				next();
				maxskip = asm_int_expr(s1);
			}
		}
		if (is_align && maxskip >= 0 && size > maxskip)
			{ MCC_TRACE("br\n"); size = 0; }
	zero_pad:
		if ((uint64_t)ind + size >= 1 << 30)
			{ MCC_TRACE("br\n"); mcc_error("too much data"); }
		if (sec->sh_type != SHT_NOBITS) { MCC_TRACE("br\n");
			if (c) { MCC_TRACE("br\n");
				gen_fill_nops(size);
				break;
			}
			sec->data_offset = ind;
			ptr = section_ptr_add(sec, size);
			memset(ptr, v, size);
		}
		ind += size;
		break;
	case TOK_ASMDIR_quad:
	case TOK_ASMDIR_xword:
	case TOK_ASMDIR_8byte:
#if MCC_PTR_SIZE == 8
		size = 8;
		goto asm_data;
#else
		next();
		for (;;) { MCC_TRACE("br\n");
			uint64_t vl;
			const char *p;

			p = tokc.str.data;
			if (tok != TOK_PPNUM) { MCC_TRACE("br\n");
			error_constant:
				mcc_error("64 bit constant");
			}
			vl = strtoll(p, (char **)&p, 0);
			if (*p != '\0')
				{ MCC_TRACE("br\n"); goto error_constant; }
			next();
			if (sec->sh_type != SHT_NOBITS) { MCC_TRACE("br\n");
				gen_le32(vl);
				gen_le32(vl >> 32);
			} else { MCC_TRACE("br\n");
				ind += 8;
			}
			if (tok != ',')
				{ MCC_TRACE("br\n"); break; }
			next();
		}
		break;
#endif
	case TOK_ASMDIR_float:
	case TOK_ASMDIR_double: { MCC_TRACE("br\n");
		int is_double = (tok == TOK_ASMDIR_double);
		next();
		for (;;) { MCC_TRACE("br\n");
			double dv;
			int neg = 0;
			const char *p;
			if (tok == '-' || tok == '+') { MCC_TRACE("br\n");
				neg = (tok == '-');
				next();
			}
			if (tok != TOK_PPNUM)
				{ MCC_TRACE("br\n"); expect("floating constant"); }
			p = tokc.str.data;
			dv = strtod(p, (char **)&p);
			if (*p != '\0')
				{ MCC_TRACE("br\n"); mcc_error("floating constant"); }
			if (neg)
				{ MCC_TRACE("br\n"); dv = -dv; }
			next();
			if (sec->sh_type != SHT_NOBITS) { MCC_TRACE("br\n");
				if (is_double) { MCC_TRACE("br\n");
					uint64_t bits;
					memcpy(&bits, &dv, 8);
					gen_le32((uint32_t)bits);
					gen_le32((uint32_t)(bits >> 32));
				} else { MCC_TRACE("br\n");
					float fv = (float)dv;
					uint32_t bits;
					memcpy(&bits, &fv, 4);
					gen_le32(bits);
				}
			} else { MCC_TRACE("br\n");
				ind += is_double ? 8 : 4;
			}
			if (tok != ',')
				{ MCC_TRACE("br\n"); break; }
			next();
		}
		break;
	}
	case TOK_ASMDIR_octa: { MCC_TRACE("br\n");
		next();
		for (;;) { MCC_TRACE("br\n");
			uint8_t buf[16];
			const char *p;
			int i, neg = 0, base = 10;
			memset(buf, 0, sizeof buf);
			if (tok == '-' || tok == '+') { MCC_TRACE("br\n");
				neg = (tok == '-');
				next();
			}
			if (tok != TOK_PPNUM)
				{ MCC_TRACE("br\n"); expect("128 bit constant"); }
			p = tokc.str.data;
			if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
				{ MCC_TRACE("br\n"); base = 16; p += 2; }
			else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B'))
				{ MCC_TRACE("br\n"); base = 2; p += 2; }
			else if (p[0] == '0' && p[1] != '\0')
				{ MCC_TRACE("br\n"); base = 8; p += 1; }
			for (; *p; p++) { MCC_TRACE("br\n");
				int d, carry;
				if (*p >= '0' && *p <= '9')
					d = *p - '0';
				else if (*p >= 'a' && *p <= 'f')
					d = *p - 'a' + 10;
				else if (*p >= 'A' && *p <= 'F')
					d = *p - 'A' + 10;
				else
					{ MCC_TRACE("br\n"); mcc_error("128 bit constant"); d = 0; }
				if (d >= base)
					{ MCC_TRACE("br\n"); mcc_error("128 bit constant"); }
				carry = d;
				for (i = 0; i < 16; i++) { MCC_TRACE("br\n");
					int v = buf[i] * base + carry;
					buf[i] = (uint8_t)v;
					carry = v >> 8;
				}
			}
			if (neg) { MCC_TRACE("br\n");
				int carry = 1;
				for (i = 0; i < 16; i++) { MCC_TRACE("br\n");
					int v = (uint8_t)~buf[i] + carry;
					buf[i] = (uint8_t)v;
					carry = v >> 8;
				}
			}
			next();
			if (sec->sh_type != SHT_NOBITS) { MCC_TRACE("br\n");
				for (i = 0; i < 16; i++)
					{ MCC_TRACE("br\n"); g(buf[i]); }
			} else { MCC_TRACE("br\n");
				ind += 16;
			}
			if (tok != ',')
				{ MCC_TRACE("br\n"); break; }
			next();
		}
		break;
	}
	case TOK_ASMDIR_arch:
	case TOK_ASMDIR_cpu:
		asm_skip_to_eol();
		break;
	case TOK_ASMDIR_macro: { MCC_TRACE("br\n");
		AsmMacro *m;
		int saved_flags = parse_flags;
		next();
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); expect("macro name"); }
		m = mcc_malloc(sizeof *m);
		memset(m, 0, sizeof *m);
		m->name = tok;
		next();
		while (tok != TOK_LINEFEED && tok != TOK_EOF && tok != ';') { MCC_TRACE("br\n");
			if (tok == ',')
				{ MCC_TRACE("br\n"); next(); continue; }
			if (tok < TOK_IDENT)
				{ MCC_TRACE("br\n"); break; }
			if (m->nparams < ASM_MACRO_MAXPARAM)
				{ MCC_TRACE("br\n"); m->params[m->nparams++] = tok; }
			next();
			if (tok == '=') { MCC_TRACE("br\n");
				next();
				while (tok != ',' && tok != TOK_LINEFEED && tok != TOK_EOF && tok != ';')
					{ MCC_TRACE("br\n"); next(); }
			}
		}
		parse_flags |= PARSE_FLAG_ACCEPT_STRAYS;
		m->body = tok_str_alloc();
		while (next(), tok != TOK_ASMDIR_endm) { MCC_TRACE("br\n");
			if (tok == CH_EOF || tok == TOK_EOF) { MCC_TRACE("br\n");
				parse_flags = saved_flags;
				mcc_error("unexpected end of file in .macro (missing .endm)");
			}
			tok_str_add_tok(m->body);
		}
		tok_str_add(m->body, TOK_EOF);
		parse_flags = saved_flags;
		m->next = asm_macros;
		asm_macros = m;
		next();
		break;
	}
	case TOK_ASMDIR_irp: { MCC_TRACE("br\n");
		int sym, saved_flags = parse_flags, i, nvalues = 0;
		TokenString *body;
		TokenString *values[256];
		next();
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); expect("symbol name"); }
		sym = tok;
		next();
		if (tok == ',')
			{ MCC_TRACE("br\n"); next(); }
		for (i = 0; i < 256; i++)
			values[i] = NULL;
		parse_flags |= PARSE_FLAG_ACCEPT_STRAYS;
		if (tok != TOK_LINEFEED && tok != TOK_EOF && tok != ';') { MCC_TRACE("br\n");
			values[0] = tok_str_alloc();
			for (;;) { MCC_TRACE("br\n");
				if (tok == TOK_LINEFEED || tok == TOK_EOF || tok == ';')
					{ MCC_TRACE("br\n"); break; }
				if (tok == ',') { MCC_TRACE("br\n");
					next();
					if (nvalues < 255)
						{ MCC_TRACE("br\n"); nvalues++; values[nvalues] = tok_str_alloc(); }
					continue;
				}
				tok_str_add_tok(values[nvalues]);
				next();
			}
			nvalues++;
		}
		for (i = 0; i < nvalues; i++)
			{ MCC_TRACE("br\n"); tok_str_add(values[i], TOK_EOF); }
		body = tok_str_alloc();
		while (next(), tok != TOK_ASMDIR_endr) { MCC_TRACE("br\n");
			if (tok == CH_EOF || tok == TOK_EOF) { MCC_TRACE("br\n");
				parse_flags = saved_flags;
				mcc_error("unexpected end of file in .irp (missing .endr)");
			}
			tok_str_add_tok(body);
		}
		tok_str_add(body, TOK_EOF);
		parse_flags = saved_flags;
		for (i = 0; i < nvalues; i++) { MCC_TRACE("br\n");
			TokenString *subst =
					asm_macro_subst(&sym, 1, &values[i], 1, body->str, sym);
			begin_macro(subst, 1);
			mcc_assemble_internal(s1, (parse_flags & PARSE_FLAG_PREPROCESS), global);
			end_macro();
		}
		tok_str_free(body);
		for (i = 0; i < nvalues; i++)
			if (values[i])
				{ MCC_TRACE("br\n"); tok_str_free(values[i]); }
		next();
		break;
	}
	case TOK_ASMDIR_byte:
		size = 1;
		goto asm_data;
	case TOK_ASMDIR_word:
#ifdef MCC_TARGET_ARM64
		size = 4;
		goto asm_data;
#endif
	case TOK_ASMDIR_hword:
	case TOK_ASMDIR_value:
	case TOK_ASMDIR_2byte:
	case TOK_ASMDIR_short:
		size = 2;
		goto asm_data;
	case TOK_ASMDIR_4byte:
	case TOK_ASMDIR_long:
	case TOK_ASMDIR_int:
		size = 4;
	asm_data:
		next();
		for (;;) { MCC_TRACE("br\n");
			ExprValue e;
			asm_expr(s1, &e);
			if (sec->sh_type != SHT_NOBITS) { MCC_TRACE("br\n");
				if (size == 4) { MCC_TRACE("br\n");
					gen_expr32(&e);
#if MCC_PTR_SIZE == 8
				} else if (size == 8) { MCC_TRACE("br\n");
					gen_expr64(&e);
#endif
				} else { MCC_TRACE("br\n");
					if (e.sym)
						{ MCC_TRACE("br\n"); expect("constant"); }
					if (size == 1)
						{ MCC_TRACE("br\n"); g(e.v); }
					else
						{ MCC_TRACE("br\n"); gen_le16(e.v); }
				}
			} else { MCC_TRACE("br\n");
				ind += size;
			}
			if (tok != ',')
				{ MCC_TRACE("br\n"); break; }
			next();
		}
		break;
	case TOK_ASMDIR_fill: {
		int repeat, size, val;
		uint8_t repeat_buf[8];
		next();
		repeat = asm_int_expr(s1);
		if (repeat < 0) { MCC_TRACE("br\n");
			mcc_error("repeat < 0; .fill ignored");
			break;
		}
		size = 1;
		val = 0;
		if (tok == ',') { MCC_TRACE("br\n");
			next();
			size = asm_int_expr(s1);
			if (size < 0) { MCC_TRACE("br\n");
				mcc_error("size < 0; .fill ignored");
				break;
			}
			if (size > 8)
				{ MCC_TRACE("br\n"); size = 8; }
			if (tok == ',') { MCC_TRACE("br\n");
				next();
				val = asm_int_expr(s1);
			}
		}
		repeat_buf[0] = val;
		repeat_buf[1] = val >> 8;
		repeat_buf[2] = val >> 16;
		repeat_buf[3] = val >> 24;
		repeat_buf[4] = 0;
		repeat_buf[5] = 0;
		repeat_buf[6] = 0;
		repeat_buf[7] = 0;
		for (int i = 0; i < repeat; i++) { MCC_TRACE("br\n");
			for (int j = 0; j < size; j++) { MCC_TRACE("br\n");
				g(repeat_buf[j]);
			}
		}
	} break;
	case TOK_ASMDIR_rept: {
		int repeat;
		TokenString *init_str;
		next();
		repeat = asm_int_expr(s1);
		init_str = tok_str_alloc();
		while (next(), tok != TOK_ASMDIR_endr) { MCC_TRACE("br\n");
			if (tok == CH_EOF)
				{ MCC_TRACE("br\n"); mcc_error("we at end of file, .endr not found"); }
			tok_str_add_tok(init_str);
		}
		tok_str_add(init_str, TOK_EOF);
		begin_macro(init_str, 1);
		while (repeat-- > 0) { MCC_TRACE("br\n");
			mcc_assemble_internal(s1, (parse_flags & PARSE_FLAG_PREPROCESS),
														global);
			macro_ptr = init_str->str;
		}
		end_macro();
		next();
		break;
	}
	case TOK_ASMDIR_org: {
		ExprValue e;
		ElfSym *esym;
		next();
		asm_expr(s1, &e);
		n = e.v;
		esym = elfsym(e.sym);
		if (esym) { MCC_TRACE("br\n");
			if (esym->st_shndx != cur_text_section->sh_num)
				{ MCC_TRACE("br\n"); expect("constant or same-section symbol"); }
			n += esym->st_value;
		}
		if (n < ind)
			{ MCC_TRACE("br\n"); mcc_error("attempt to .org backwards"); }
		v = c = 0;
		size = n - ind;
		goto zero_pad;
	} break;
	case TOK_ASMDIR_set:
	case TOK_ASMDIR_equ:
	case TOK_ASMDIR_equiv: {
		int is_equiv = (tok == TOK_ASMDIR_equiv);
		next();
		tok1 = tok;
		if (is_equiv) { MCC_TRACE("br\n");
			Sym *prev = asm_label_find(tok1);
			if (prev && prev->c && elfsym(prev)->st_shndx != SHN_UNDEF)
				{ MCC_TRACE("br\n"); mcc_error("symbol '%s' is already defined",
									get_tok_str(tok1, NULL)); }
		}
		next();
		if (tok == ',')
			{ MCC_TRACE("br\n"); set_symbol(s1, tok1); }
		break;
	}
	case TOK_ASMDIR_comm:
	case TOK_ASMDIR_lcomm: {
		Section *prev_sec;
		Sym *csym;
		int cname, calign = 1, csize, coffset, is_comm = (tok == TOK_ASMDIR_comm);
		next();
		cname = tok;
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); expect("identifier"); }
		next();
		if (tok != ',')
			{ MCC_TRACE("br\n"); expect("','"); }
		next();
		csize = asm_int_expr(s1);
		if (tok == ',') { MCC_TRACE("br\n");
			next();
			calign = asm_int_expr(s1);
		}
		if (calign < 1)
			{ MCC_TRACE("br\n"); calign = 1; }
		prev_sec = cur_text_section;
		use_section1(s1, find_section(s1, ".bss"));
		ind = (ind + calign - 1) & ~(calign - 1);
		if (cur_text_section->sh_addralign < calign)
			{ MCC_TRACE("br\n"); cur_text_section->sh_addralign = calign; }
		coffset = ind;
		csym = asm_new_label1(s1, cname, 0, cur_text_section->sh_num, coffset);
		if (is_comm) { MCC_TRACE("br\n");
			csym->type.t &= ~VT_STATIC;
			update_storage(csym);
		}
		elfsym(csym)->st_size = csize;
		ind += csize;
		use_section1(s1, prev_sec);
		break;
	}
	case TOK_ASMDIR_if:
		next();
		if (asm_int_expr(s1) == 0)
			{ MCC_TRACE("br\n"); asm_conditional_false(s1); }
		break;
	case TOK_ASMDIR_ifdef:
	case TOK_ASMDIR_ifndef: {
		Sym *dsym;
		ElfSym *desym;
		int want = (tok == TOK_ASMDIR_ifdef), defined;
		next();
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); expect("identifier"); }
		dsym = asm_label_find(tok);
		desym = (dsym && dsym->c) ? elfsym(dsym) : NULL;
		defined = (desym && desym->st_shndx != SHN_UNDEF);
		next();
		if (defined != want)
			{ MCC_TRACE("br\n"); asm_conditional_false(s1); }
		break;
	}
	case TOK_ASMDIR_ifeq:
	case TOK_ASMDIR_ifne:
	case TOK_ASMDIR_ifgt:
	case TOK_ASMDIR_iflt:
	case TOK_ASMDIR_ifge:
	case TOK_ASMDIR_ifle: {
		int which = tok, e, take;
		next();
		e = asm_int_expr(s1);
		if (which == TOK_ASMDIR_ifeq)
			{ MCC_TRACE("br\n"); take = (e == 0); }
		else if (which == TOK_ASMDIR_ifne)
			{ MCC_TRACE("br\n"); take = (e != 0); }
		else if (which == TOK_ASMDIR_ifgt)
			{ MCC_TRACE("br\n"); take = (e > 0); }
		else if (which == TOK_ASMDIR_iflt)
			{ MCC_TRACE("br\n"); take = (e < 0); }
		else if (which == TOK_ASMDIR_ifge)
			{ MCC_TRACE("br\n"); take = (e >= 0); }
		else
			{ MCC_TRACE("br\n"); take = (e <= 0); }
		if (!take)
			{ MCC_TRACE("br\n"); asm_conditional_false(s1); }
		break;
	}
	case TOK_ASMDIR_ifb:
	case TOK_ASMDIR_ifnb: {
		int want_blank = (tok == TOK_ASMDIR_ifb), is_blank;
		next();
		is_blank = (tok == TOK_LINEFEED || tok == ';' ||
								tok == TOK_EOF || tok == CH_EOF);
		asm_skip_to_eol();
		if (is_blank != want_blank)
			{ MCC_TRACE("br\n"); asm_conditional_false(s1); }
		break;
	}
	case TOK_ASMDIR_ifc:
	case TOK_ASMDIR_ifnc: {
		int want_eq = (tok == TOK_ASMDIR_ifc), eq;
		char a1[256], a2[256];
		a1[0] = a2[0] = 0;
		next();
		while (tok != ',' && tok != TOK_LINEFEED && tok != ';' &&
					 tok != TOK_EOF && tok != CH_EOF) { MCC_TRACE("br\n");
			pstrcat(a1, sizeof a1, get_tok_str(tok, &tokc));
			next();
		}
		if (tok == ',')
			{ MCC_TRACE("br\n"); next(); }
		while (tok != TOK_LINEFEED && tok != ';' &&
					 tok != TOK_EOF && tok != CH_EOF) { MCC_TRACE("br\n");
			pstrcat(a2, sizeof a2, get_tok_str(tok, &tokc));
			next();
		}
		eq = (strcmp(a1, a2) == 0);
		if (eq != want_eq)
			{ MCC_TRACE("br\n"); asm_conditional_false(s1); }
		break;
	}
	case TOK_ASMDIR_elseif:
		asm_skip_conditional(s1, 0);
		break;
	case TOK_ASMDIR_else:
		asm_skip_conditional(s1, 0);
		break;
	case TOK_ASMDIR_endif:
		asm_skip_to_eol();
		break;
	case TOK_ASMDIR_globl:
	case TOK_ASMDIR_global:
	case TOK_ASMDIR_weak:
	case TOK_ASMDIR_hidden:
		tok1 = tok;
		do { MCC_TRACE("br\n");
			Sym *sym;
			next();
			if (tok < TOK_IDENT)
				{ MCC_TRACE("br\n"); expect("identifier"); }
			sym = get_asm_sym(tok, NULL);
			if (tok1 != TOK_ASMDIR_hidden)
				{ MCC_TRACE("br\n"); sym->type.t &= ~VT_STATIC; }
			if (tok1 == TOK_ASMDIR_weak)
				{ MCC_TRACE("br\n"); sym->a.weak = 1; }
			else if (tok1 == TOK_ASMDIR_hidden)
				{ MCC_TRACE("br\n"); sym->a.visibility = STV_HIDDEN; }
			update_storage(sym);
			next();
		} while (tok == ',');
		break;
	case TOK_ASMDIR_string:
	case TOK_ASMDIR_ascii:
	case TOK_ASMDIR_asciz: {
		const char *p;
		int size, t;

		t = tok;
		next();
		for (;;) { MCC_TRACE("br\n");
			if (tok != TOK_STR)
				{ MCC_TRACE("br\n"); expect("string constant"); }
			p = tokc.str.data;
			size = tokc.str.size;
			if (t == TOK_ASMDIR_ascii && size > 0)
				{ MCC_TRACE("br\n"); size--; }
			for (int i = 0; i < size; i++)
				{ MCC_TRACE("br\n"); g(p[i]); }
			next();
			if (tok == ',') { MCC_TRACE("br\n");
				next();
			} else if (tok != TOK_STR) { MCC_TRACE("br\n");
				break;
			}
		}
	} break;
	case TOK_ASMDIR_text:
	case TOK_ASMDIR_data:
	case TOK_ASMDIR_bss: {
		char sname[64];
		tok1 = tok;
		n = 0;
		next();
		if (tok != ';' && tok != TOK_LINEFEED) { MCC_TRACE("br\n");
			n = asm_int_expr(s1);
			next();
		}
		if (n)
			{ MCC_TRACE("br\n"); snprintf(sname, sizeof(sname), "%s%d", get_tok_str(tok1, NULL), n); }
		else
			{ MCC_TRACE("br\n"); snprintf(sname, sizeof(sname), "%s", get_tok_str(tok1, NULL)); }
		use_section(s1, sname);
	} break;
	case TOK_ASMDIR_file: {
		const char *p;
		parse_flags &= ~PARSE_FLAG_TOK_STR;
		next();
		if (tok == TOK_PPNUM)
			{ MCC_TRACE("br\n"); next(); }
		if (tok == TOK_PPSTR && tokc.str.data[0] == '"') { MCC_TRACE("br\n");
			tokc.str.data[tokc.str.size - 2] = 0;
			p = tokc.str.data + 1;
		} else if (tok >= TOK_IDENT) { MCC_TRACE("br\n");
			p = get_tok_str(tok, &tokc);
		} else { MCC_TRACE("br\n");
			skip_to_eol(0);
			parse_flags |= PARSE_FLAG_TOK_STR;
			break;
		}
		mccpp_putfile(p);

		parse_flags |= PARSE_FLAG_TOK_STR;
		next();
	} break;
	case TOK_ASMDIR_ident: {
		char ident[256];

		ident[0] = '\0';
		next();
		if (tok == TOK_STR)
			{ MCC_TRACE("br\n"); pstrcat(ident, sizeof(ident), tokc.str.data); }
		else
			{ MCC_TRACE("br\n"); pstrcat(ident, sizeof(ident), get_tok_str(tok, &tokc)); }
		mcc_warning_c(warn_unsupported)("ignoring .ident %s", ident);
		next();
	} break;
	case TOK_ASMDIR_size: {
		Sym *sym;
		ElfSym *esym;

		next();
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); expect("identifier"); }
		sym = asm_label_find(tok);
		if (!sym)
			{ MCC_TRACE("br\n"); mcc_error("label not found: %s", get_tok_str(tok, NULL)); }
		mcc_warning_c(warn_unsupported)("ignoring .size %s,*", get_tok_str(tok, NULL));
		next();
		skip(',');
		n = asm_int_expr(s1);
		esym = elfsym(sym);
		if (esym) { MCC_TRACE("br\n");
			esym->st_size = n;
		}
	} break;
	case TOK_ASMDIR_type: {
		Sym *sym;
		const char *newtype;
		int st_type;

		next();
		if (tok < TOK_IDENT)
			{ MCC_TRACE("br\n"); expect("identifier"); }
		sym = get_asm_sym(tok, NULL);
		next();
		skip(',');
		if (tok == TOK_STR) { MCC_TRACE("br\n");
			newtype = tokc.str.data;
		} else { MCC_TRACE("br\n");
			if (tok == '@' || tok == '%')
				{ MCC_TRACE("br\n"); next(); }
			newtype = get_tok_str(tok, NULL);
		}

		if (!strcmp(newtype, "function") || !strcmp(newtype, "STT_FUNC")) { MCC_TRACE("br\n");
			if (IS_ASM_SYM(sym))
				{ MCC_TRACE("br\n"); sym->type.t |= VT_ASM_FUNC; }
			st_type = STT_FUNC;
		set_st_type:
			if (sym->c) { MCC_TRACE("br\n");
				ElfSym *esym = elfsym(sym);
				esym->st_info = ELFW(ST_INFO)(ELFW(ST_BIND)(esym->st_info), st_type);
			}
		} else if (!strcmp(newtype, "object") || !strcmp(newtype, "STT_OBJECT")) { MCC_TRACE("br\n");
			st_type = STT_OBJECT;
			goto set_st_type;
		} else
			{ MCC_TRACE("br\n"); mcc_warning_c(warn_unsupported)("change type of '%s' from 0x%x to '%s' ignored",
																			get_tok_str(sym->v, NULL), sym->type.t, newtype); }

		next();
	} break;
	case TOK_ASMDIR_pushsection:
	case TOK_ASMDIR_section: {
		char sname[256];
		int old_nb_section = s1->nb_sections;
		int flags = SHF_ALLOC;

		tok1 = tok;
		next();
		sname[0] = '\0';
		while (tok != ';' && tok != TOK_LINEFEED && tok != ',') { MCC_TRACE("br\n");
			if (tok == TOK_STR)
				{ MCC_TRACE("br\n"); pstrcat(sname, sizeof(sname), tokc.str.data); }
			else
				{ MCC_TRACE("br\n"); pstrcat(sname, sizeof(sname), get_tok_str(tok, NULL)); }
			next();
		}
		if (tok == ',') { MCC_TRACE("br\n");
			const char *p;
			next();
			if (tok != TOK_STR)
				{ MCC_TRACE("br\n"); expect("string constant"); }
			for (p = tokc.str.data; *p; ++p) { MCC_TRACE("br\n");
				if (*p == 'w')
					{ MCC_TRACE("br\n"); flags |= SHF_WRITE; }
				if (*p == 'x')
					{ MCC_TRACE("br\n"); flags |= SHF_EXECINSTR; }
			}
			next();
			if (tok == ',') { MCC_TRACE("br\n");
				next();
				if (tok == '@' || tok == '%')
					{ MCC_TRACE("br\n"); next(); }
				next();
			}
		}
		last_text_section = cur_text_section;
		if (tok1 == TOK_ASMDIR_section)
			{ MCC_TRACE("br\n"); use_section(s1, sname); }
		else
			{ MCC_TRACE("br\n"); push_section(s1, sname); }
		if (old_nb_section != s1->nb_sections) { MCC_TRACE("br\n");
			cur_text_section->sh_addralign = 1;
			if (!strcmp(sname, ".init") || !strcmp(sname, ".fini"))
				{ MCC_TRACE("br\n"); flags |= SHF_EXECINSTR; }
			cur_text_section->sh_flags = flags;
		}
	} break;
	case TOK_ASMDIR_previous: {
		Section *sec;
		next();
		if (!last_text_section)
			{ MCC_TRACE("br\n"); mcc_error("no previous section referenced"); }
		sec = cur_text_section;
		use_section1(s1, last_text_section);
		last_text_section = sec;
	} break;
	case TOK_ASMDIR_popsection:
		next();
		pop_section(s1);
		break;
#ifdef MCC_TARGET_I386
	case TOK_ASMDIR_code16: {
		next();
		s1->seg_size = 16;
	} break;
	case TOK_ASMDIR_code32: {
		next();
		s1->seg_size = 32;
	} break;
#endif
#if MCC_PTR_SIZE == 8
	case TOK_ASMDIR_code64:
		next();
		break;
#endif
#ifdef MCC_TARGET_RISCV64
	case TOK_ASMDIR_option:
		next();
		switch (tok) { MCC_TRACE("br\n");
		case TOK_ASM_rvc:
		case TOK_ASM_norvc:
		case TOK_ASM_pic:
		case TOK_ASM_nopic:
		case TOK_ASM_relax:
		case TOK_ASM_norelax:
		case TOK_ASM_push:
		case TOK_ASM_pop:
			next();
			break;
		case TOK_ASM_arch:
			mcc_error("unimp .option '.%s'", get_tok_str(tok, NULL));
			break;
		default:
			mcc_error("unknown .option '.%s'", get_tok_str(tok, NULL));
			break;
		}
		break;
#endif
	case TOK_ASMDIR_symver:
		next();
		next();
		skip(',');
		next();
		skip('@');
		next();
		break;
	case TOK_ASMDIR_reloc: {
		ExprValue e;
		const char *reloc_name;
		int reloc_type = -1;

		next();
		asm_expr(s1, &e);
		skip(',');
		reloc_name = get_tok_str(tok, NULL);
#if defined(MCC_TARGET_ARM64)
		if (!strcmp(reloc_name, "R_AARCH64_CALL26"))
			{ MCC_TRACE("br\n"); reloc_type = R_AARCH64_CALL26; }
#elif defined(MCC_TARGET_RISCV64)
		if (!strcmp(reloc_name, "R_RISCV_CALL") || !strcmp(reloc_name, "R_RISCV_CALL_PLT"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_CALL; }
		else if (!strcmp(reloc_name, "R_RISCV_BRANCH"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_BRANCH; }
		else if (!strcmp(reloc_name, "R_RISCV_JAL"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_JAL; }
		else if (!strcmp(reloc_name, "R_RISCV_PCREL_HI20"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_PCREL_HI20; }
		else if (!strcmp(reloc_name, "R_RISCV_PCREL_LO12_I"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_PCREL_LO12_I; }
		else if (!strcmp(reloc_name, "R_RISCV_PCREL_LO12_S"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_PCREL_LO12_S; }
		else if (!strcmp(reloc_name, "R_RISCV_32_PCREL"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_32_PCREL; }
		else if (!strcmp(reloc_name, "R_RISCV_32"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_32; }
		else if (!strcmp(reloc_name, "R_RISCV_64"))
			{ MCC_TRACE("br\n"); reloc_type = R_RISCV_64; }
#endif
		if (reloc_type < 0)
			{ MCC_TRACE("br\n"); mcc_error("unimp: reloc '%s' unknown", reloc_name); }
		next();
		skip(',');
		greloca(cur_text_section, get_asm_sym(tok, NULL), e.v, reloc_type, 0);
		next();
	} break;
	default:
		mcc_error("unknown assembler directive '.%s'", get_tok_str(tok, NULL));
		break;
	}
}

#if MCC_EH_FRAME

#define asm_cfi (mcc_state->asm_cfi_st)

static void asm_cfi_factors(MCCState *s1) { MCC_TRACE("enter\n");
	unsigned char *p, *end;

	if (asm_cfi.have_factors)
		{ MCC_TRACE("br\n"); return; }
	asm_cfi.code_align = 1;
	asm_cfi.data_align = -MCC_PTR_SIZE;
	p = eh_frame_section->data + s1->eh_start;
	end = eh_frame_section->data + eh_frame_section->data_offset;
	p += 8;
	p += 1;
	while (p < end && *p)
		{ MCC_TRACE("br\n"); p++; }
	p++;
	asm_cfi.code_align = dwarf_read_uleb128(&p, end);
	asm_cfi.data_align = dwarf_read_sleb128(&p, end);
	if (!asm_cfi.code_align)
		{ MCC_TRACE("br\n"); asm_cfi.code_align = 1; }
	if (!asm_cfi.data_align)
		{ MCC_TRACE("br\n"); asm_cfi.data_align = -MCC_PTR_SIZE; }
	asm_cfi.have_factors = 1;
}

static void asm_cfi_room(int n) { MCC_TRACE("enter\n");
	if (asm_cfi.len + n > asm_cfi.cap) { MCC_TRACE("br\n");
		int newcap = asm_cfi.cap ? asm_cfi.cap * 2 : 1024;
		while (asm_cfi.len + n > newcap)
			{ MCC_TRACE("br\n"); newcap *= 2; }
		asm_cfi.buf = mcc_realloc(asm_cfi.buf, newcap);
		asm_cfi.cap = newcap;
	}
}

static void asm_cfi_advance(MCCState *s1) { MCC_TRACE("enter\n");
	unsigned long pc, delta;

	if (cur_text_section != asm_cfi.sec)
		{ MCC_TRACE("br\n"); mcc_error(".cfi directive outside its function's section"); }
	pc = ind;
	delta = pc - asm_cfi.last;
	if (asm_cfi.code_align > 1) { MCC_TRACE("br\n");
		if (delta % asm_cfi.code_align)
			{ MCC_TRACE("br\n"); mcc_error(".cfi advance not a multiple of the code alignment factor"); }
		delta /= asm_cfi.code_align;
	}
	asm_cfi_room(5);
	asm_cfi.len += mcc_cfi_advance(asm_cfi.buf + asm_cfi.len, delta);
	asm_cfi.last = pc;
}

static void asm_parse_cfi_directive(MCCState *s1) { MCC_TRACE("enter\n");
	char dir[32];
	long long a, b;

	pstrcpy(dir, sizeof(dir), get_tok_str(tok, NULL) + 5);
	next();
	if (!eh_frame_section) { MCC_TRACE("br\n");
		while (tok != ';' && tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next(); }
		return;
	}
	if (!strcmp(dir, "startproc")) { MCC_TRACE("br\n");
		if (tok >= TOK_IDENT)
			{ MCC_TRACE("br\n"); next(); }
		if (asm_cfi.active)
			{ MCC_TRACE("br\n"); mcc_error("previous .cfi_startproc not closed"); }
		asm_cfi.active = 1;
		asm_cfi.sec = cur_text_section;
		asm_cfi.start = asm_cfi.last = ind;
		asm_cfi.len = 0;
		asm_cfi_factors(s1);
		return;
	}
	if (!asm_cfi.active)
		{ MCC_TRACE("br\n"); mcc_error(".cfi_%s without .cfi_startproc", dir); }
	if (!strcmp(dir, "endproc")) { MCC_TRACE("br\n");
		if (cur_text_section != asm_cfi.sec)
			{ MCC_TRACE("br\n"); mcc_error(".cfi_endproc outside its function's section"); }
		mcc_eh_frame_fde(s1, asm_cfi.sec, asm_cfi.start, ind - asm_cfi.start,
										 asm_cfi.buf, asm_cfi.len);
		asm_cfi.nfde++;
		asm_cfi.active = 0;
		return;
	}
	if (!strcmp(dir, "def_cfa")) { MCC_TRACE("br\n");
		a = asm_int_expr(s1);
		skip(',');
		b = asm_int_expr(s1);
		if (a < 0 || b < 0)
			{ MCC_TRACE("br\n"); mcc_error("unsupported .cfi_def_cfa operands"); }
		asm_cfi_advance(s1);
		asm_cfi_room(1 + 2 * DWARF_MAX_128);
		asm_cfi.buf[asm_cfi.len++] = DW_CFA_def_cfa;
		asm_cfi.len += mcc_cfi_uleb(asm_cfi.buf + asm_cfi.len, a);
		asm_cfi.len += mcc_cfi_uleb(asm_cfi.buf + asm_cfi.len, b);
	} else if (!strcmp(dir, "def_cfa_offset")) { MCC_TRACE("br\n");
		a = asm_int_expr(s1);
		if (a < 0)
			{ MCC_TRACE("br\n"); mcc_error("unsupported negative .cfi_def_cfa_offset"); }
		asm_cfi_advance(s1);
		asm_cfi_room(1 + DWARF_MAX_128);
		asm_cfi.buf[asm_cfi.len++] = DW_CFA_def_cfa_offset;
		asm_cfi.len += mcc_cfi_uleb(asm_cfi.buf + asm_cfi.len, a);
	} else if (!strcmp(dir, "def_cfa_register")) { MCC_TRACE("br\n");
		a = asm_int_expr(s1);
		if (a < 0)
			{ MCC_TRACE("br\n"); mcc_error("bad .cfi_def_cfa_register operand"); }
		asm_cfi_advance(s1);
		asm_cfi_room(1 + DWARF_MAX_128);
		asm_cfi.buf[asm_cfi.len++] = DW_CFA_def_cfa_register;
		asm_cfi.len += mcc_cfi_uleb(asm_cfi.buf + asm_cfi.len, a);
	} else if (!strcmp(dir, "offset")) { MCC_TRACE("br\n");
		a = asm_int_expr(s1);
		skip(',');
		b = asm_int_expr(s1);
		if (a < 0 || b % asm_cfi.data_align || b / asm_cfi.data_align < 0)
			{ MCC_TRACE("br\n"); mcc_error("unsupported .cfi_offset operands"); }
		b /= asm_cfi.data_align;
		asm_cfi_advance(s1);
		asm_cfi_room(1 + 2 * DWARF_MAX_128);
		if (a < 0x40) { MCC_TRACE("br\n");
			asm_cfi.buf[asm_cfi.len++] = DW_CFA_offset + a;
		} else { MCC_TRACE("br\n");
			asm_cfi.buf[asm_cfi.len++] = DW_CFA_offset_extended;
			asm_cfi.len += mcc_cfi_uleb(asm_cfi.buf + asm_cfi.len, a);
		}
		asm_cfi.len += mcc_cfi_uleb(asm_cfi.buf + asm_cfi.len, b);
	} else if (!strcmp(dir, "restore")) { MCC_TRACE("br\n");
		a = asm_int_expr(s1);
		if (a < 0)
			{ MCC_TRACE("br\n"); mcc_error("bad .cfi_restore operand"); }
		asm_cfi_advance(s1);
		asm_cfi_room(1 + DWARF_MAX_128);
		if (a < 0x40) { MCC_TRACE("br\n");
			asm_cfi.buf[asm_cfi.len++] = DW_CFA_restore + a;
		} else { MCC_TRACE("br\n");
			asm_cfi.buf[asm_cfi.len++] = DW_CFA_restore_extended;
			asm_cfi.len += mcc_cfi_uleb(asm_cfi.buf + asm_cfi.len, a);
		}
	} else { MCC_TRACE("br\n");
		mcc_warning_c(warn_unsupported)("ignoring .cfi_%s", dir);
		while (tok != ';' && tok != TOK_LINEFEED && tok != TOK_EOF)
			{ MCC_TRACE("br\n"); next(); }
	}
}
#endif

static int mcc_assemble_internal(MCCState *s1, int do_preprocess, int global) { MCC_TRACE("enter\n");
	int opcode;
	int saved_parse_flags = parse_flags;

	parse_flags = PARSE_FLAG_ASM_FILE | PARSE_FLAG_TOK_STR;
	if (do_preprocess)
		{ MCC_TRACE("br\n"); parse_flags |= PARSE_FLAG_PREPROCESS; }
	for (;;) { MCC_TRACE("br\n");
		next();
		if (tok == TOK_EOF)
			{ MCC_TRACE("br\n"); break; }
		mcc_debug_line(s1);
		parse_flags |= PARSE_FLAG_LINEFEED;
	redo:
#if !defined(MCC_TARGET_ARM64)
		if (tok == '#') { MCC_TRACE("br\n");
			while (tok != TOK_LINEFEED)
				{ MCC_TRACE("br\n"); next(); }
		} else
#endif
				if (tok >= TOK_ASMDIR_FIRST && tok <= TOK_ASMDIR_LAST) { MCC_TRACE("br\n");
			asm_parse_directive(s1, global);
		} else if (tok == TOK_PPNUM) { MCC_TRACE("br\n");
			const char *p;
			int n, dd;
			p = tokc.str.data;
			dd = 0;
			if (p[0] == '.') { MCC_TRACE("br\n");
				if (!strcmp(p, ".2byte"))
					dd = TOK_ASMDIR_2byte;
				else if (!strcmp(p, ".4byte"))
					dd = TOK_ASMDIR_4byte;
				else if (!strcmp(p, ".8byte"))
					dd = TOK_ASMDIR_8byte;
			}
			if (dd) { MCC_TRACE("br\n");
				tok = dd;
				asm_parse_directive(s1, global);
			} else { MCC_TRACE("br\n");
				n = strtoul(p, (char **)&p, 10);
				if (*p != '\0')
					{ MCC_TRACE("br\n"); expect("':'"); }
				asm_new_label(s1, asm_get_local_label_name(s1, n), 1);
				next();
				skip(':');
				goto redo;
			}
		} else if (tok >= TOK_IDENT) { MCC_TRACE("br\n");
			const char *nm = get_tok_str(tok, NULL);
			int dc = 0;
#if MCC_EH_FRAME
			if (!strncmp(nm, ".cfi_", 5)) { MCC_TRACE("br\n");
				asm_parse_cfi_directive(s1);
				goto cfi_done;
			}
#else
			if (!strncmp(nm, ".cfi_", 5)) { MCC_TRACE("br\n");
				asm_skip_to_eol();
				goto cfi_done;
			}
#endif
			if (!strcmp(nm, ".dc.b"))
				dc = TOK_ASMDIR_byte;
			else if (!strcmp(nm, ".dc.w") || !strcmp(nm, ".dc"))
				dc = TOK_ASMDIR_2byte;
			else if (!strcmp(nm, ".dc.l"))
				dc = TOK_ASMDIR_4byte;
			if (dc) { MCC_TRACE("br\n");
				tok = dc;
				asm_parse_directive(s1, global);
			} else { MCC_TRACE("br\n");
				opcode = tok;
				next();
				if (tok == ':') { MCC_TRACE("br\n");
					asm_new_label(s1, opcode, 0);
					next();
					goto redo;
				} else if (tok == '=') { MCC_TRACE("br\n");
					set_symbol(s1, opcode);
					goto redo;
				} else { MCC_TRACE("br\n");
					AsmMacro *mac = asm_macro_find(opcode);
					if (mac) { MCC_TRACE("br\n");
						asm_macro_expand(s1, mac, global);
					} else { MCC_TRACE("br\n");
						if (get_tok_str(opcode, NULL)[0] == '.')
							{ MCC_TRACE("br\n"); mcc_error("unknown directive"); }
						asm_opcode(s1, opcode);
					}
				}
			}
		}
	cfi_done:
		if (tok != ';' && tok != TOK_LINEFEED)
			{ MCC_TRACE("br\n"); expect("end of line"); }
		parse_flags &= ~PARSE_FLAG_LINEFEED;
	}

	parse_flags = saved_parse_flags;
	return 0;
}

ST_FUNC int mcc_assemble(MCCState *s1, int do_preprocess) { MCC_TRACE("enter\n");
	int ret, i;
	mcc_debug_start(s1);
	for (i = 1; i < s1->nb_sections; i++) { MCC_TRACE("br\n");
		s1->sections[i]->asm_base = -1;
	}
	asm_standalone = 1;
	cur_text_section = text_section;
	ind = cur_text_section->data_offset;
	if (cur_text_section->asm_base < 0)
		{ MCC_TRACE("br\n"); cur_text_section->asm_base = ind; }
	nocode_wanted = 0;
#if MCC_EH_FRAME
	asm_cfi.active = 0;
	asm_cfi.nfde = 0;
#endif
	asm_macros_free();
	ret = mcc_assemble_internal(s1, do_preprocess, 1);
	asm_macros_free();
	cur_text_section->data_offset = ind;
	asm_standalone = 0;
#if MCC_EH_FRAME
	if (asm_cfi.active)
		{ MCC_TRACE("br\n"); mcc_error_noabort("open .cfi_startproc at end of file"); }

	if (asm_cfi.nfde)
		{ MCC_TRACE("br\n"); mcc_eh_frame_end(s1); }
#endif
	mcc_debug_end(s1);
	return ret;
}

static struct AsmInlineFrame {
	int depth;
	BufferedFile *file;
	Section *section;
	int ind;
	int tok_flags;
	int parse_flags;
	const int *macro_ptr;
	int dotid;
	int dolid;
} asm_inline_frame;

ST_FUNC void mcc_assemble_inline(MCCState *s1, const char *str, int len, int global) { MCC_TRACE("enter\n");
	const int *saved_macro_ptr = macro_ptr;
	int dotid = set_idnum('.', IS_ID);
	int dolid = 0;
#if !defined(MCC_TARGET_RISCV64) && !defined(MCC_TARGET_X86_64)
	dolid = set_idnum('$', 0);
#endif
	if (asm_inline_frame.depth++ == 0) { MCC_TRACE("br\n");
		asm_inline_frame.file = file;
		asm_inline_frame.section = cur_text_section;
		asm_inline_frame.ind = ind;
		asm_inline_frame.tok_flags = tok_flags;
		asm_inline_frame.parse_flags = parse_flags;
		asm_inline_frame.macro_ptr = saved_macro_ptr;
		asm_inline_frame.dotid = dotid;
		asm_inline_frame.dolid = dolid;
	}

	mcc_open_bf(s1, ":asm:", len);
	memcpy(file->buffer, str, len);
	macro_ptr = NULL;
	mcc_assemble_internal(s1, 0, global);
	mcc_close();

#if !defined(MCC_TARGET_RISCV64) && !defined(MCC_TARGET_X86_64)
	set_idnum('$', dolid);
#endif
	set_idnum('.', dotid);
	macro_ptr = saved_macro_ptr;
	asm_inline_frame.depth--;
}

ST_FUNC void mcc_asm_inline_unwind(void) { MCC_TRACE("enter\n");
	if (asm_inline_frame.depth <= 0)
		{ MCC_TRACE("br\n"); return; }
	while (file && file != asm_inline_frame.file)
		{ MCC_TRACE("br\n"); mcc_close(); }
	if (asm_inline_frame.section) { MCC_TRACE("br\n");
		cur_text_section = asm_inline_frame.section;
		ind = asm_inline_frame.ind;
	}
	tok_flags = asm_inline_frame.tok_flags;
	parse_flags = asm_inline_frame.parse_flags;
	macro_ptr = asm_inline_frame.macro_ptr;
#if !defined(MCC_TARGET_RISCV64) && !defined(MCC_TARGET_X86_64)
	set_idnum('$', asm_inline_frame.dolid);
#endif
	set_idnum('.', asm_inline_frame.dotid);
	asm_inline_frame.depth = 0;
}

ST_FUNC const char *skip_constraint_modifiers(const char *p) { MCC_TRACE("enter\n");
	while (*p == '=' || *p == '&' || *p == '+' || *p == '%')
		{ MCC_TRACE("br\n"); p++; }
	return p;
}

ST_FUNC int find_constraint(ASMOperand *operands, int nb_operands,
														const char *name, const char **pp) { MCC_TRACE("enter\n");
	int index;
	TokenSym *ts;
	const char *p;

	if (isnum(*name)) { MCC_TRACE("br\n");
		index = 0;
		while (isnum(*name)) { MCC_TRACE("br\n");
			index = (index * 10) + (*name) - '0';
			name++;
		}
		if ((unsigned)index >= nb_operands) { MCC_TRACE("br\n");
			int k = index - nb_operands;
			int i;
			index = -1;
			for (i = 0; i < nb_operands; i++) { MCC_TRACE("br\n");
				const char *q;
				int rw = 0;
				for (q = operands[i].constraint;
						 *q == '=' || *q == '&' || *q == '+' || *q == '%'; q++) { MCC_TRACE("br\n");
					if (*q == '+')
						{ MCC_TRACE("br\n"); rw = 1; }
				}
				if (!rw)
					{ MCC_TRACE("br\n"); continue; }
				if (k-- == 0)
					{ MCC_TRACE("br\n"); index = i; break; }
			}
		}
	} else if (*name == '[') { MCC_TRACE("br\n");
		name++;
		p = strchr(name, ']');
		if (p) { MCC_TRACE("br\n");
			ts = tok_alloc(name, p - name);
			for (index = 0; index < nb_operands; index++) { MCC_TRACE("br\n");
				if (operands[index].id == ts->tok)
					{ MCC_TRACE("br\n"); goto found; }
			}
			index = -1;
		found:
			name = p + 1;
		} else { MCC_TRACE("br\n");
			index = -1;
		}
	} else { MCC_TRACE("br\n");
		index = -1;
	}
	if (pp)
		{ MCC_TRACE("br\n"); *pp = name; }
	return index;
}

#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
static int asm_x87_constraint(ASMOperand *op) { MCC_TRACE("enter\n");
	const char *p = skip_constraint_modifiers(op->constraint);
	if (*p == 't' || *p == 'f')
		{ MCC_TRACE("br\n"); return *p; }
	return 0;
}

static int asm_x87_clobber_name(const char *s) { MCC_TRACE("enter\n");
	if (!strcmp(s, "st"))
		{ MCC_TRACE("br\n"); return 1; }
	if (s[0] == 's' && s[1] == 't' && s[2] == '(' &&
			s[3] >= '0' && s[3] <= '7' && s[4] == ')' && s[5] == '\0')
		{ MCC_TRACE("br\n"); return 1; }
	return 0;
}

static char asm_x87_suffix(ASMOperand *op) { MCC_TRACE("enter\n");
	int r;

	switch (op->vt->type.t & VT_BTYPE) { MCC_TRACE("br\n");
	case VT_FLOAT:
		r = 's';
		break;
	case VT_DOUBLE:
		r = 'l';
		break;
	case VT_LDOUBLE:
		r = 't';
		break;
	default:
		mcc_error("x87 asm operand must have floating type");
		return 0;
	}
	return (char)r;
}

static void asm_x87_check_mem(ASMOperand *op) { MCC_TRACE("enter\n");
	int r = op->vt->r;
	if (!(r & VT_LVAL) ||
			((r & VT_VALMASK) != VT_LOCAL &&
			 (r & (VT_VALMASK | VT_SYM)) != (VT_CONST | VT_SYM)))
		{ MCC_TRACE("br\n"); mcc_error("unsupported x87 asm operand"); }
}

static void asm_x87_wrap(ASMOperand *operands, int nb_operands,
												 int nb_outputs, int x87_clobber, CString *astr) { MCC_TRACE("enter\n");
	CString buf;
	ASMOperand *op, *inop;
	int found = -1;
	char sfx, insfx;

	for (int i = 0; i < nb_operands; i++) { MCC_TRACE("br\n");
		if (!asm_x87_constraint(&operands[i]))
			{ MCC_TRACE("br\n"); continue; }
		if (found >= 0)
			{ MCC_TRACE("br\n"); mcc_error("only one x87 asm operand is supported"); }
		found = i;
	}
	if (found < 0)
		{ MCC_TRACE("br\n"); return; }

	op = &operands[found];
	inop = op;
	if (op->input_index >= 0) { MCC_TRACE("br\n");
		if (found >= nb_outputs)
			{ MCC_TRACE("br\n"); mcc_error("x87 asm operand cannot be tied to another operand"); }
		inop = &operands[op->input_index];
	}
	sfx = asm_x87_suffix(op);
	insfx = asm_x87_suffix(inop);
	asm_x87_check_mem(op);
	if (inop != op)
		{ MCC_TRACE("br\n"); asm_x87_check_mem(inop); }

	cstr_new_s(&buf);
	if (found >= nb_outputs || op->is_rw || inop != op) { MCC_TRACE("br\n");
		cstr_printf(&buf, "fld%c ", insfx);
		subst_asm_operand(&buf, inop->vt, 0);
		cstr_ccat(&buf, '\n');
	}
	cstr_cat(&buf, astr->data, -1);
	if (found < nb_outputs) { MCC_TRACE("br\n");
		cstr_printf(&buf, "\nfstp%c ", sfx);
		subst_asm_operand(&buf, op->vt, 0);
	} else if (!x87_clobber) { MCC_TRACE("br\n");
		cstr_cat(&buf, "\nfstp %st(0)", -1);
	}
	cstr_ccat(&buf, '\0');
	cstr_reset(astr);
	cstr_cat(astr, buf.data, buf.size);
	cstr_free_s(&buf);
}

static const char *asm_flag_cc_names[] = {
	"a", "ae", "b", "be", "c", "e", "g", "ge", "l", "le",
	"na", "nae", "nb", "nbe", "nc", "ne", "ng", "nge", "nl", "nle",
	"no", "np", "ns", "nz", "o", "p", "pe", "po", "s", "z", NULL
};

static int asm_flag_cc_valid(const char *name) { MCC_TRACE("enter\n");
	for (int i = 0; asm_flag_cc_names[i]; i++) { MCC_TRACE("br\n");
		if (!strcmp(asm_flag_cc_names[i], name))
			{ MCC_TRACE("br\n"); return 1; }
	}
	return 0;
}

static int asm_flag_prepare(ASMOperand *operands, int nb_outputs,
														 int *flag_idx, char flag_cc[][8]) { MCC_TRACE("enter\n");
	ASMOperand *op;
	const char *cc;
	int n = 0;

	for (int i = 0; i < nb_outputs; i++) { MCC_TRACE("br\n");
		op = &operands[i];
		if (op->constraint[0] != '=' || op->constraint[1] != '@' ||
				op->constraint[2] != 'c' || op->constraint[3] != 'c')
			{ MCC_TRACE("br\n"); continue; }
		cc = op->constraint + 4;
		if (!asm_flag_cc_valid(cc))
			{ MCC_TRACE("br\n"); mcc_error("invalid asm flag output constraint '%s'", op->constraint); }
		pstrcpy(flag_cc[n], 8, cc);
		flag_idx[n] = i;
		n++;
		pstrcpy(op->constraint, sizeof op->constraint, "=r");
	}
	return n;
}

static void asm_flag_wrap(ASMOperand *operands, const int *flag_idx,
													 char flag_cc[][8], int nb_flag, CString *astr) { MCC_TRACE("enter\n");
	ASMOperand *op;
	int reg, width;
	char bytebuf[8], dwordbuf[8];
	const char *bytename, *dwordname;

	if (astr->size)
		{ MCC_TRACE("br\n"); astr->size--; }
	for (int k = 0; k < nb_flag; k++) { MCC_TRACE("br\n");
		op = &operands[flag_idx[k]];
		reg = op->reg;
		if (reg < 0)
			{ MCC_TRACE("br\n"); continue; }
		if (reg < 4) { MCC_TRACE("br\n");
			bytename = get_tok_str(TOK_ASM_al + reg, NULL);
			dwordname = get_tok_str(TOK_ASM_eax + reg, NULL);
		} else if (reg == 6) { MCC_TRACE("br\n");
			bytename = "sil";
			dwordname = get_tok_str(TOK_ASM_eax + reg, NULL);
		} else if (reg == 7) { MCC_TRACE("br\n");
			bytename = "dil";
			dwordname = get_tok_str(TOK_ASM_eax + reg, NULL);
		} else { MCC_TRACE("br\n");
			snprintf(bytebuf, sizeof bytebuf, "r%db", reg);
			snprintf(dwordbuf, sizeof dwordbuf, "r%dd", reg);
			bytename = bytebuf;
			dwordname = dwordbuf;
		}
		switch (op->vt->type.t & VT_BTYPE) { MCC_TRACE("br\n");
		case VT_BYTE:
		case VT_BOOL:
			width = 1;
			break;
		default:
			width = 4;
			break;
		}
		cstr_printf(astr, "\n\tset%s %%%s", flag_cc[k], bytename);
		if (width != 1)
			{ MCC_TRACE("br\n"); cstr_printf(astr, "\n\tmovzbl %%%s,%%%s", bytename, dwordname); }
	}
	cstr_ccat(astr, '\0');
}
#endif

static void subst_asm_operands(ASMOperand *operands, int nb_operands,
															 CString *out_str, const char *str) { MCC_TRACE("enter\n");
	int c, index, modifier;
	ASMOperand *op;
	SValue sv;
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
	int dialect = 0;
#endif

	for (;;) { MCC_TRACE("br\n");
		c = *str++;
		if (c == '%') { MCC_TRACE("br\n");
			if (*str == '%') { MCC_TRACE("br\n");
				str++;
				goto add_char;
			}
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
			if (*str == '{' || *str == '}' || *str == '|') { MCC_TRACE("br\n");
				c = *str++;
				goto add_char;
			}
			if (*str == 'v' && !isnum(str[1]) && str[1] != '[') { MCC_TRACE("br\n");
				str++;
				continue;
			}
#endif
			modifier = 0;
			if (*str == 'c' || *str == 'n' ||
					*str == 'b' || *str == 'w' || *str == 'h' || *str == 'k' ||
					*str == 'q' || *str == 'l' ||
#ifdef MCC_TARGET_ARM64
					*str == 'x' || *str == 's' || *str == 'd' || *str == 'Z' ||
#endif
#ifdef MCC_TARGET_RISCV64
					*str == 'z' ||
#endif
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
					*str == 'p' ||
#endif
					*str == 'P')
				modifier = *str++;
			index = find_constraint(operands, nb_operands, str, &str);
			if (index < 0)
			{ MCC_TRACE("br\n"); error:
				mcc_error("invalid operand reference after %%"); }
			op = &operands[index];
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
			if (asm_x87_constraint(op) ||
					(op->vt && op->ref_index >= 0 &&
					 asm_x87_constraint(&operands[op->ref_index]))) { MCC_TRACE("br\n");
				cstr_cat(out_str, "%st", -1);
				continue;
			}
#endif
			if (modifier == 'l') { MCC_TRACE("br\n");
				cstr_cat(out_str, get_tok_str(op->is_label, NULL), -1);
			} else { MCC_TRACE("br\n");
				if (op->vt == NULL)
					{ MCC_TRACE("br\n"); goto error; }
				sv = *op->vt;
				if (op->reg >= 0) { MCC_TRACE("br\n");
					sv.r = op->reg;
					if (op->is_memory)
						{ MCC_TRACE("br\n"); sv.r |= VT_LVAL; }
				}
				subst_asm_operand(out_str, &sv, modifier);
			}
		} else { MCC_TRACE("br\n");
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
			if (c == '{') { MCC_TRACE("br\n");
				dialect = 1;
				continue;
			}
			if (c == '|' && dialect) { MCC_TRACE("br\n");
				while (*str && *str != '}') { MCC_TRACE("br\n");
					if (*str == '%' && str[1])
						{ MCC_TRACE("br\n"); str++; }
					str++;
				}
				if (*str == '}')
					{ MCC_TRACE("br\n"); str++; }
				dialect = 0;
				continue;
			}
			if (c == '}' && dialect) { MCC_TRACE("br\n");
				dialect = 0;
				continue;
			}
#endif
		add_char:
			cstr_ccat(out_str, c);
			if (c == '\0')
				{ MCC_TRACE("br\n"); break; }
		}
	}
}

static void parse_asm_operands(ASMOperand *operands, int *nb_operands_ptr,
															 int is_output, uint64_t *gvmask) { MCC_TRACE("enter\n");
	ASMOperand *op;
	int nb_operands;
	char *astr;

	if (tok != ':') { MCC_TRACE("br\n");
		nb_operands = *nb_operands_ptr;
		for (;;) { MCC_TRACE("br\n");
			if (nb_operands >= MAX_ASM_OPERANDS)
				{ MCC_TRACE("br\n"); mcc_error("too many asm operands"); }
			op = &operands[nb_operands++];
			op->id = 0;
			op->is_label = 0;
			if (tok == '[') { MCC_TRACE("br\n");
				next();
				if (tok < TOK_IDENT)
					{ MCC_TRACE("br\n"); expect("identifier"); }
				op->id = tok;
				next();
				skip(']');
			}
			astr = parse_mult_str("string constant")->data;
			pstrcpy(op->constraint, sizeof op->constraint, astr);
			skip('(');
			asm_lvalue_cast++;
			gexpr();
			asm_lvalue_cast--;
			if (is_output) { MCC_TRACE("br\n");
				if (!(vtop->type.t & VT_ARRAY))
					{ MCC_TRACE("br\n"); test_lvalue(); }
			} else { MCC_TRACE("br\n");
				if ((vtop->r & VT_LVAL) &&
						((vtop->r & VT_VALMASK) == VT_LLOCAL ||
						 (vtop->r & VT_VALMASK) < VT_CONST) &&
						!strchr(op->constraint, 'm')
#ifdef MCC_TARGET_ARM64
						&& !strchr(op->constraint, 'Q') && !strstr(op->constraint, "Ump")
						&& !strchr(op->constraint, 'o')
#endif
				) { MCC_TRACE("br\n");
					gv(MCC_RC_INT);
					if (gvmask && nb_operands - 1 < 64)
						*gvmask |= (uint64_t)1 << (nb_operands - 1);
				}
			}
			op->vt = vtop;
			skip(')');
			if (tok == ',') { MCC_TRACE("br\n");
				next();
			} else { MCC_TRACE("br\n");
				break;
			}
		}
		*nb_operands_ptr = nb_operands;
	}
}

ST_FUNC void asm_instr(void) { MCC_TRACE("enter\n");
	CString astr, *astr1;

	ASMOperand operands[MAX_ASM_OPERANDS];
	int nb_outputs, nb_operands, must_subst, out_reg, nb_labels;
	int asm_eff = 0;
	uint64_t asm_gvmask;
	uint8_t clobber_regs[MCC_NB_ASM_REGS];
	Section *sec;
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
	int flag_idx[MAX_ASM_OPERANDS], nb_flag;
	char flag_cc[MAX_ASM_OPERANDS][8];
	int x87_clobber;
#endif

	while (tok == TOK_VOLATILE1 || tok == TOK_VOLATILE2 || tok == TOK_VOLATILE3 || tok == TOK_GOTO) { MCC_TRACE("br\n");
		if (tok == TOK_GOTO)
			{ MCC_TRACE("br\n"); asm_eff |= MCC_ASM_EFF_GOTO; }
		else
			{ MCC_TRACE("br\n"); asm_eff |= MCC_ASM_EFF_VOLATILE; }
		next();
	}

	astr1 = parse_asm_str();
	cstr_new_s(&astr);
	cstr_cat(&astr, astr1->data, astr1->size);

	nb_operands = 0;
	nb_outputs = 0;
	nb_labels = 0;
	asm_gvmask = 0;
	must_subst = 0;
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
	nb_flag = 0;
	x87_clobber = 0;
#endif
	memset(clobber_regs, 0, sizeof(clobber_regs));
	if (tok == ':') { MCC_TRACE("br\n");
		next();
		must_subst = 1;
		parse_asm_operands(operands, &nb_operands, 1, &asm_gvmask);
		nb_outputs = nb_operands;
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
		nb_flag = asm_flag_prepare(operands, nb_outputs, flag_idx, flag_cc);
#endif
		if (tok == ':') { MCC_TRACE("br\n");
			next();
			if (tok != ')') { MCC_TRACE("br\n");
				parse_asm_operands(operands, &nb_operands, 0, &asm_gvmask);
				if (tok == ':') { MCC_TRACE("br\n");
					next();
					for (;;) { MCC_TRACE("br\n");
						if (tok == ':' || tok == ')')
							{ MCC_TRACE("br\n"); break; }
						if (tok != TOK_STR)
							{ MCC_TRACE("br\n"); expect("string constant"); }
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
						if (asm_x87_clobber_name(tokc.str.data)) { MCC_TRACE("br\n");
							x87_clobber = 1;
							asm_eff |= MCC_ASM_EFF_X87;
						} else
#endif
							{ MCC_TRACE("br\n");
								const char *cname = (const char *)tokc.str.data;
								if (!strcmp(cname, "memory"))
									{ MCC_TRACE("br\n"); asm_eff |= MCC_ASM_EFF_MEM; }
								else if (!strcmp(cname, "cc") || !strcmp(cname, "flags"))
									{ MCC_TRACE("br\n"); asm_eff |= MCC_ASM_EFF_CC; }
								asm_clobber(clobber_regs, cname);
							}
						next();
						if (tok == ',') { MCC_TRACE("br\n");
							next();
						} else { MCC_TRACE("br\n");
							break;
						}
					}
				}
				if (tok == ':') { MCC_TRACE("br\n");
					next();
					for (;;) { MCC_TRACE("br\n");
						Sym *csym;
						int asmname;
						if (nb_operands + nb_labels >= MAX_ASM_OPERANDS)
							{ MCC_TRACE("br\n"); mcc_error("too many asm operands"); }
						if (tok < TOK_UIDENT)
							{ MCC_TRACE("br\n"); expect("label identifier"); }
						memset(operands + nb_operands + nb_labels, 0,
									 sizeof(operands[0]));
						operands[nb_operands + nb_labels++].id = tok;

						csym = label_find(tok);
						if (!csym) { MCC_TRACE("br\n");
							csym = label_push(&global_label_stack, tok,
																LABEL_FORWARD);
						} else { MCC_TRACE("br\n");
							if (csym->r == LABEL_DECLARED)
								{ MCC_TRACE("br\n"); csym->r = LABEL_FORWARD; }
						}
						next();
						asmname = asm_get_prefix_name(mcc_state, "LG.",
																					++asmgoto_n);
						if (!csym->c)
							{ MCC_TRACE("br\n"); put_extern_sym2(csym, SHN_UNDEF, 0, 0, 1); }
						get_asm_sym(asmname, csym);
						operands[nb_operands + nb_labels - 1].is_label = asmname;

						if (tok != ',')
							{ MCC_TRACE("br\n"); break; }
						next();
					}
				}
			}
		}
	}
	skip(')');
	if (tok != ';')
		{ MCC_TRACE("br\n"); expect("';'"); }

	if (nb_outputs == 0)
		{ MCC_TRACE("br\n"); asm_eff |= MCC_ASM_EFF_VOLATILE; }

	rir_hook_asm_operands(nb_operands, asm_gvmask);
	save_regs(0);

	asm_compute_constraints(operands, nb_operands, nb_outputs,
													clobber_regs, &out_reg);

	if (g_debug & MCC_DBG_ASM)
		{ MCC_TRACE("br\n"); printf("asm: \"%s\"\n", (char *)astr.data); }
	if (must_subst) { MCC_TRACE("br\n");
		cstr_reset(astr1);
		cstr_cat(astr1, astr.data, astr.size);
		cstr_reset(&astr);
		subst_asm_operands(operands, nb_operands + nb_labels, &astr, astr1->data);
	}
#if defined(MCC_TARGET_I386) || defined(MCC_TARGET_X86_64)
	asm_x87_wrap(operands, nb_operands, nb_outputs, x87_clobber, &astr);
	if (nb_flag) { MCC_TRACE("br\n");
		asm_flag_wrap(operands, flag_idx, flag_cc, nb_flag, &astr);
		asm_eff |= MCC_ASM_EFF_CCOUT | MCC_ASM_EFF_CC;
	}
#endif

	if (g_debug & MCC_DBG_ASM)
		{ MCC_TRACE("br\n"); printf("subst_asm: \"%s\"\n", (char *)astr.data); }

	ir_cap_asm_gen_code(operands, nb_operands, nb_outputs, nb_labels, asm_eff, 0,
											 clobber_regs, out_reg);

	sec = cur_text_section;
	ir_cap_asm(astr.data, astr.size - 1, 0);
	cstr_free_s(&astr);
	if (sec != cur_text_section) { MCC_TRACE("br\n");
		mcc_warning("inline asm tries to change current section");
		use_section1(mcc_state, sec);
	}

	next();

	ir_cap_asm_gen_code(operands, nb_operands, nb_outputs, nb_labels, asm_eff, 1,
											 clobber_regs, out_reg);

	for (int i = 0; i < nb_operands; i++) { MCC_TRACE("br\n");
		vpop();
	}
}

ST_FUNC void asm_global_instr(void) { MCC_TRACE("enter\n");
	CString *astr;
	int saved_nocode_wanted = nocode_wanted;

	nocode_wanted = 0;
	next();
	astr = parse_asm_str();
	skip(')');
	if (tok != ';')
		{ MCC_TRACE("br\n"); expect("';'"); }

	if (g_debug & MCC_DBG_ASM)
		{ MCC_TRACE("br\n"); printf("asm_global: \"%s\"\n", (char *)astr->data); }
	cur_text_section = text_section;
	ind = cur_text_section->data_offset;

	mcc_assemble_inline(mcc_state, astr->data, astr->size - 1, 1);

	cur_text_section->data_offset = ind;

	next();

	nocode_wanted = saved_nocode_wanted;
}

